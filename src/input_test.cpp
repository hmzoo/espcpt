/* ============================================================
 *  TEST DES ENTREES — NodeMCU v2 (ESP8266)
 * ============================================================
 *  Sketch de diagnostic : affiche l'état des 4 boutons et du
 *  capteur optique, sur l'écran OLED ET sur le port série.
 *
 *  Un "1" = appuyé (bouton) ou faisceau coupé (capteur).
 *  Un "0" = relâché / faisceau libre.
 *
 *  Le relais est forcé au repos (aucune commutation).
 *
 *  Utilisation :
 *    pio run -e input_test -t upload
 *    pio device monitor -e input_test
 * ============================================================ */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

static bool gOledReady = false;

/* ------------------------------------------------------------
 *  SETUP
 * ------------------------------------------------------------ */
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== TEST DES ENTREES ==="));
  Serial.println(F("1 = appuye / faisceau coupe"));

  // --- Relais forcé au repos (sécurité) ---
  pinMode(PIN_RELAY, OUTPUT);
#if RELAY_ACTIVE_LOW
  digitalWrite(PIN_RELAY, HIGH);
#else
  digitalWrite(PIN_RELAY, LOW);
#endif

  // --- Boutons + capteur ---
  pinMode(PIN_BTN_INC,   INPUT_PULLUP);
  pinMode(PIN_BTN_DEC,   INPUT_PULLUP);
  pinMode(PIN_BTN_RESET, INPUT_PULLUP);
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_SENSOR,    INPUT_PULLUP);

  // --- Ecran ---
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  gOledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (gOledReady) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("TEST ENTREES"));
    display.display();
  } else {
    Serial.println(F("[ERREUR] Ecran OLED non detecte"));
  }
}

/* ------------------------------------------------------------
 *  LOOP
 * ------------------------------------------------------------ */
void loop() {
  const bool inc = (digitalRead(PIN_BTN_INC)   == LOW);
  const bool dec = (digitalRead(PIN_BTN_DEC)   == LOW);
  const bool raz = (digitalRead(PIN_BTN_RESET) == LOW);
  const bool sta = (digitalRead(PIN_BTN_START) == LOW);
  const bool cap = (digitalRead(PIN_SENSOR)    == LOW);  // LOW = faisceau coupe

  if (gOledReady) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("TEST ENTREES"));
    display.setCursor(0, 10);
    display.print(F("+"));  display.print(inc);
    display.print(F(" -")); display.print(dec);
    display.print(F(" R")); display.print(raz);
    display.print(F(" S")); display.print(sta);
    display.setCursor(0, 20);
    display.print(F("CAP: ")); display.print(cap);
    display.display();
  }

  Serial.print(F("+="));  Serial.print(inc);
  Serial.print(F("  -=")); Serial.print(dec);
  Serial.print(F("  RAZ=")); Serial.print(raz);
  Serial.print(F("  START=")); Serial.print(sta);
  Serial.print(F("  CAPTEUR=")); Serial.println(cap);

  delay(150);
}
