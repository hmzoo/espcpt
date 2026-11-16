/* ============================================================
 *  Compte-tours moteur 220 V  —  NodeMCU v2 (ESP8266)
 * ============================================================
 *  Fonctions :
 *    - Compte les tours du moteur via un capteur optique FC-33
 *      (1 encoche = 1 impulsion = 1 tour).
 *    - Affiche les tours comptés, la consigne, la vitesse (tr/min)
 *      et l'état sur un écran OLED SSD1306 en I2C.
 *    - 4 boutons : "+", "−", "RAZ", "START/STOP".
 *    - Arrête automatiquement le moteur (relais) quand la
 *      consigne de tours est atteinte.
 *
 *  ⚠️  SÉCURITÉ : ce montage pilote du 220 V. Voir README.md.
 *
 *  Câblage : voir docs/wiring.md
 *  Réglages : voir include/config.h
 * ============================================================ */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#include "config.h"

/* ============================================================
 *  Écran OLED
 * ============================================================ */
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

/* ============================================================
 *  Variables partagées avec l'interruption
 *  (déclarées "volatile" car modifiées dans l'ISR)
 * ============================================================ */
volatile uint32_t gPulseCount      = 0;   // impulsions depuis le dernier START
volatile uint32_t gLastPulseUs     = 0;   // micros() de la dernière impulsion
volatile uint32_t gPulseIntervalUs = 0;   // durée entre 2 impulsions
volatile bool     gRunning         = false;

/* ============================================================
 *  État applicatif
 * ============================================================ */
static uint32_t gTarget     = TARGET_DEFAULT;  // consigne en tours
static bool     gDone       = false;           // consigne atteinte
static uint32_t gRpm        = 0;               // vitesse affichée
static uint32_t gLastDrawMs = 0;
static bool     gForceDraw  = true;

// --- Sauvegarde de la consigne en EEPROM ---
static bool     gTargetDirty        = false;
static uint32_t gLastTargetChangeMs = 0;

#define EEPROM_SIZE        16
#define EEPROM_ADDR_MAGIC  0
#define EEPROM_ADDR_TARGET 2
#define EEPROM_MAGIC_VALUE 0xBEEF

/* ============================================================
 *  Boutons (anti-rebond + répétition automatique)
 * ============================================================ */
struct Button {
  uint8_t  pin;
  bool     allowRepeat;   // répétition tant que le bouton est maintenu
  bool     stable;        // état logique stable (true = appuyé)
  bool     lastRead;
  uint32_t lastChangeMs;
  uint32_t nextRepeatMs;
};

static Button gBtnInc   = { PIN_BTN_INC,   true,  false, false, 0, 0 };
static Button gBtnDec   = { PIN_BTN_DEC,   true,  false, false, 0, 0 };
static Button gBtnReset = { PIN_BTN_RESET, false, false, false, 0, 0 };
static Button gBtnStart = { PIN_BTN_START, false, false, false, 0, 0 };

/* Renvoie true au front d'appui, puis se répète si allowRepeat. */
static bool buttonTick(Button &b) {
  const uint32_t now     = millis();
  const bool     reading = (digitalRead(b.pin) == LOW);   // pull-up → LOW = appuyé

  if (reading != b.lastRead) {
    b.lastRead     = reading;
    b.lastChangeMs = now;
  }

  if ((now - b.lastChangeMs) < BTN_DEBOUNCE_MS) {
    return false;                       // anti-rebond
  }

  if (reading != b.stable) {
    b.stable = reading;
    if (reading) {                      // front montant = appui
      b.nextRepeatMs = now + BTN_REPEAT_DELAY_MS;
      return true;
    }
    return false;                       // relâchement
  }

  if (reading && b.allowRepeat && now >= b.nextRepeatMs) {
    b.nextRepeatMs = now + BTN_REPEAT_PERIOD_MS;
    return true;
  }
  return false;
}

/* ============================================================
 *  Commande du relais
 * ============================================================ */
static inline void relayWrite(bool on) {
#if RELAY_ACTIVE_LOW
  digitalWrite(PIN_RELAY, on ? LOW : HIGH);
#else
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
#endif
}

/* ============================================================
 *  Interruption : une impulsion du capteur optique
 *  → doit être ultra-rapide et résider en RAM (ICACHE_RAM_ATTR)
 * ============================================================ */
