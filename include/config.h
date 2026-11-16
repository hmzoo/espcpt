/* ============================================================
 *  config.h — Configuration matérielle et paramètres métier
 *  Projet : Compte-tours moteur 220 V (NodeMCU v2 / ESP8266)
 * ============================================================
 *  C'est LE fichier à modifier si vous changez le câblage
 *  ou les paramètres (consigne par défaut, sens du relais...).
 * ============================================================ */

#pragma once

/* ------------------------------------------------------------
 *  1. AFFECTATION DES BROCHES  (étiquettes NodeMCU → GPIO)
 * ------------------------------------------------------------
 *  | Étiquette | GPIO  | Utilisation ici                        |
 *  |-----------|-------|----------------------------------------|
 *  | D0        | 16    | Relais (sortie)                        |
 *  | D1        | 5     | OLED SCL (I2C)                         |
 *  | D2        | 4     | OLED SDA (I2C)                         |
 *  | D3        | 0     | Bouton +   (⚠ pin de boot)             |
 *  | D4        | 2     | Bouton −   (⚠ pin de boot)             |
 *  | D5        | 14    | Bouton RAZ                             |
 *  | D6        | 12    | Capteur optique (ENTRÉE D'INTERRUPTION)|
 *  | D7        | 13    | Bouton START                           |
 *  | D8        | 15    | ⚠ NON UTILISÉ (doit rester LOW au boot)|
 * ------------------------------------------------------------ */

#define PIN_OLED_SCL   D1   // GPIO5
#define PIN_OLED_SDA   D2   // GPIO4
#define PIN_SENSOR     D6   // GPIO12 — entrée d'interruption
#define PIN_RELAY      D0   // GPIO16 — sortie commande relais

#define PIN_BTN_INC    D3   // GPIO0  — bouton "+"
#define PIN_BTN_DEC    D4   // GPIO2  — bouton "−"
#define PIN_BTN_RESET  D5   // GPIO14 — bouton "RAZ"
#define PIN_BTN_START  D7   // GPIO13 — bouton "START/STOP" (⚠ 4e bouton à câbler)

/* ------------------------------------------------------------
 *  2. RELAIS
 * ------------------------------------------------------------
 *  1 = module actif à l'état BAS  (IN = LOW  → relais collé)
 *  0 = module actif à l'état HAUT (IN = HIGH → relais collé)
 *  La plupart des modules bleus "SRD-5VDC-SL-C" sont ACTIFS BAS.
 *  ➜ À VÉRIFIER avec votre module avant de brancher le moteur !
 * ------------------------------------------------------------ */
#define RELAY_ACTIVE_LOW   1

/* ------------------------------------------------------------
 *  3. ÉCRAN OLED (SSD1306, I2C)
 * ------------------------------------------------------------
 *  Module utilisé : 0,91" 128x32 (petit écran long et étroit).
 *  Si vous remontez un 0,96"/1,3" 128x64, passez OLED_HEIGHT à 64 :
 *  la disposition de l'écran s'adapte automatiquement (cf. main.cpp).
 * ------------------------------------------------------------ */
#define OLED_WIDTH   128
#define OLED_HEIGHT  32
#define OLED_ADDR    0x3C   // essayer 0x3D si l'écran n'est pas détecté

/* ------------------------------------------------------------
 *  4. PARAMÈTRES DU COMPTE-TOURS
 * ------------------------------------------------------------ */

// Nombre d'impulsions du capteur par tour moteur.
// Disque plein avec 1 encoche : le faisceau est interrompu puis rétabli
// à chaque tour, soit 2 fronts par tour (entrée + sortie de l'encoche).
#define PULSES_PER_TURN   2

// Anti-rebond du capteur optique : ignore les impulsions plus rapprochées
// que cette durée (filtre les rebonds du comparateur et le bruit).
// 5000 µs = 5 ms  ->  jusqu'à 200 impulsions/s (12 000 tr/min avec 1 encoche).
#define PULSE_MIN_INTERVAL_US  5000

// Bornes et valeur par défaut de la consigne (en tours)
#define TARGET_MIN        1
#define TARGET_MAX        9999
#define TARGET_DEFAULT    100

// Pas d'incrémentation / décrémentation de la consigne
#define TARGET_STEP       10

// Durée d'inactivité avant sauvegarde de la consigne en EEPROM (ms)
#define EEPROM_SAVE_DELAY_MS  1500

// Anti-rebond des boutons (ms)
#define BTN_DEBOUNCE_MS   40

// Répétition automatique quand on maintient "+" ou "−"
#define BTN_REPEAT_DELAY_MS   400   // délai avant le 1er rappel
#define BTN_REPEAT_PERIOD_MS   80   // période des rappels

// Si aucune impulsion n'arrive pendant ce délai, la vitesse est
// affichée à 0 (3 s ⇒ détecte les moteurs jusqu'à ~20 tr/min)
#define RPM_TIMEOUT_US    3000000UL

// Période de rafraîchissement de l'écran (ms)
#define DISPLAY_PERIOD_MS 100
