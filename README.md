# ESPBasto - Controleur Webasto

Controleur de chauffage Webasto pour van amenage, avec ecran tactile et communication sans fil.

## Materiel

Deux variantes de controleur sont supportees (selection a la compilation
via l'environnement PlatformIO) :

### Variante CYD (`pio run -e cyd`)

| Composant | Reference |
|-----------|-----------|
| Controleur | ESP32-2432S028R (ESP32 + ecran 2.8" ILI9341 + tactile resistif XPT2046) |
| Capteur | AHT21 (temperature + humidite, I2C sur CN1 : SDA=27, SCL=22) |
| Batterie | MAX17048 (jauge I2C, optionnelle) |

### Variante CrowPanel (`pio run -e crowpanel`)

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

### Commun aux deux variantes

| Composant | Reference |
|-----------|-----------|
| Relais | Seeed XIAO ESP32-C3 (projet separe : [ESPBastoRelay](../ESPBastoRelay)) |
| Communication | ESP-NOW (~200m, decouverte automatique) |

## Fonctionnalites

- **3 modes de chauffage** : thermostat avec hysteresis, minuteur, consigne simple
- **Ecran tactile** : interface "Muted Industrial", utilisable au doigt
- **Decouverte automatique** du relais (broadcast ESP-NOW)
- **Securite** : temperature max 40C, verrou anti-redemarrage 3 min, watchdog connexion
- **Economie d'energie** : veille ecran 60s, LVGL stoppe en veille, capteur adaptatif

## Architecture

```
src/
  hal/         Display, tactile, backlight, capteur AHT21, batterie MAX17048
  comm/        Protocole ESP-NOW, decouverte relais, ping/retry
  core/        Machine d'etat chauffage, 3 modes, stockage NVS
  ui/          Theme LVGL, header, 6 ecrans (menu, modes, alertes, recherche)
  power/       Veille ecran, CPU 80MHz, LEDs off
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
