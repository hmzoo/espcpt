/* ============================================================
 *  TEST OLED — NodeMCU v2 (ESP8266)
 * ============================================================
 *  Sketch autonome qui ne touche QU'À l'écran OLED.
 *  Aucune autre broche (relais, capteur, boutons) n'est utilisée.
 *
 *  Câblage :
 *    OLED VCC → 3.3V
 *    OLED GND → GND
 *    OLED SCL → D1 (GPIO5)
 *    OLED SDA → D2 (GPIO4)
 *
 *  Ce que fait le test :
 *    1. Scan du bus I2C (affiche les adresses trouvées sur le port série)
 *    2. Test de tous les pixels (blanc, damier, inversion)
 *    3. Affichage animé : compteur + barre de progression
 *
 *  Compilation / flash :
 *    pio run -e oled_test -t upload
 *    pio device monitor -e oled_test
 * ============================================================ */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SDA      D2      // GPIO4
#define OLED_SCL      D1      // GPIO5
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

/* --- LED intégrée de la carte (ESP-12E : GPIO2 = D4) ---
 * Elle est active à l'état BAS (allumée quand la broche est LOW). */
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
#define LED_PIN         LED_BUILTIN
#define LED_ACTIVE_LOW  1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static uint8_t  gAddr      = 0;
static bool     gOledReady = false;
static uint32_t gLedNext   = 0;
static bool     gLedState  = false;

/* ------------------------------------------------------------
 *  LED : écriture, clignotement court, heartbeat non bloquant
 * ------------------------------------------------------------ */
static inline void ledWrite(bool on) {
#if LED_ACTIVE_LOW
  digitalWrite(LED_PIN, on ? LOW : HIGH);
#else
  digitalWrite(LED_PIN, on ? HIGH : LOW);
#endif
}

/* Clignote pendant `ms` (utilisé pendant les tests de démarrage). */
static void delayBlink(uint32_t ms) {
  const uint32_t start = millis();
  while (millis() - start < ms) {
    gLedState = !gLedState;
    ledWrite(gLedState);
    delay(100);
  }
}

/* Heartbeat : 1 Hz si l'écran est OK, 5 Hz sinon (code d'erreur visuel). */
static void heartbeat() {
  const uint32_t now = millis();
  if (now >= gLedNext) {
    gLedNext  = now + (gOledReady ? 500 : 100);
    gLedState = !gLedState;
    ledWrite(gLedState);
  }
}

/* ------------------------------------------------------------
 *  Scan du bus I2C — retourne 0x3C ou 0x3D si trouvé
 * ------------------------------------------------------------ */
static uint8_t scanI2C() {
  Serial.println(F("Scan I2C (SDA=D2/GPIO4, SCL=D1/GPIO5)..."));
  uint8_t found = 0;

  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  Peripherique trouve a 0x"));
      Serial.println(addr, HEX);
      if (addr == 0x3C || addr == 0x3D) {
        found = addr;          // adresse typique d'un SSD1306
      }
    }
  }

  if (found == 0) {
    Serial.println(F("  Aucun ecran SSD1306 (0x3C/0x3D) detecte."));
  }
  return found;
}

/* ------------------------------------------------------------
 *  Test matériel de l'écran : tous les pixels
 * ------------------------------------------------------------ */
static void pixelTest() {
  // Ecran noir
  display.clearDisplay();
  display.display();
  delayBlink(200);

  // Ecran entièrement blanc
  display.fillScreen(SSD1306_WHITE);
  display.display();
  delayBlink(500);

  // Damier : permet de repérer un pixel mort ou une colonne manquante
  display.clearDisplay();
  for (int y = 0; y < SCREEN_HEIGHT; y += 2) {
    for (int x = (y / 2) % 2; x < SCREEN_WIDTH; x += 2) {
      display.drawPixel(x, y, SSD1306_WHITE);
    }
  }
  display.display();
  delayBlink(900);

  // Test du mode inversé
  display.fillScreen(SSD1306_WHITE);
  display.display();
  delayBlink(300);
  display.invertDisplay(true);
  delayBlink(500);
  display.invertDisplay(false);

  display.clearDisplay();
  display.display();
}

/* ------------------------------------------------------------
 *  SETUP
 * ------------------------------------------------------------ */
