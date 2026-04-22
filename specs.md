# ESPBasto - Spécifications Techniques

## Vue d'ensemble

Contrôle d'un chauffage Webasto via ESP-NOW avec 3 modes de fonctionnement.

**Priorité n°1** : Faible consommation électrique ⚡

> Cette version des specs est recalée sur le matériel réellement présent dans `Matériel.rtf` : **module ESP32-2432S028R** (ESP32-WROOM-32 + écran 2,8" ILI9341 + tactile résistif XPT2046).

---

## 1. Économie d'énergie

### Mise en veille écran

| Paramètre | Valeur |
|-----------|--------|
| Timeout inactivité | **60 secondes** |
| Actions surveillées | Touch écran, bouton BOOT (secours) |

**Logique :**
```
SI aucune action pendant 60s :
  → Rétroéclairage OFF (TFT_BL = GPIO21)
  → Contrôleur ILI9341 en SLPIN

SI action détectée (écran éteint) :
  → ILI9341 SLPOUT + délai ~120 ms
  → Rétroéclairage ON
  → Première action consommée (réveil uniquement)

SI action détectée (écran allumé) :
  → Action normale sur l'interface
```

> ⚠️ Le premier toucher qui réveille l'écran est **consommé** : il ne déclenche pas l'action de l'interface.

> ℹ️ Le chauffage continue de fonctionner même si l'écran est éteint.

### Autres optimisations énergétiques

| Optimisation | Remarque |
|-------------|----------|
| CPU à 80 MHz | Suffisant pour l'UI + ESP-NOW |
| Backlight OFF | Meilleur levier d'économie (~25-40 mA) |
| ILI9341 en veille (`SLPIN`) | Réduit la consommation du contrôleur TFT |
| WiFi modem sleep (`WIFI_PS_MIN_MODEM`) | `MAX_MODEM` cause des pertes de paquets ESP-NOW |
| LVGL stoppé en veille | `lv_timer_handler()` non appelé quand l'écran dort (économie CPU + SPI) |
| LVGL à 10 FPS | `LV_DISP_DEF_REFR_PERIOD = 100` (suffisant pour un thermostat) |
| Capteur adaptatif | 5s écran actif, 10s en veille + mode actif, arrêt complet sinon |
| Désactivation LED RGB | LED actives à l'état bas, forcées HIGH au démarrage |
| GPIO speaker en LOW | Pin 26 forcée en sortie basse (évite les oscillations) |
| Logs conditionnels | Macro `DEBUG_MODE` — Serial désactivable en production (~1 mA) |

**Consommation estimée :**

| État | Consommation |
|------|-------------|
| Écran actif | ~65 mA |
| Écran en veille | ~20 mA |

### Indicateur batterie (MAX17048)

Le MAX17048 est un fuel gauge I2C (même bus que l'AHT21) qui fournit
le pourcentage de charge et la tension sans calibration.

| Paramètre | Valeur |
|-----------|--------|
| Intervalle de lecture | 30 secondes |
| Affichage | `XX%` dans le header, entre la température et les icônes |
| Seuil batterie basse | 10% |
| Couleur indicateur | Gris > 20%, or brun > 10%, rouge < 10% |

---

## 2. Architecture

```
┌──────────────────────────┐        ESP-NOW         ┌─────────────────────┐
│ ESP32-2432S028R #1       │ ◄────────────────────► │      Module #2      │
│     (Contrôleur HMI)     │       (~200 m)         │   (Relais Webasto)  │
├──────────────────────────┤                        ├─────────────────────┤
│ • ESP32-WROOM-32         │                        │ • Relais 24V        │
│ • TFT ILI9341 240x320    │                        │ • Webasto           │
│ • Tactile XPT2046        │                        │                     │
│ • RGB LED                │                        │                     │
│ • LDR                    │                        │                     │
│ • µSD                    │                        │                     │
│ • HP (GPIO26)            │                        │                     │
│ • CN1/P3 pour extensions │                        │                     │
└──────────────────────────┘                        └─────────────────────┘
```

### Extensions retenues pour le projet thermostat


Pour conserver les fonctions du projet d'origine, on retient les extensions suivantes :

| Fonction projet | Matériel retenu | Interface | Statut |
|-----------------|-----------------|-----------|--------|
| Température / humidité | **AHT21 externe** | I2C sur CN1 | Recommandé |
| Navigation UI | **Écran tactile résistif** | XPT2046 |  |
| Niveau de la batterie | MAX17048 |  |  |

---

## 3. Modes de fonctionnement

### Valeurs par défaut

| Paramètre | Valeur par défaut |
|-----------|-------------------|
| Setpoint (consigne) | **20°C** |
| Hystérésis | **3°C** |
| Durée minuteur | **30 min** |

---

### Démarrage et sauvegarde

**Sauvegarde des paramètres :** Oui (`Preferences` / NVS)

| Paramètre sauvegardé |
|----------------------|
| Setpoint |
| Hystérésis |
| Durée minuteur |
| Dernier mode utilisé |
| Adresse MAC relais (découverte auto) |

**État au démarrage :**
```
Affichage : Menu principal
Chauffage : OFF (ne démarre qu'après action utilisateur)
Paramètres : Chargés depuis NVS (ou défauts si premier démarrage)
```

---

### Navigation et état du chauffage

**Règle principale :** Le bouton `RETOUR` est une **navigation pure**. Le chauffage continue de fonctionner en arrière-plan quel que soit le mode.

```
RETOUR (tous les modes) :
  → Retour au menu principal
  → Le chauffage CONTINUE de fonctionner
  → Le mode actif reste en mémoire

Menu principal (chauffage actif) :
  → Header : 🔥 ON + mode actif visible
  → Bouton ARRÊTER ajouté en bas des 3 tuiles (barre d'action basse)
  → Appui ARRÊTER → envoi HEAT_OFF → retrait du bouton
```

> ⚠️ L'envoi de `HEAT_OFF` déclenche le verrou anti-redémarrage de 3 min côté relais.

### Verrouillage interface (ACK_LOCKED)

```
SI ACK_LOCKED reçu :
  → Menu affiché normalement
  → Toutes les tuiles et boutons grisés / non cliquables
  → Cadenas 🔒 visible dans le header
  → Tout appui tactile est ignoré

SI ACK_UNLOCKED reçu :
  → Restauration visuelle complète
  → Interactions réactivées
  → Cadenas masqué
```

> ℹ️ Pendant le verrouillage, aucune action n'est possible : ni navigation, ni réglage, ni démarrage.

---

### Machine d'état du chauffage (côté contrôleur)

Le contrôleur maintient un état global du chauffage, indépendant du mode sélectionné.

**3 états :**

| État | Description | Interface |
|------|-------------|-----------|
| `IDLE` | Chauffage OFF, prêt à démarrer | Boutons actifs, pas de bouton ARRÊTER sur le menu |
| `HEATING` | Chauffage ON confirmé | 🔥 ON dans le header, bouton ARRÊTER sur le menu |
| `LOCKED` | Verrou anti-redémarrage actif | 🔒 dans le header, tout grisé, aucune interaction |

**Diagramme de transitions :**

```
                         Utilisateur / Thermostat
                         envoi HEAT_ON
                    ┌─────────────────────────┐
                    │                         │
                    │    ACK_ON reçu           ▼
              ┌───────────┐             ┌──────────┐
     ┌───────►│           │             │          │
     │        │   IDLE    │             │ HEATING  │
     │        │   (OFF)   │◄────┐       │   (ON)   │
     │        │           │     │       │          │
     │        └─────┬─────┘     │       └────┬─────┘
     │              │           │            │
     │              │           │            │ Arrêt utilisateur
     │              │      Perte connexion   │ Thermostat décide OFF
     │              │      (3 pings échoués) │ Minuteur expiré
     │              │      → retour IDLE     │ Consigne atteinte
     │              │      + alerte          │ → envoi HEAT_OFF
     │              │           │            │
     │              │ ACK_LOCKED│            │ ACK_OFF reçu
     │              │ reçu      │            │
     │              │           │  ┌─────────┘
     │              ▼           │  ▼
     │        ┌───────────┐    │
     │        │           │    │
     │        │  LOCKED   │────┘
     │        │(verrouillé│
     │        │           │
     │        └─────┬─────┘
     │              │
     │              │ ACK_UNLOCKED reçu
     └──────────────┘
```

**Gestion des erreurs de communication :**
```
Envoi HEAT_ON ou HEAT_OFF :
  → Attente ACK : 1 seconde
  → Retry : 3 tentatives espacées de 500 ms
  → Si 3 échecs → rester dans l'état actuel + alerte connexion perdue

Perte de connexion pendant HEATING (3 pings consécutifs sans PONG) :
  → Retour à IDLE (le watchdog relay coupe après 3 min)
  → Affichage alerte connexion perdue
```

> ℹ️ Ce diagramme d'état est indépendant du mode (A, B ou C). Le mode détermine **quand** envoyer HEAT_ON/OFF, la machine d'état gère **comment**.

---

### Lissage de la température (EMA)

La température lue par l'AHT21 est lissée par une moyenne mobile exponentielle pour filtrer le bruit capteur.

| Paramètre | Valeur |
|-----------|--------|
| Coefficient α | **0.1** |
| Fréquence de lecture | Toutes les **2 secondes** |
| Convergence (~95%) | ~60 secondes (30 lectures) |
| Valeur initiale | Première lecture valide du capteur |

**Formule :**
```
valeur_lissée = α × nouvelle_lecture + (1 - α) × valeur_précédente

Avec α = 0.1 :
  valeur_lissée = 0.1 × nouvelle_lecture + 0.9 × valeur_précédente
```

> ℹ️ Les décisions thermostat (modes A et C) utilisent la **valeur lissée**, évaluée toutes les **60 secondes**. Avec α = 0.1 et 30 lectures entre chaque décision, la valeur est bien stabilisée.

---

### Mode A - Thermostat avec Hystérésis

| Paramètre | Plage | Pas | Défaut |
|-----------|-------|-----|--------|
| Consigne (setpoint) | 10°C à 35°C | 0.5°C | 20°C |
| Hystérésis | 1°C à 5°C | 1°C | 3°C |

**Logique :**
```
Lecture capteur : toutes les 2 secondes (lissage EMA)
Décision thermostat : toutes les 60 secondes (valeur lissée)

SI température < (setpoint - hysteresis) → Chauffage ON
SI température >= setpoint → Chauffage OFF
SINON → Garder l'état actuel (zone morte)
```

**Exemple** (setpoint=21°C, hystérésis=3°C) :
- T < 18°C → ON
- T >= 21°C → OFF
- 18°C ≤ T < 21°C → Zone morte

---

### Mode B - Thermostat avec Minuteur

| Paramètre | Plage | Pas | Défaut |
|-----------|-------|-----|--------|
| Durée | 1 min à 120 min | 1 min | 30 min |

**Logique :**
```
Démarrage → Chauffage ON + Timer démarre
Timer > 0  → Chauffage ON
Timer = 0  → Chauffage OFF
```

> ⚠️ Pas de contrôle de température dans ce mode.

---

### Mode C - Thermostat avec Consigne

| Paramètre | Plage | Pas | Défaut |
|-----------|-------|-----|--------|
| Consigne (setpoint) | 10°C à 35°C | 0.5°C | 20°C |

**Logique :**
```
Lecture capteur : toutes les 2 secondes (lissage EMA)
Décision : toutes les 60 secondes (valeur lissée)

SI température < setpoint → Chauffage ON
SI température ≥ setpoint → Chauffage OFF (définitif)
```

> ℹ️ Une fois la consigne atteinte, le chauffage s'arrête et ne redémarre pas automatiquement.

---

## 4. Matériel - Contrôleur (#1)

### Matériel présent sur la carte ESP32-2432S028R

| Élément | Modèle / fonction | Interface |
|--------|--------------------|-----------|
| Microcontrôleur | **ESP32-WROOM-32** (double cœur) | - |
| Écran | **ILI9341** 2,8" 240x320 | SPI |
| Tactile | **XPT2046** résistif | SPI dédié |
| Rétroéclairage | TFT backlight | GPIO |
| LED RGB | Rouge / Vert / Bleu | GPIO |
| Capteur de lumière | **LDR** | ADC |
| Stockage | Lecteur **microSD / TF** | SPI |
| Audio | Sortie haut-parleur amplifiée | GPIO / PWM / DAC |
| Boutons | **BOOT** + **RST** | GPIO / reset |
| Extensions | Connecteurs **P3**, **CN1**, **P1** | GPIO / alimentation |

### Périphériques externes recommandés pour ce projet

| Périphérique | Librairie recommandée | Interface retenue | Pins retenus |
|-------------|------------------------|-------------------|--------------|
| AHT21 (temp/humidité) | `Adafruit AHTX0` | I2C | SDA=**GPIO27**, SCL=**GPIO22** |


> ℹ️ Le bus I2C externe est volontairement fixé à **CN1** avec `Wire.begin(27, 22)` pour éviter tout conflit avec le rétroéclairage sur GPIO21.

---

## 5. Bibliothèques recommandées

### Stack principale retenue

| Fonction | Librairie | Statut | Notes |
|----------|-----------|--------|-------|
| WiFi / ESP-NOW | `WiFi.h` + `ESP_NOW.h` | **Recommandé** | API officielle Arduino-ESP32 |
| Écran TFT | `TFT_eSPI` | **Recommandé** | Performant pour ILI9341 |
| Écran tactile | `XPT2046_Touchscreen` | **Recommandé** | À gérer séparément du TFT |
| NVS / paramètres | `Preferences.h` | **Recommandé** | Sauvegarde paramètres |
| I2C externe | `Wire.h` | **Recommandé** | Bus personnalisé sur CN1 |
| AHT21 externe | `Adafruit AHTX0` | **Recommandé** | Meilleur choix qu'un DHT11 pour un thermostat |
| µSD | `SD.h` | Optionnel | Logs / assets / thèmes |

### Règles d'implémentation importantes

1. **Ne pas utiliser le tactile via TFT_eSPI** sur cette carte : le XPT2046 est câblé sur un **bus SPI différent** du TFT.
2. Le TFT peut être piloté par `TFT_eSPI`.
3. Le tactile doit être piloté par `XPT2046_Touchscreen` avec son propre `SPIClass` si nécessaire.
4. Le rétroéclairage est sur **GPIO21** : ne pas réutiliser cette pin pour autre chose.
5. Les GPIO **34, 35, 36, 39** sont des entrées uniquement.
6. **Calibration tactile XPT2046 : valeurs fixes en dur.** Les constantes de calibration (min/max X et Y) sont définies dans le code après un test sur la carte. Pas d'écran de calibration dynamique.

---

## 6. Câblage / GPIO - Contrôleur (#1)

### Écran TFT ILI9341 (SPI TFT)

| Signal | GPIO | Fonction |
|--------|------|----------|
| TFT_MISO | **GPIO12** | Lecture SPI TFT |
| TFT_MOSI | **GPIO13** | Données SPI TFT |
| TFT_SCLK | **GPIO14** | Horloge SPI TFT |
| TFT_CS | **GPIO15** | Chip Select TFT |
| TFT_DC | **GPIO2** | Data / Command |
| TFT_RST | **-1** | Reset non câblé séparément |
| TFT_BL | **GPIO21** | Rétroéclairage |

### Écran tactile XPT2046 (SPI tactile dédié)

| Signal | GPIO | Fonction |
|--------|------|----------|
| XPT2046_IRQ | **GPIO36** | IRQ tactile |
| XPT2046_MOSI | **GPIO32** | Données SPI tactile |
| XPT2046_MISO | **GPIO39** | Lecture SPI tactile |
| XPT2046_CLK | **GPIO25** | Horloge SPI tactile |
| XPT2046_CS | **GPIO33** | Chip Select tactile |

### Lecteur microSD / TF

| Signal | GPIO | Fonction |
|--------|------|----------|
| SD_MISO | **GPIO19** | Lecture SPI SD |
| SD_MOSI | **GPIO23** | Données SPI SD |
| SD_SCK | **GPIO18** | Horloge SPI SD |
| SD_CS | **GPIO5** | Chip Select SD |

### LED RGB (actives à l'état bas)

| Couleur | GPIO | Note |
|---------|------|------|
| Rouge | **GPIO4** | `LOW = ON` |
| Vert | **GPIO16** | `LOW = ON` |
| Bleu | **GPIO17** | `LOW = ON` |

### Capteur de lumière / LDR

| Signal | GPIO | Note |
|--------|------|------|
| LDR | **GPIO34** | ADC, entrée uniquement |

### Haut-parleur

| Signal | GPIO | Note |
|--------|------|------|
| Speaker | **GPIO26** | Connecté à l'ampli, à réserver à l'audio |

### Boutons intégrés

| Bouton | GPIO | Note |
|--------|------|------|
| BOOT | **GPIO0** | Strapping pin, à ne pas utiliser comme contrôle principal |
| RESET | - | Reset matériel |

### Connecteurs d'extension

#### Connecteur P3

| Broche | GPIO | Note |
|--------|------|------|
| GND | - | Masse |
| IO35 | **GPIO35** | Entrée uniquement, pas de pull-up interne |
| IO22 | **GPIO22** | Disponible |
| IO21 | **GPIO21** | Déjà utilisé par le backlight |

#### Connecteur CN1

| Broche | GPIO | Note |
|--------|------|------|
| GND | - | Masse |
| IO22 | **GPIO22** | Recommandé comme SCL I2C |
| IO27 | **GPIO27** | Recommandé comme SDA I2C |
| 3V3 | - | Alimentation capteurs |

#### Connecteur P1 (série)

| Broche | GPIO | Note |
|--------|------|------|
| TX | **GPIO1** | Série |
| RX | **GPIO3** | Série |

### Bus I2C externe retenu pour le projet

| Signal I2C | GPIO | Usage |
|------------|------|-------|
| SDA | **GPIO27** | AHT21 / MAX17048 |
| SCL | **GPIO22** | AHT21 / MAX17048 |

---

## 7. Résumé des pins - Contrôleur (#1)

| GPIO | Utilisé par | Notes |
|------|-------------|-------|
| GPIO0 | BOOT | Strapping pin |
| GPIO1 | TX série | Connecteur P1 |
| GPIO2 | TFT_DC | Écran TFT |
| GPIO3 | RX série | Connecteur P1 |
| GPIO4 | LED rouge | Actif à bas |
| GPIO5 | SD_CS | Carte microSD |
| GPIO12 | TFT_MISO | SPI TFT |
| GPIO13 | TFT_MOSI | SPI TFT |
| GPIO14 | TFT_SCLK | SPI TFT |
| GPIO15 | TFT_CS | SPI TFT |
| GPIO16 | LED verte | Actif à bas |
| GPIO17 | LED bleue | Actif à bas |
| GPIO18 | SD_SCK | SPI SD |
| GPIO19 | SD_MISO | SPI SD |
| GPIO21 | TFT_BL | Backlight, ne pas réutiliser |
| GPIO22 | I2C externe SCL | CN1 / P3 |
| GPIO23 | SD_MOSI | SPI SD |
| GPIO25 | XPT2046_CLK | SPI tactile |
| GPIO26 | Speaker | Audio uniquement |
| GPIO27 | I2C externe SDA | CN1 |
| GPIO32 | XPT2046_MOSI | SPI tactile |
| GPIO33 | XPT2046_CS | SPI tactile |
| GPIO34 | LDR | ADC, entrée uniquement |
| GPIO35 | Extension P3 | Entrée uniquement |
| GPIO36 | XPT2046_IRQ | Entrée uniquement |
| GPIO39 | XPT2046_MISO | Entrée uniquement |

---

## 8. Communication ESP-NOW

| Paramètre | Valeur |
|-----------|--------|
| Protocole | ESP-NOW |
| Portée | ~200 m (extérieur) |
| Latence | ~2-3 ms |
| Timeout ACK | **1 seconde** |
| Retry ACK | **3 tentatives** espacées de 500 ms |
| WiFi power save | Modem sleep (`WIFI_PS_MIN_MODEM`) |
| Adresse MAC relais | **Découverte automatique** (sauvegardée en NVS) |

### Découverte automatique du relais

Le contrôleur découvre l'adresse MAC du relais automatiquement au premier démarrage, puis la sauvegarde en NVS.

**Séquence au démarrage :**
```
SI adresse MAC sauvegardée en NVS :
  → Envoi PING unicast à l'adresse sauvegardée
  → SI ACK_PONG reçu → Connexion établie
  → SI pas de réponse après 3 tentatives → Lancer la découverte broadcast

SI pas d'adresse sauvegardée (ou échec unicast) :
  → Envoi PING en broadcast (FF:FF:FF:FF:FF:FF)
  → Retry toutes les 5 secondes
  → Affichage "RECHERCHE RELAIS..." sur l'écran

Réception ACK_PONG :
  → Extraire l'adresse MAC de l'expéditeur
  → Enregistrer comme peer unicast
  → Sauvegarder en NVS
  → Toutes les communications suivantes en unicast
```

**Écran de recherche :**
```
┌──────────────────────────────────────┐
│ WEBASTO CTRL                         │
├──────────────────────────────────────┤
│                                      │
│        RECHERCHE RELAIS...           │
│                                      │
│           ◌  (animation)             │
│                                      │
└──────────────────────────────────────┘
```

> ℹ️ Le relais enregistre dynamiquement le premier contrôleur qui lui envoie une commande (cf. `specs_relay.md`). La découverte est donc symétrique : le contrôleur trouve le relais, le relais enregistre le contrôleur.

> ℹ️ Si le relais est remplacé physiquement (nouvelle MAC), le contrôleur échouera en unicast et relancera automatiquement la découverte broadcast.

### Messages envoyés (Contrôleur → Relais)

| Commande | Code | Description |
|----------|------|-------------|
| `HEAT_ON` | 1 | Allumer chauffage |
| `HEAT_OFF` | 2 | Éteindre chauffage |
| `PING` | 3 | Vérifier connexion / découverte |

### Messages reçus (Relais → Contrôleur)

| Réponse | Code | Description |
|---------|------|-------------|
| `ACK_ON` | 11 | Chauffage allumé confirmé |
| `ACK_OFF` | 12 | Chauffage éteint confirmé |
| `ACK_PONG` | 13 | Relais connecté |
| `ACK_LOCKED` | 14 | Interface verrouillée |
| `ACK_UNLOCKED` | 15 | Interface déverrouillée |

---

## 9. Interface utilisateur (LVGL)

### Objectif ergonomique

L'interface doit être utilisable **au doigt** sur l'écran résistif 2,8” (240x320), **sans stylet**. Elle est construite avec **LVGL** et adopte une identité visuelle **”Muted Industrial”**, inspirée du style [VolosR/WaveShareC6lvglexample](https://github.com/VolosR/WaveShareC6lvglexample) : dashboard industriel sobre, tons charbonneux, accents teal/or brun éteints, design flat sans ombres, typographie monospace.

> 📐 Mockup HTML de référence : `ui-preview/index.html`

Conséquences directes pour le design :

- **Aucun petit bouton** ni icône isolée difficile à viser
- **Grandes zones tactiles** avec espacement visible entre actions
- **Une seule action principale par écran**
- **Actions critiques en bas d'écran** avec gros boutons
- **Pas d'interaction fine** de type mini `+` / `-` trop rapprochés

---

### Direction artistique — Cockpit moderne épuré

| Élément | Direction retenue |
|---------|-------------------|
| Ambiance | **Cockpit avionique moderne / tableau de bord épuré** |
| Fond principal | **Noir pur** (`#000000`) ou noir profond (`#0A0A0F`) |
| Couleur accent primaire | **Cyan** (`#00E5FF`) — états actifs, données principales |
| Couleur accent secondaire | **Ambre** (`#FFB300`) — chauffage ON, alertes modérées |
| Couleur danger | **Rouge vif** (`#FF1744`) — erreurs, arrêt, alertes critiques |
| Couleur succès | **Vert** (`#00E676`) — connexion OK, confirmation |
| Texte principal | **Blanc** (`#FFFFFF`) à **95% opacité** |
| Texte secondaire | **Blanc** à **60% opacité** |
| Surfaces / cartes | **Gris très sombre** (`#1A1A2E`) avec `radius: 12px` |
| Boutons | Fond sombre + bordure accent 1px, `radius: 8px`, ombre légère |
| Contraste | Élevé pour lecture immédiate |

**Principes de style :**
- Rendu **propre, aéré et fonctionnel**, pas de fioritures décoratives
- Les couleurs sont **sémantiques** : chaque couleur a une signification (cyan = info, ambre = chauffage, rouge = danger)
- Les ombres (`shadow`) sont subtiles (2-4px) pour donner de la profondeur sans surcharge
- Les animations sont **courtes et fonctionnelles** (transitions 200-300ms), jamais gratuites

---

### Widgets LVGL utilisés

| Widget LVGL | Usage dans le projet |
|-------------|---------------------|
| `lv_arc` | **Jauge circulaire température** — widget central des modes A et C |
| `lv_bar` | **Barre de progression minuteur** — progression visuelle du mode B |
| `lv_btn` | Boutons d'action (`RETOUR`, `DÉMARRER`, `ARRÊTER`, `+`, `-`) |
| `lv_label` | Textes, titres, valeurs numériques |
| `lv_obj` | Conteneurs, cartes avec `radius` et `shadow` |
| `lv_anim` | Transitions entre écrans (fondu), pulsation du statut ON |

**Règles d'utilisation des widgets :**
1. `lv_arc` : mode `LV_ARC_MODE_NORMAL`, épaisseur 8-12px, couleur d'arc = cyan (température) ou ambre (chauffage ON)
2. `lv_bar` : hauteur 12px, coins arrondis, animation fluide de progression
3. `lv_btn` : toujours avec `radius: 8px`, padding interne 12px, ombre 2px
4. Transitions entre écrans : `lv_scr_load_anim()` avec `LV_SCR_LOAD_ANIM_FADE_IN`, durée 250ms

---

### Palette de couleurs par état

| État système | Couleur fond bouton | Couleur bordure | Couleur texte |
|-------------|---------------------|-----------------|---------------|
| Normal / IDLE | `#1A1A2E` | `#2A2A3E` | `#FFFFFF` 95% |
| Chauffage ON | `#1A1A2E` | `#FFB300` ambre | `#FFB300` |
| Appuyé (pressed) | accent à 30% opacité | accent plein | `#FFFFFF` |
| Désactivé / LOCKED | `#0D0D15` | `#1A1A2E` | `#FFFFFF` 30% |
| Danger / erreur | `#2D0A0A` | `#FF1744` | `#FF1744` |
| Succès | `#0A2D0A` | `#00E676` | `#00E676` |

---

### Règles d'ergonomie tactile

| Règle | Valeur / principe |
|-------|-------------------|
| Taille mini d'une cible tactile | **70 x 40 px minimum** |
| Taille recommandée boutons principaux | **200 x 50 px** |
| Espacement entre boutons | **8 à 12 px minimum** |
| Zone active | Toute la surface visuelle du bouton |
| Navigation principale | **Grosses tuiles / gros boutons** |
| Réglage de valeur | **Grand bouton `-` / valeur / grand bouton `+`** |
| Action retour | Bouton large fixe en bas gauche |
| Action principale | Bouton large accent coloré en bas droite |
| Coins arrondis boutons | **8 px** |
| Coins arrondis cartes | **12 px** |

> ℹ️ Sur écran résistif, la précision est inférieure à un smartphone. L'UI privilégie des **cibles larges, simples et bien espacées**.

---

### Structure d'écran commune

Chaque écran suit la même structure en 3 zones :

```
┌──────────────────────────────────┐
│         HEADER (30px)            │  ← fond #0A0A0F, bordure basse cyan 1px
├──────────────────────────────────┤
│                                  │
│                                  │
│       ZONE CENTRALE              │  ← fond #000000, contenu principal
│        (~230px)                  │
│                                  │
│                                  │
├──────────────────────────────────┤
│      BARRE ACTION (60px)         │  ← fond #0A0A0F, boutons d'action
└──────────────────────────────────┘
         240 x 320 px
```

---

### Contrôles

| Contrôle | Action |
|----------|--------|
| Appui sur une tuile | Entrer dans un mode (transition fondu) |
| Appui sur bouton `-` ou `+` | Modifier une valeur |
| Appui sur bouton d'action | Lancer / arrêter / valider |
| Appui sur écran d'alerte | Acquitter l'erreur |
| BOOT (matériel) | Secours / réveil uniquement |
| Appui long sur `+` / `-` | Auto-répétition après 500 ms |

> ⚠️ Le bouton BOOT est un **strapping pin**, il ne doit pas être l'élément principal d'ergonomie.

---

### Header

Bandeau fixe en haut de chaque écran (30px de haut).

**Layout :**
```
┌──────────────────────────────────┐
│ TITRE          21.5°C  🔥  📶    │
└──────────────────────────────────┘
  ← label gauche    indicateurs → 
```

| Élément | Widget LVGL | Style |
|---------|-------------|-------|
| Titre écran | `lv_label` | Blanc 95%, taille 14px, aligné gauche |
| Température | `lv_label` | Cyan `#00E5FF`, taille 14px |
| Icône chauffage | `lv_label` | Ambre `#FFB300` si ON, masqué si OFF |
| Icône connexion | `lv_label` | Vert `#00E676` si OK, rouge `#FF1744` si perdu |
| Icône verrou | `lv_label` | Rouge `#FF1744`, visible si LOCKED |

**Règle de sobriété :**
- Max **4 indicateurs** en plus du titre
- Priorité visuelle : `Erreur` > `Verrouillé` > `Connexion` > `Température`
- Séparé du contenu par une **ligne horizontale cyan** de 1px

**Logique ping :**
```
Intervalle ping : 60 secondes

SI PONG reçu → compteur échecs = 0, indicateur = vert
SI pas de PONG → compteur échecs + 1

SI compteur échecs ≥ 3 → alerte connexion perdue
```

**Logique verrouillage :**
```
SI ACK_LOCKED reçu :
  → Afficher icône verrou dans le header
  → Griser toutes les tuiles et boutons (opacité 30%)
  → Bloquer toutes les actions tactiles

SI ACK_UNLOCKED reçu :
  → Masquer verrou
  → Restaurer opacité 100%
  → Débloquer l'interface
```

---

### Écran de recherche relais

Affiché au démarrage si le relais n'est pas encore connecté.

```
┌──────────────────────────────────┐
│ WEBASTO CTRL                     │
├──────────────────────────────────┤
│                                  │
│      ┌──────────────────┐        │
│      │                  │        │
│      │    lv_spinner     │       │  ← spinner LVGL cyan, 48px
│      │                  │        │
│      └──────────────────┘        │
│                                  │
│      RECHERCHE RELAIS...         │  ← label blanc 60%, clignotant (lv_anim)
│                                  │
├──────────────────────────────────┤
│                                  │
└──────────────────────────────────┘
```

**Widget :** `lv_spinner` avec couleur cyan, vitesse 1000ms, arc 60°.

---

### Alerte connexion perdue

Écran plein écran, réveille l'écran si nécessaire.

```
┌──────────────────────────────────┐
│                                  │
│     ╔══════════════════════╗     │
│     ║                      ║     │
│     ║   CONNEXION PERDUE   ║     │  ← label rouge #FF1744, 18px
│     ║                      ║     │
│     ║  Vérifier le module  ║     │  ← label blanc 60%, 14px
│     ║  relais et la portée ║     │
│     ║  radio.              ║     │
│     ║                      ║     │
│     ╚══════════════════════╝     │  ← carte #1A1A2E, bordure rouge 2px
│                                  │
├──────────────────────────────────┤
│         [      OK      ]         │  ← bouton pleine largeur, bordure rouge
└──────────────────────────────────┘
```

**Style :** Fond noir, carte centrale avec bordure rouge 2px, ombre rouge diffuse 8px.

---

### Erreur capteur AHT21

**Logique :**
```
Lecture AHT21 : toutes les 2 secondes (lissage EMA)

SI lecture OK → Afficher température/humidité, reset timer erreur
SI échec lecture → Timer erreur + 1 min, afficher alerte

SI timer erreur < 5 min → Chauffage CONTINUE (mode dégradé)
SI timer erreur ≥ 5 min → Chauffage OFF (sécurité)
```

**Écran :** Même structure que l'alerte connexion, avec texte adapté :

```
┌──────────────────────────────────┐
│                                  │
│     ╔══════════════════════╗     │
│     ║   ERREUR CAPTEUR     ║     │  ← label rouge, 18px
│     ║                      ║     │
│     ║  Vérifier le câblage ║     │  ← label blanc 60%
│     ║  AHT21 sur CN1       ║     │
│     ║  SDA=GPIO27           ║     │
│     ║  SCL=GPIO22           ║     │
│     ╚══════════════════════╝     │
│                                  │
├──────────────────────────────────┤
│         [      OK      ]         │
└──────────────────────────────────┘
```

> ⚠️ Le chauffage continue pendant 5 min max, puis s'arrête par sécurité.

---

### Écran Menu Principal

3 tuiles pleine largeur avec coins arrondis, ombre et bordure subtile.

**État normal (chauffage OFF) :**
```
┌──────────────────────────────────┐
│ WEBASTO         21.5°C  48%  📶  │  ← header avec temp + humidité
├──────────────────────────────────┤
│                                  │
│  ╔════════════════════════════╗  │
│  ║  THERMOSTAT                ║  │  ← tuile, fond #1A1A2E
│  ║  Hystérésis auto           ║  │    bordure #2A2A3E, radius 12
│  ╚════════════════════════════╝  │    ombre 3px
│                                  │
│  ╔════════════════════════════╗  │
│  ║  MINUTEUR                  ║  │
│  ║  Durée programmée          ║  │
│  ╚════════════════════════════╝  │
│                                  │
│  ╔════════════════════════════╗  │
│  ║  CONSIGNE                  ║  │
│  ║  Température cible         ║  │
│  ╚════════════════════════════╝  │
│                                  │
└──────────────────────────────────┘
```

**Chauffage actif (ON) :**
```
┌──────────────────────────────────┐
│ WEBASTO         21.5°C  🔥  📶   │  ← icône flamme ambre pulsante
├──────────────────────────────────┤
│                                  │
│  ╔════════════════════════════╗  │
│  ║  THERMOSTAT                ║  │
│  ╚════════════════════════════╝  │
│                                  │
│  ╔════════════════════════════╗  │
│  ║  MINUTEUR                  ║  │
│  ╚════════════════════════════╝  │
│                                  │
│  ╔════════════════════════════╗  │
│  ║  CONSIGNE                  ║  │
│  ╚════════════════════════════╝  │
│                                  │
├──────────────────────────────────┤
│  [        ARRÊTER        ]       │  ← bouton bordure rouge, texte rouge
└──────────────────────────────────┘
```

**Verrouillé (ACK_LOCKED) :**
- Mêmes tuiles mais avec **opacité 30%** (`lv_obj_set_style_opa`)
- Aucun bouton d'action
- Icône verrou rouge dans le header

**Tuile — détail du widget :**

| Propriété | Valeur |
|-----------|--------|
| Widget | `lv_obj` (conteneur) + `lv_label` x2 |
| Fond | `#1A1A2E` |
| Bordure | `#2A2A3E`, 1px (normal) / accent couleur au toucher |
| Rayon | 12px |
| Ombre | 3px, noir 50% |
| Hauteur | ~70px |
| Titre | Blanc 95%, 16px |
| Sous-titre | Blanc 50%, 12px |
| Pressed | Bordure cyan `#00E5FF`, fond légèrement éclairci |

**Animation flamme (chauffage ON) :**
- `lv_anim` sur l'opacité de l'icône flamme ambre : oscillation 100%↔60%, durée 1200ms, boucle infinie

---

### Écran Mode A — Thermostat (Hystérésis)

**Widget central :** `lv_arc` affichant la température actuelle par rapport à la consigne.

```
┌──────────────────────────────────┐
│ THERMOSTAT             🔥  📶    │
├──────────────────────────────────┤
│                                  │
│         ╭───────────╮            │
│       ╱   21.5°C      ╲         │  ← lv_arc 120px, cyan
│      │    ──────────    │        │    label temp au centre, 24px
│       ╲   cible: 20°C ╱         │    label consigne 12px blanc 50%
│         ╰───────────╯            │
│                                  │
│  CONSIGNE                        │
│  [  -  ]    20.0°C    [  +  ]    │  ← boutons 60x40, valeur 18px cyan
│                                  │
│  HYSTÉRÉSIS                      │
│  [  -  ]     3°C      [  +  ]    │  ← boutons 60x40, valeur 18px
│                                  │
├──────────────────────────────────┤
│  [ RETOUR ]      [ DÉMARRER ]    │  ← DÉMARRER = bordure cyan
└──────────────────────────────────┘    ARRÊTER = bordure ambre si actif
```

**Détail de l'arc (`lv_arc`) :**

| Propriété | Valeur |
|-----------|--------|
| Diamètre | 120px |
| Épaisseur | 10px |
| Fond arc | `#1A1A2E` |
| Arc actif | Cyan `#00E5FF` (normal) / Ambre `#FFB300` (chauffage ON) |
| Plage | 10°C à 35°C |
| Valeur affichée | Température actuelle lissée (EMA) |
| Label central | Température en gros (24px), consigne en petit (12px) |

**Zone morte visualisée :** L'arc change de couleur selon la zone :
- T < (setpoint - hystérésis) → arc rouge : zone ON
- T >= setpoint → arc vert : zone OFF
- Entre les deux → arc ambre : zone morte

---

### Écran Mode B — Minuteur

**Widget central :** `lv_bar` horizontale pour la progression du minuteur.

```
┌──────────────────────────────────┐
│ MINUTEUR               🔥  📶    │
├──────────────────────────────────┤
│                                  │
│             25:42                │  ← temps restant, 32px, cyan
│                                  │
│  ┌────────────────────────────┐  │
│  │████████████░░░░░░░░░░░░░░░│  │  ← lv_bar, hauteur 12px
│  └────────────────────────────┘  │    cyan→ambre selon progression
│                                  │
│  DURÉE                           │
│  [  -  ]     30 min    [  +  ]   │  ← réglage de la durée
│                                  │
│                                  │
│                                  │
├──────────────────────────────────┤
│  [ RETOUR ]      [ DÉMARRER ]    │
└──────────────────────────────────┘
```

**Détail de la barre (`lv_bar`) :**

| Propriété | Valeur |
|-----------|--------|
| Largeur | 200px |
| Hauteur | 12px |
| Rayon | 6px (pleinement arrondi) |
| Fond | `#1A1A2E` |
| Indicateur | Cyan `#00E5FF` (>50%) → Ambre `#FFB300` (<50%) → Rouge `#FF1744` (<10%) |
| Animation | Transition fluide (`LV_ANIM_ON`) |

**Temps restant :** Label `lv_label` en 32px, format `MM:SS`, couleur cyan. Passe en ambre sous 5 min, rouge sous 1 min.

**Comportement :**
- Si le minuteur tourne, le bouton principal devient `ARRÊTER` (bordure ambre)
- `RETOUR` revient au menu sans arrêter le minuteur
- Le réglage de durée est grisé (désactivé) quand le minuteur tourne

---

### Écran Mode C — Consigne

**Widget central :** `lv_arc` similaire au mode A mais simplifié (pas d'hystérésis).

```
┌──────────────────────────────────┐
│ CONSIGNE               🔥  📶    │
├──────────────────────────────────┤
│                                  │
│         ╭───────────╮            │
│       ╱   21.5°C      ╲         │  ← lv_arc 120px
│      │    ──────────    │        │
│       ╲  cible: 25°C  ╱         │
│         ╰───────────╯            │
│                                  │
│  TEMPÉRATURE CIBLE               │
│  [  -  ]    25.0°C    [  +  ]    │
│                                  │
│                                  │
│                                  │
├──────────────────────────────────┤
│  [ RETOUR ]      [ DÉMARRER ]    │
└──────────────────────────────────┘
```

**Arc — couleur selon état :**
- T < setpoint → Cyan (chauffage en cours ou à lancer)
- T >= setpoint → Vert `#00E676` (consigne atteinte, chauffage coupé)

**Règles :**
1. `DÉMARRER` lance le mode (bordure cyan)
2. Une fois la consigne atteinte, chauffage coupé, arc passe au vert
3. `RETOUR` revient au menu sans couper le chauffage

---

### Barre d'action basse

Bandeau fixe en bas de chaque écran fonctionnel (60px de haut).

| Emplacement | Usage | Style |
|-------------|-------|-------|
| Bas gauche | `RETOUR` | Bordure `#2A2A3E`, texte blanc 70% |
| Bas droite | Action principale | Bordure accent (cyan/ambre/rouge selon contexte) |

**Propriétés communes des boutons d'action :**

| Propriété | Valeur |
|-----------|--------|
| Hauteur | 44px |
| Largeur | ~110px chacun |
| Rayon | 8px |
| Fond | `#1A1A2E` |
| Bordure | 1px, couleur selon rôle |
| Texte | 14px, centré, capitales |
| Espacement entre boutons | 10px |

---

### États visuels des boutons

| État | Fond | Bordure | Texte | Ombre |
|------|------|---------|-------|-------|
| Normal | `#1A1A2E` | `#2A2A3E` 1px | Blanc 95% | 2px noir 30% |
| Pressed | accent 20% opacité | accent 100% | Blanc 100% | 0px |
| Désactivé (LOCKED) | `#0D0D15` | `#1A1A2E` | Blanc 30% | 0px |
| DÉMARRER | `#1A1A2E` | Cyan `#00E5FF` | Cyan | 2px |
| ARRÊTER | `#1A1A2E` | Ambre `#FFB300` | Ambre | 2px |
| ARRÊTER (menu) | `#1A1A2E` | Rouge `#FF1744` | Rouge | 2px |
| OK (erreur) | `#1A1A2E` | Rouge `#FF1744` | Rouge | 2px |

---

### Animations LVGL

| Animation | Widget | Propriété | Durée | Type |
|-----------|--------|-----------|-------|------|
| Transition écran | `lv_scr_load_anim` | Fondu | 250ms | Aller simple |
| Flamme chauffage ON | `lv_label` (icône) | Opacité 100%↔60% | 1200ms | Boucle infinie |
| Barre minuteur | `lv_bar` | Valeur | 1000ms | Linéaire, continu |
| Pression bouton | `lv_btn` | Fond + bordure | 100ms | Aller simple |
| Apparition bouton ARRÊTER | `lv_obj` | Opacité 0%→100% | 200ms | Aller simple |
| Grisage LOCKED | Tous widgets | Opacité → 30% | 300ms | Aller simple |

> ℹ️ Les animations doivent rester **courtes et fonctionnelles**. Pas d'effets de rebond, de glissement ou d'élasticité.

---

### Comportement tactile

| Cas | Comportement |
|-----|--------------|
| Appui valide | Feedback visuel immédiat (état pressed 100ms) |
| Premier appui après réveil écran | Consommé, aucune action fonctionnelle |
| Appui hors zone | Ignoré |
| Double appui involontaire | Anti-rebond logiciel ~150 ms |
| Appui long sur `+` / `-` | Auto-répétition après 500 ms |

---

### Éléments à éviter

- Petits boutons de moins de 40px de haut
- Icônes seules comme unique cible tactile
- Paramètres sur plusieurs colonnes serrées
- Scroll vertical
- Dégradés complexes ou textures lourdes (performance ILI9341)
- Animations longues (>500ms) ou décoratives
- Plus de 2 niveaux d'ombre empilés

---

### Diagramme de navigation

```
                         ┌──────────────┐
                    ┌────│  RECHERCHE   │
                    │    │   RELAIS     │
                    │    └──────────────┘
                    │ Connexion OK
                    ▼
              ┌─────────────┐
              │    MENU     │ ◄──── RETOUR (tous modes)
              │  PRINCIPAL  │
              └──────┬──────┘
                     │
         ┌───────────┼───────────────┐
         │           │               │
         ▼           ▼               ▼
  ┌────────────┐ ┌──────────┐ ┌──────────┐
  │ THERMOSTAT │ │ MINUTEUR │ │ CONSIGNE │
  │    (A)     │ │    (B)   │ │    (C)   │
  └────────────┘ └──────────┘ └──────────┘

  Écrans superposés (prioritaires) :
  ┌──────────────┐  ┌──────────────┐
  │   ALERTE     │  │   ERREUR     │
  │  CONNEXION   │  │  CAPTEUR     │
  └──────────────┘  └──────────────┘
```

---

### Résumé UX retenu

L'interface finale doit respecter les principes suivants :

- **Pilotable au doigt** sur écran résistif
- **Simple à comprendre en quelques secondes**
- **Sans petites cibles tactiles**
- **Avec gros boutons homogènes à coins arrondis**
- **Avec une action principale évidente par écran**
- **Avec une identité cockpit moderne épuré** : noir, cyan, ambre
- **Avec des widgets LVGL natifs** : arcs, barres, animations
- **Compatible avec un usage debout, mobile, ou en environnement froid**


## 10. Notes ESP32-2432S028R / PlatformIO

### Configuration PlatformIO recommandée

```ini
[env:cyd]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
    adafruit/Adafruit AHTX0
    adafruit/Adafruit MAX1704X

build_flags =
    -DUSER_SETUP_LOADED
    -DUSE_HSPI_PORT
    -DILI9341_DRIVER
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1
    -DTFT_BL=21
    -DTFT_BACKLIGHT_ON=HIGH
    -DSPI_FREQUENCY=40000000
    -DSPI_READ_FREQUENCY=20000000
    -DSPI_TOUCH_FREQUENCY=2500000
```

### Particularités importantes

| Élément | Note |
|---------|------|
| **Board PlatformIO** | Utiliser `esp32dev` par défaut pour compatibilité |
| **ESP32** | ESP32-WROOM-32 classique, pas ESP32-C3 |
| **GPIO21** | Réservé au rétroéclairage TFT |
| **Tactile** | Sur bus SPI séparé du TFT |
| **GPIO34/35/36/39** | Entrées uniquement |
| **GPIO0** | Bouton BOOT, pin de strapping |
| **I2C externe** | Utiliser `Wire.begin(27, 22)` |
| **LED RGB** | Actives à l'état bas |
| **TFT_RST** | Non câblé séparément (`-1`) |

---

## 11. Sécurité

### Protections implémentées

| Protection | Mécanisme | Fichier |
|-----------|-----------|---------|
| **Température max absolue** | Arrêt forcé du chauffage si T >= 40°C | `heater_fsm.cpp` |
| **Erreur capteur critique** | Arrêt forcé après 5 min sans lecture valide | `heater_fsm.cpp` |
| **Verrou anti-redémarrage** | 3 min d'attente après chaque arrêt (côté relay) | `specs_relay.md` |
| **Race conditions** | Queue FreeRTOS entre callback WiFi et loop() | `heater_fsm.cpp`, `relay_link.cpp` |
| **Modes mutuellement exclusifs** | Garde dans loop() : si >1 mode actif → arrêt de tous | `main.cpp` |
| **Validation NVS** | `constrain()` dans tous les setters (valeurs corrompues) | `mode_*.cpp` |
| **Capteur non prêt** | Démarrage chauffage interdit avant 1ère lecture valide | `heater_fsm.cpp` |
| **Perte connexion** | Détection après 2 pings (2 min), relay watchdog à 3 min | `relay_link.cpp` |
| **Perte connexion pendant HEATING** | Transition vers LOCKED (pas IDLE) | `heater_fsm.cpp` |
| **Stop = arrêt chauffage** | `thermostat_stop()` et `setpoint_stop()` envoient HEAT_OFF | `mode_*.cpp` |

### Watchdog relay (côté relais)

Si le relais ne reçoit aucun ping pendant 3 minutes, il coupe le chauffage automatiquement. Le contrôleur détecte la perte après 2 minutes (2 pings échoués) et passe en état LOCKED.

---

## 12. Historique des versions

| Version | Date | Modifications |
|---------|------|---------------|
| 0.1.0 | Avril 2026 | Spécifications initiales : architecture, 3 modes, GPIO, UI rétro-futuriste |
| 0.2.0 | Avril 2026 | Ajout machine d'état chauffage, navigation sans arrêt, verrouillage ACK_LOCKED, lissage EMA α=0.1, découverte automatique du relais |
| 0.3.0 | Avril 2026 | Thème "Muted Industrial", audit sécurité (10 corrections), optimisations batterie, indicateur batterie MAX17048 |