void ICACHE_RAM_ATTR onPulse() {
  if (!gRunning) {
    return;                             // on ne compte que moteur en marche
  }

  const uint32_t now   = micros();
  const uint32_t delta = now - gLastPulseUs;

  // Anti-rebond : on ignore les impulsions trop rapprochées (bruit du
  // comparateur du capteur optique sur les fronts).
  if (delta < PULSE_MIN_INTERVAL_US) {
    return;
  }

  gPulseIntervalUs = delta;
  gLastPulseUs     = now;
  gPulseCount++;
}

/* ============================================================
 *  Machine d'état moteur
 * ============================================================ */
static void startMotor() {
  gPulseCount      = 0;
  gDone            = false;
  gPulseIntervalUs = 0;
  gLastPulseUs     = micros();
  gRunning         = true;              // AVANT le relais, pour ne rien manquer
  relayWrite(true);
  gForceDraw = true;
  Serial.println(F("[MOTEUR] MARCHE"));
}

static void stopMotor() {
  relayWrite(false);                    // relais au repos = moteur arrêté
  gRunning   = false;
  gRpm       = 0;
  gForceDraw = true;
  Serial.println(F("[MOTEUR] ARRÊT"));
}

static void finishRun() {
  stopMotor();
  gDone = true;
  gForceDraw = true;
  Serial.print(F("[FIN] Consigne atteinte : "));
  Serial.print(gPulseCount / PULSES_PER_TURN);
  Serial.println(F(" tours"));
}

/* ============================================================
 *  Affichage
 * ============================================================ */
static void drawScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // --- Titre ---
#if OLED_HEIGHT >= 64
  // ================= Disposition 128x64 =================
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("COMPTEUR DE TOURS"));

  // --- Compteur courant (grand) ---
  display.setTextSize(2);
  display.setCursor(0, 12);
  display.print(gPulseCount / PULSES_PER_TURN);

  // --- Consigne ---
  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print(F("Consigne : "));
  display.print(gTarget);

  // --- Vitesse ---
  display.setCursor(0, 42);
  display.print(F("Vitesse : "));
  display.print(gRpm);

  // --- État ---
  display.setCursor(0, 54);
  display.print(F("Etat : "));
  if (gRunning)        display.print(F("MARCHE"));
  else if (gDone)      display.print(F("TERMINE"));
  else                 display.print(F("ARRET"));
#else
  // ================= Disposition 128x32 =================
  // Ligne 1 : consigne + vitesse
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("C:"));
  display.print(gTarget);
  display.print(F("  "));
  display.print(gRpm);
  display.print(F(" tr/min"));

  // Ligne 2 : compteur courant (grand)
  display.setTextSize(2);
  display.setCursor(0, 8);
  display.print(gPulseCount / PULSES_PER_TURN);

  // Ligne 3 : état
  display.setTextSize(1);
  display.setCursor(0, 24);
  if (gRunning)        display.print(F("MARCHE"));
  else if (gDone)      display.print(F("TERMINE"));
  else                 display.print(F("ARRET"));
#endif

  display.display();
}

/* ============================================================
 *  EEPROM : sauvegarde / restauration de la consigne
 * ============================================================ */
static void loadTarget() {
  EEPROM.begin(EEPROM_SIZE);

  uint16_t magic = 0;
  EEPROM.get(EEPROM_ADDR_MAGIC, magic);
  if (magic != EEPROM_MAGIC_VALUE) {
    Serial.println(F("[EEPROM] Pas de consigne sauvegardee."));
    return;
  }

  uint16_t saved = 0;
  EEPROM.get(EEPROM_ADDR_TARGET, saved);
  if (saved >= TARGET_MIN && saved <= TARGET_MAX) {
    gTarget = saved;
    Serial.print(F("[EEPROM] Consigne restauree : "));
    Serial.println(gTarget);
  }
}

static void saveTarget() {
  uint16_t magic = EEPROM_MAGIC_VALUE;
  EEPROM.put(EEPROM_ADDR_MAGIC, magic);
  EEPROM.put(EEPROM_ADDR_TARGET, (uint16_t)gTarget);
  if (EEPROM.commit()) {
    Serial.print(F("[EEPROM] Consigne sauvegardee : "));
    Serial.println(gTarget);
  } else {
    Serial.println(F("[EEPROM] ERREUR de sauvegarde."));
  }
}