void setup() {
  Serial.begin(115200);

  // --- LED : 3 clignotements rapides = l'ESP démarre bien ---
  pinMode(LED_PIN, OUTPUT);
  ledWrite(false);
  for (int i = 0; i < 3; i++) {
    ledWrite(true);  delay(120);
    ledWrite(false); delay(120);
  }
  gLedNext = millis() + 500;

  delay(300);

  Serial.println(F("\n=============================="));
  Serial.println(F("        TEST ECRAN OLED"));
  Serial.println(F("=============================="));

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);           // 400 kHz ; repli à 100 kHz si instable

  gAddr = scanI2C();

  if (gAddr == 0) {
    Serial.println(F("AUCUN PERIPHERIQUE I2C detecte !"));
    Serial.println(F("Verifiez l'ordre des broches du module : GND VCC SCL SDA"));
    Serial.println(F("  GND -> GND   VCC -> 3.3V (ou 5V)   SCL -> D1   SDA -> D2"));
    Serial.println(F("Verifiez aussi que la barrette 4 pins est bien soudee."));
    Serial.println(F("=> LED clignote RAPIDEMENT (5 Hz) : l'ESP fonctionne, l'ecran non."));
    gOledReady = false;
    return;    // on ne bloque pas : loop() continue de faire clignoter la LED
  }

  gOledReady = display.begin(SSD1306_SWITCHCAPVCC, gAddr);

  if (!gOledReady) {
    Serial.println(F("Echec a 400 kHz, nouvel essai a 100 kHz..."));
    Wire.setClock(100000);
    gOledReady = display.begin(SSD1306_SWITCHCAPVCC, gAddr);
  }

  if (!gOledReady) {
    Serial.println(F("ECHEC display.begin()."));
    Serial.println(F("Verifiez : VCC=3.3V, GND, SCL=D1, SDA=D2,"));
    Serial.println(F("adresse 0x3C/0x3D, ou modele SH1106 au lieu de SSD1306."));
    Serial.println(F("=> La LED clignote RAPIDEMENT (5 Hz) = l'ESP fonctionne,"));
    Serial.println(F("   le probleme vient de l'ecran."));
    return;    // on ne bloque pas : loop() continue de faire clignoter la LED
  }

  Serial.print(F("Ecran initialise a 0x"));
  Serial.println(gAddr, HEX);

  pixelTest();
}

/* ------------------------------------------------------------
 *  LOOP : affichage animé
 * ------------------------------------------------------------ */
void loop() {
  heartbeat();     // LED : 1 Hz (ou 5 Hz si l'écran est KO)

  if (!gOledReady) {
    return;        // écran absent : on continue simplement de clignoter
  }

  static uint32_t t0        = millis();
  static uint32_t lastDraw  = 0;
  static uint32_t lastSec   = 0xFFFFFFFF;

  if (millis() - lastDraw < 200) {
    return;
  }
  lastDraw = millis();

  const uint32_t sec = (millis() - t0) / 1000;

  // Petit log série une fois par seconde
  if (sec != lastSec) {
    lastSec = sec;
    Serial.print(F("t = "));
    Serial.print(sec);
    Serial.println(F(" s"));
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

#if SCREEN_HEIGHT >= 64
  // --- En-tête 128x64 ---
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("TEST OLED : OK"));

  display.setCursor(0, 10);
  display.print(F("I2C 0x"));
  display.print(gAddr, HEX);
  display.print(F("  "));
  display.print(SCREEN_WIDTH);
  display.print(F("x"));
  display.print(SCREEN_HEIGHT);

  // --- Compteur en gros ---
  display.setTextSize(2);
  display.setCursor(0, 26);
  display.print(F("t="));
  display.print(sec);
  display.print(F("s"));

  // --- Barre de progression ---
  const int w = (sec % 10) * 12;   // 0..108 px
  display.drawRect(0, 52, SCREEN_WIDTH, 11, SSD1306_WHITE);
  display.fillRect(2, 54, w, 7, SSD1306_WHITE);
#else
  // --- En-tête 128x32 ---
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("OLED 0x"));
  display.print(gAddr, HEX);
  display.print(F(" 128x32"));

  // --- Compteur en gros ---
  display.setTextSize(2);
  display.setCursor(0, 9);
  display.print(F("t="));
  display.print(sec);
  display.print(F("s"));

  // --- Barre de progression ---
  const int w = (sec % 10) * 12;   // 0..108 px
  display.drawRect(0, 26, SCREEN_WIDTH, 6, SSD1306_WHITE);
  display.fillRect(1, 27, w, 4, SSD1306_WHITE);
#endif

  display.display();
}
