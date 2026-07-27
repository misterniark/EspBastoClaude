# ESPBasto - Controleur Webasto

Controleur de chauffage Webasto pour van amenage, avec ecran tactile et communication sans fil.

## Materiel

### Variante CrowPanel — **cible de production** (`pio run`)

| Composant | Reference |
|-----------|-----------|
| Controleur | [CrowPanel Advance 2.8" HMI](https://www.elecrow.com/pub/wiki/CrowPanel_Advance_2.8-HMI_ESP32_AI_Display.html) (ESP32-S3 + ecran 2.8" ST7789 + tactile capacitif FT6236) |
| Capteur | Crowtail One Wire Waterproof Temperature Sensor 2.0 (DS18B20 etanche), branche sur le connecteur **UART1-OUT** (signal = IO18) |
| Batterie | MAX17048 (jauge I2C, optionnelle, sur le connecteur I2C-OUT) |

Notes CrowPanel :
- Le DS18B20 ne mesure pas l'humidite (non utilisee par l'interface).
- Batterie lithium 3.7-4.2V sur le connecteur BAT (circuit de charge integre).
- A valider au premier flash : orientation de l'image (constante `TFT_ROTATION`
  dans `src/config.h`), orientation du tactile (drapeaux `TOUCH_SWAP_XY`,
  `TOUCH_MIRROR_X`, `TOUCH_MIRROR_Y`), et ordre des couleurs
  (`TFT_RGB_ORDER` dans `platformio.ini` si rouge et bleu sont inverses).

### Relais (commun)

| Composant | Reference |
|-----------|-----------|
| Relais | Seeed XIAO ESP32-C3 (projet separe : [ESPBastoRelay](../ESPBastoRelay)) |
| Communication | ESP-NOW (~200m, decouverte automatique) |

### Variante CYD (`pio run -e cyd`) — NON MAINTENUE

Le portage ESP32-2432S028R (ESP32 + ecran ILI9341 + tactile resistif
XPT2046 + capteur AHT21) est **abandonne depuis le 27/07/2026**. Le code
est conserve dans les branches `#else` du projet mais n'est plus teste
sur materiel, plus construit par defaut, et ne suit plus les evolutions.

**Ne pas mettre en service** : l'audit du 27/07/2026 a releve un defaut
non corrige de l'anti-rebond tactile (`src/hal/touchpad.cpp`) qui
transforme un appui maintenu en rafale de clics — un seul tap peut
demarrer puis arreter le chauffage. Pour reanimer la cible : corriger
l'anti-rebond, epingler la dependance Git `XPT2046_Touchscreen`, puis
revalider integralement au banc.

## Mise en service : verifications materielles obligatoires

Trois points que le logiciel ne peut pas compenser. A traiter AVANT
toute installation reelle sur le Webasto.

### 1. Resistance de rappel 4,7 kOhm sur le bus de la sonde

Le module Crowtail One Wire 2.0 n'embarque AUCUNE resistance de rappel.
Le firmware arme le pull-up interne de l'ESP32 (~45 kOhm), ce qui suffit
sur un banc a cable court, mais reste fragile avec un cable long et les
parasites d'allumage du Webasto : fronts mous, erreurs CRC, et au bout
de 90 secondes sans mesure valide le chauffage est coupe par securite.

**A faire** : souder une resistance de **4,7 kOhm entre DATA et 3V3**
du connecteur UART1-OUT (DATA = IO18).

**Verification** (firmware de banc, env `crowpanel_test`) :

```bash
pio run -e crowpanel_test --target upload
```

puis envoyer `owdiag 30` sur le port serie. Comparer avant/apres :

| Mesure | Sans resistance (releve du 27/07/2026) | Avec resistance attendue |
|---|---|---|
| `remontee_200us` | 0 | **1** |
| `remontee_5ms` | 0 | 1 |
| rafale | 30/30 valides, amplitude 0,12 C | 30/30, amplitude stable |

Le critere decisif est `remontee_200us` : une resistance de 4,7 kOhm
remonte la ligne en moins d'une microseconde, le pull-up interne seul
laisse le bus a zero pendant des millisecondes.

Refaire la mesure **cable definitif installe et Webasto en marche** :
c'est la seule condition qui reproduit les parasites reels.

### 2. Resistance de rappel bas sur la commande du relais

Entre le reset de l'ESP32 et la premiere instruction du firmware, la
broche de commande du relais est en haute impedance : son etat depend
du module. Une resistance de **10 kOhm entre la broche de commande et
la masse** (pour un module actif-HIGH) garantit le relais ouvert
pendant cette fenetre, y compris lors d'un brownout.

### 3. Polarite du module relais

Le firmware suppose `RELAY_ACTIVE_HIGH = true` (`ESPBastoRelay/src/config.h`),
alors que la plupart des modules relais 5 V du commerce sont **actifs
LOW**. Une polarite inversee signifie chauffage allume au repos.

**A verifier, Webasto DEBRANCHE** : alimenter le relais seul et
observer la LED du module (ou mesurer la continuite entre COM et NO).
Au demarrage, le relais doit etre OUVERT. S'il se ferme, passer
`RELAY_ACTIVE_HIGH` a `false` et reflasher.

## Fonctionnalites

- **3 modes de chauffage** : thermostat avec hysteresis, minuteur, consigne simple
- **Ecran tactile** : interface "Muted Industrial", utilisable au doigt
- **Decouverte automatique** du relais (broadcast ESP-NOW)
- **Securite** : temperature max 40C, verrou anti-redemarrage 3 min, watchdog connexion
- **Economie d'energie** : veille ecran 60s, LVGL stoppe en veille, capteur adaptatif

## Architecture

```
src/
  hal/         Display, tactile FT6236, backlight, sonde DS18B20, batterie MAX17048
  comm/        Protocole ESP-NOW, decouverte relais, ping/retry
  core/        Machine d'etat chauffage, 3 modes, stockage NVS
  ui/          Theme LVGL, header, 6 ecrans (menu, modes, alertes, recherche)
  power/       Veille ecran, CPU 80MHz, buzzer off
```

## Build

Prerequis : [PlatformIO](https://platformio.org/)

```bash
# Compiler
pio run

# Flasher
pio run --target upload

# Moniteur serie
pio device monitor
```

## Protocole ESP-NOW

| Controleur -> Relais | Code | Relais -> Controleur | Code |
|---------------------|------|---------------------|------|
| HEAT_ON | 1 | ACK_ON | 11 |
| HEAT_OFF | 2 | ACK_OFF | 12 |
| PING | 3 | ACK_PONG | 13 |
| | | ACK_LOCKED | 14 |
| | | ACK_UNLOCKED | 15 |

## Documentation

- [specs.md](specs.md) - Specifications completes du controleur
- [specs_relay.md](specs_relay.md) - Specifications du module relais
- [ui-preview/](ui-preview/) - Mockup HTML de l'interface

## Licence

Projet personnel.