/* ============================================================
 *  SETUP
 * ============================================================ */
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=============================="));
  Serial.println(F("  Compte-tours moteur 220 V"));
  Serial.println(F("=============================="));

  // ---- Consigne sauvegardée en EEPROM ----
  loadTarget();

  // ---- Relais : forcer l'état ARRÊT dès le démarrage ----
  pinMode(PIN_RELAY, OUTPUT);
  relayWrite(false);

  // ---- Boutons : pull-up interne, appui = GND ----
  pinMode(PIN_BTN_INC,   INPUT_PULLUP);
  pinMode(PIN_BTN_DEC,   INPUT_PULLUP);
  pinMode(PIN_BTN_RESET, INPUT_PULLUP);
  pinMode(PIN_BTN_START, INPUT_PULLUP);

  // ---- Capteur optique : entrée avec interruption ----
  // INPUT_PULLUP : indispensable si la sortie du FC-33 est à
  // collecteur ouvert (LM393). Front DESCENDANT = 1 impulsion/tour.
  pinMode(PIN_SENSOR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), onPulse, FALLING);

  // ---- Écran OLED ----
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[ERREUR] Ecran OLED introuvable."));
    Serial.println(F("         Verifiez le cablage I2C et l'adresse (0x3C / 0x3D)."));
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("Compte-tours pret"));
    display.display();
  }

  delay(800);
  gForceDraw = true;
}

/* ============================================================
 *  LOOP
 * ============================================================ */
void loop() {
  const uint32_t nowMs = millis();

  /* ---------- Boutons ---------- */
  if (buttonTick(gBtnInc)) {
    if (gTarget < TARGET_MAX) {
      gTarget += TARGET_STEP;
      if (gTarget > TARGET_MAX) gTarget = TARGET_MAX;
      gTargetDirty        = true;
      gLastTargetChangeMs = nowMs;
      gForceDraw          = true;
    }
  }

  if (buttonTick(gBtnDec)) {
    if (gTarget > TARGET_MIN) {
      if (gTarget > TARGET_MIN + TARGET_STEP) {
        gTarget -= TARGET_STEP;
      } else {
        gTarget = TARGET_MIN;
      }
      gTargetDirty        = true;
      gLastTargetChangeMs = nowMs;
      gForceDraw          = true;
    }
  }

  if (buttonTick(gBtnReset)) {
    stopMotor();
    gPulseCount = 0;
    gDone       = false;
    gRpm        = 0;
    gForceDraw  = true;
    Serial.println(F("[CMD] RAZ compteur"));
  }

  if (buttonTick(gBtnStart)) {
    if (gRunning) {
      stopMotor();                      // appui = STOP manuel
      Serial.println(F("[CMD] STOP manuel"));
    } else {
      if (gTargetDirty) {               // on fige la consigne avant de démarrer
        saveTarget();
        gTargetDirty = false;
      }
      startMotor();                     // appui = START
      Serial.print(F("[CMD] START vers "));
      Serial.print(gTarget);
      Serial.println(F(" tours"));
    }
  }

  /* ---------- Sauvegarde de la consigne après inactivité ---------- */
  if (gTargetDirty && (nowMs - gLastTargetChangeMs >= EEPROM_SAVE_DELAY_MS)) {
    saveTarget();
    gTargetDirty = false;
  }

  /* ---------- Fin de course : consigne atteinte ---------- */
  if (gRunning && (gPulseCount / PULSES_PER_TURN) >= gTarget) {
    finishRun();
  }

  /* ---------- Calcul de la vitesse (tr/min) ---------- */
  const uint32_t nowUs    = micros();
  const uint32_t interval = gPulseIntervalUs;

  if (gRunning && interval > 0 && (nowUs - gLastPulseUs) < RPM_TIMEOUT_US) {
    uint32_t rpm = 60000000UL / interval / PULSES_PER_TURN;
    if (rpm > 60000UL) {
      rpm = 60000UL;                    // garde-fou (parasites)
    }
    gRpm = (gRpm * 3 + rpm) / 4;        // lissage (moyenne glissante)
  } else {
    gRpm = 0;
  }

  /* ---------- Debug série (1 fois/s pendant la marche) ---------- */
  static uint32_t lastDbgMs = 0;
  if (gRunning && (nowMs - lastDbgMs >= 1000)) {
    lastDbgMs = nowMs;
    Serial.print(F("[DBG] pulses="));
    Serial.print(gPulseCount);
    Serial.print(F("  tours="));
    Serial.print(gPulseCount / PULSES_PER_TURN);
    Serial.print(F("  interval_us="));
    Serial.println(gPulseIntervalUs);
  }

  /* ---------- Rafraîchissement de l'écran ---------- */
  if (gForceDraw || (nowMs - gLastDrawMs) >= DISPLAY_PERIOD_MS) {
    drawScreen();
    gLastDrawMs = nowMs;
    gForceDraw  = false;
  }

  delay(5);
}
