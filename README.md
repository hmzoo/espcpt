# Compte-tours moteur 220 V — NodeMCU v2 (ESP8266)

Système de comptage de tours pour un moteur électrique 220 V monophasé,
piloté par une carte **NodeMCU v2 (Amica) / ESP8266**.

On sélectionne une **consigne de tours** avec deux boutons, on démarre le moteur,
le compteur s'incrémente à chaque tour, et le moteur **s'arrête automatiquement**
quand la consigne est atteinte.

---

## ⚠️ Avertissement sécurité

Ce projet pilote du **220 V secteur**, potentiellement mortel.

- Débranchez toujours le secteur avant toute intervention.
- Utilisez un **boîtier isolant fermé**, un **fusible** et une mise à la **terre** de la carcasse moteur.
- Installez un **snubber RC** sur les contacts du relais (charge inductive).
- Ne laissez jamais de conducteur 220 V accessible.

---

## 🧩 Matériel

| Composant | Référence | Remarques |
|-----------|-----------|-----------|
| Microcontrôleur | **NodeMCU v2 (Amica)** | ESP8266EX, module ESP-12E, 4 Mo, pont CP2102 |
| Écran | **OLED SSD1306 I²C 0,91"** 128×32 | Module long et étroit, adresse `0x3C` ; le code gère aussi le 128×64 via `OLED_HEIGHT` |
| Capteur | **Module optique à fourche FC-33** | Disque plein à 1 encoche → **2 impulsions/tour** |
| Actionneur | **Module relais 5 V** (SRD-5VDC-SL-C) | Contacts 10 A / 250 VAC |
| Boutons | **4 × poussoir tactile 12×12 mm** | `+`, `−`, `RAZ`, `START` |
| Protection | Snubber RC, fusible 2 A, condensateur 100 µF | Voir `docs/wiring.md` |

