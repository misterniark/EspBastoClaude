# ESPBasto - Controleur Webasto

Controleur de chauffage Webasto pour van amenage, avec ecran tactile et communication sans fil.

## Materiel

| Composant | Reference |
|-----------|-----------|
| Controleur | ESP32-2432S028R (ESP32 + ecran 2.8" ILI9341 + tactile XPT2046) |
| Relais | Seeed XIAO ESP32-C3 (projet separe : [ESPBastoRelay](../ESPBastoRelay)) |
| Capteur | AHT21 (temperature + humidite, I2C) |
| Batterie | MAX17048 (jauge I2C) |
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