> Le moteur doit consommer **< 3 A (≈ 700 W)** pour que le relais 10 A convienne
> (attention au courant d'appel : le snubber est fortement recommandé).

---

## 🔌 Câblage

Le schéma détaillé, les tableaux de connexion et les précautions sont dans
**[`docs/wiring.md`](docs/wiring.md)**.

Résumé de l'affectation des broches :

| Fonction | NodeMCU | GPIO |
|----------|---------|------|
| OLED `SCL` | `D1` | GPIO5 |
| OLED `SDA` | `D2` | GPIO4 |
| OLED `VCC` | `VIN` | 5 V *(régulateur interne)* |
| OLED `GND` | `GND` | — |
| Capteur optique (`OUT`) | `D6` | GPIO12 *(interruption)* |
| Relais (`IN`) | `D0` | GPIO16 |
| Bouton `+` | `D3` | GPIO0 |
| Bouton `−` | `D4` | GPIO2 |
| Bouton `RAZ` | `D5` | GPIO14 |
| Bouton `START` | `D7` | GPIO13 |

---

## 🛠️ Installation et flashage

### Prérequis

- [PlatformIO](https://platformio.org/) (extension VS Code **ou** CLI `pio`)
- Pilote CP210x (généralement déjà présent sous Linux)

### Avec VS Code

1. Ouvrir ce dossier dans VS Code (l'extension PlatformIO détecte `platformio.ini`).
2. **Build** : icône ✔ (ou `PlatformIO: Build`).
3. **Upload** : icône → (ou `PlatformIO: Upload`).
4. **Moniteur série** : `PlatformIO: Serial Monitor` (115200 bauds).

### En ligne de commande

```bash
pio run                  # compiler
pio run --target upload  # compiler + flasher
pio device monitor       # moniteur série (115200)
```

Le port est détecté automatiquement. Pour le forcer :

```bash
pio run --target upload --upload-port /dev/ttyUSB0
```

### Tester uniquement l'écran OLED

Un sketch dédié (`src/oled_test.cpp`) ne touche **qu'à l'écran** : scan du bus I2C,
test de tous les pixels (blanc, damier, inversion) puis animation. Il est compilé
dans l'environnement `oled_test` :

```bash
pio run -e oled_test -t upload     # flasher le test
pio device monitor -e oled_test    # voir le scan I2C
```

L'adresse I2C détectée (`0x3C` ou `0x3D`) s'affiche **à l'écran** et sur le port série.

**LED intégrée (témoin de vie)** — la LED bleue de la carte (`GPIO2` / `D4`) clignote :

| Clignotement | Signification |
|--------------|---------------|
| **3 impulsions rapides** au démarrage | L'ESP démarre correctement |
| **1 Hz** (lent, régulier) | L'ESP **et** l'écran fonctionnent |
| **5 Hz** (rapide) | L'ESP fonctionne, mais **l'écran n'est pas détecté** → vérifier son câblage |

Pour revenir ensuite au firmware principal :

```bash
pio run -t upload                  # environnement nodemcuv2 (par défaut)
```

---

## 🎮 Utilisation

| Bouton | Action |
|--------|--------|
| **`+`** | Augmente la consigne (**+10** ; maintien = répétition rapide) |
| **`−`** | Diminue la consigne (**−10** ; maintien = répétition rapide) |
| **`RAZ`** | Remet le compteur à zéro et arrête le moteur |
| **`START`** | Démarre le moteur ; un second appui l'arrête manuellement |

> 💾 **Mémoire** : la consigne est sauvegardée automatiquement en EEPROM
> (1,5 s après la dernière modification, ou au moment du `START`) et
> **restaurée au démarrage** suivant.

Déroulement typique :

1. Régler la consigne (ex. `500` tours) avec `+` / `−`.
2. Appuyer sur `START` → le relais colle, le moteur tourne.
3. L'écran affiche en temps réel : **tours comptés / vitesse (tr/min) / état**.
4. À 500 tours, le relais retombe → moteur arrêté, état `TERMINE`.

### Affichage OLED

Sur le module **0,91" 128×32** utilisé ici :

```
C:500  1480 tr/min
      500          <- tours comptés (grand format)
MARCHE
```

> Pour un écran **128×64**, la disposition s'adapte automatiquement (4 lignes,
> compteur en grand) : il suffit de mettre `OLED_HEIGHT = 64` dans `config.h`.

---

## ⚙️ Configuration

Tout se règle dans **[`include/config.h`](include/config.h)** :

| Paramètre | Rôle |
|-----------|------|
| `RELAY_ACTIVE_LOW` | `1` si votre module relais est actif à l'état bas (le plus courant) |
| `PULSES_PER_TURN` | Impulsions par tour : **2** pour un disque plein à 1 encoche (entrée + sortie) |
| `TARGET_DEFAULT` / `TARGET_MIN` / `TARGET_MAX` | Bornes de la consigne |
| `TARGET_STEP` | Pas des boutons `+` / `−` (`10` par défaut) |
| `EEPROM_SAVE_DELAY_MS` | Délai avant sauvegarde automatique de la consigne |
| `OLED_HEIGHT` | `32` (0,91") ou `64` (0,96"/1,3") — adapte la disposition |
| `OLED_ADDR` | `0x3C` ou `0x3D` selon l'écran |
| `BTN_DEBOUNCE_MS` | Anti-rebond des boutons |
| `RPM_TIMEOUT_US` | Délai sans impulsion avant d'afficher `0 tr/min` |

---

## 📁 Arborescence

```
espcpt/
├── platformio.ini          # Configuration PlatformIO
├── README.md               # Ce fichier
├── include/
│   └── config.h            # Broches + paramètres  ← à modifier si besoin
├── src/
│   └── main.cpp            # Programme principal
├── docs/
│   └── wiring.md           # Schéma de câblage complet
└── photos/                 # Photos des composants
```

---

## 🔍 Dépannage

| Symptôme | Cause probable / Solution |
|----------|---------------------------|
| `Écran OLED introuvable` | Vérifier `SCL`/`SDA`, l'alimentation, et tester `0x3D` dans `config.h` |
| Le moteur démarre au branchement | Module relais actif-bas + GPIO16 ; ajouter un **transistor inverseur** ou inverser `RELAY_ACTIVE_LOW` |
| Aucun comptage | Vérifier le capteur (fourche), `INPUT_PULLUP`, le front `FALLING`, et le disque |
| Comptage doublé | Le capteur génère 2 fronts → garder `FALLING` (déjà le cas) |
| La carte ne flashe pas | Un bouton `+`/`−` est appuyé au boot (GPIO0/GPIO2) ou câble USB de charge seule |
| Vitesse affichée 0 | Moteur lent (< 20 tr/min) → augmenter `RPM_TIMEOUT_US` |
| Redémarrages aléatoires | Pic de courant du relais → ajouter le condensateur 100 µF sur le 5 V |

---

## 📜 Licence

Projet personnel — utilisation libre.
