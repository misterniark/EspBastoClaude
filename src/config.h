/**
 * @file config.h
 * @brief Constantes globales du projet ESPBasto
 *
 * Centralise toutes les constantes matérielles (pins GPIO),
 * les paramètres de timing, les couleurs UI et les valeurs par défaut.
 *
 * Deux variantes matérielles sont supportées, sélectionnées à la
 * compilation via l'environnement PlatformIO :
 *
 *   - CYD (défaut, env:cyd) : ESP32-2432S028R
 *       écran ILI9341 + tactile résistif XPT2046 + capteur AHT21 (I2C)
 *
 *   - CROWPANEL (env:crowpanel, drapeau -DHW_CROWPANEL) :
 *       CrowPanel Advance 2.8" HMI (ESP32-S3)
 *       écran ST7789 + tactile capacitif FT6236 (I2C) +
 *       sonde étanche DS18B20 (OneWire, port UART1-OUT)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

/* ==========================================
 * Mode debug — Logs série
 *
 * Commenter la ligne suivante pour désactiver
 * tous les Serial.print en production et
 * économiser ~1-2 mA (UART désactivé).
 * ========================================== */
#define DEBUG_MODE

#ifdef DEBUG_MODE
  #define LOG_INIT()     Serial.begin(115200)
  #define LOG(fmt, ...)  Serial.printf(fmt "\n", ##__VA_ARGS__)
  #define LOGN(msg)      Serial.println(msg)
#else
  #define LOG_INIT()     ((void)0)
  #define LOG(fmt, ...)  ((void)0)
  #define LOGN(msg)      ((void)0)
#endif

#ifdef HW_CROWPANEL

/* ##########################################################
 * ###   Matériel : CrowPanel Advance 2.8" (ESP32-S3)     ###
 * ##########################################################
 * Source : wiki Elecrow + code d'usine (LovyanGFX_Driver.h)
 * https://www.elecrow.com/pub/wiki/CrowPanel_Advance_2.8-HMI_ESP32_AI_Display.html
 */

/* Nom du capteur de température (pour les logs) */
#define SENSOR_NAME "DS18B20"

/* ==========================================
 * GPIO — Écran TFT ST7789 (SPI2/FSPI)
 * ========================================== */
/* Note : les pins TFT (MOSI=39, SCLK=42, CS=40, DC=41, RST=-1) sont
 * configurées via build_flags dans platformio.ini pour TFT_eSPI. */
constexpr int PIN_TFT_BL    = 38;   /* Rétroéclairage (HIGH = ON) */

/* Rotation TFT_eSPI : 0 = portrait 240x320 (connecteurs vers le bas).
 * Passer à 2 si l'image apparaît à l'envers dans le boîtier. */
constexpr int TFT_ROTATION  = 0;

/* ==========================================
 * GPIO — Tactile capacitif FT6236 (I2C, adresse 0x38)
 * Bus I2C partagé avec le connecteur externe I2C-OUT.
 * ========================================== */
constexpr int     PIN_TOUCH_SDA   = 15;
constexpr int     PIN_TOUCH_SCL   = 16;
constexpr int     PIN_TOUCH_INT   = 47;   /* INT actif bas (non utilisé, polling I2C) */
constexpr uint8_t FT6X36_I2C_ADDR = 0x38;

/* Transformation des coordonnées tactiles brutes vers l'écran.
 * Le FT6236 rapporte le repère PHYSIQUE de la dalle (portrait
 * 240x320), indépendant de la rotation d'affichage (MADCTL) :
 * les miroirs sont donc dérivés de TFT_ROTATION pour rester
 * cohérents si on retourne l'affichage (rotation 2 = 180° →
 * miroir des deux axes).
 * À valider sur le matériel réel : si le toucher est inversé
 * alors que l'affichage est correct, ajuster ces expressions
 * (voir hal/touch_mapping.h). */
constexpr bool TOUCH_SWAP_XY  = false;
constexpr bool TOUCH_MIRROR_X = (TFT_ROTATION == 2);
constexpr bool TOUCH_MIRROR_Y = (TFT_ROTATION == 2);

/* ==========================================
 * GPIO — I2C externe (connecteur I2C-OUT, HY2.0-4P)
 * Même bus physique que le tactile : SDA=15, SCL=16.
 * C'est ici que peut se brancher la jauge MAX17048 (optionnelle).
 * ========================================== */
constexpr int PIN_I2C_SDA = 15;
constexpr int PIN_I2C_SCL = 16;

/* ==========================================
 * GPIO — Sonde DS18B20 (OneWire)
 *
 * La sonde Crowtail One Wire Waterproof 2.0 se branche sur le
 * connecteur UART1-OUT (HY2.0-4P). La broche signal du câble
 * Crowtail correspond à RX = IO18 côté ESP32-S3 (vérifié sur
 * matériel : ROM 28085C8700164F15 détectée sur IO18).
 *
 * ATTENTION : le module Crowtail n'intègre PAS de résistance de
 * rappel — sans le pull-up interne armé par ds_probe(), le bus
 * flotte à 0 et la sonde est indétectable.
 * ========================================== */
constexpr int PIN_ONEWIRE = 18;

/* ==========================================
 * GPIO — Autres périphériques CrowPanel
 * ========================================== */
constexpr int PIN_BUZZER = 8;    /* Buzzer passif — forcé LOW au boot */

#else /* ================== CYD (ESP32-2432S028R) ==================
       *
       * ⚠️ CIBLE NON MAINTENUE depuis le 27/07/2026 — conservée pour
       * référence, plus testée sur matériel, ne pas mettre en service.
       * Défaut connu non corrigé : anti-rebond tactile XPT2046 (voir
       * hal/touchpad.cpp et le bandeau de platformio.ini).
       * ============================================================ */

/* Nom du capteur de température (pour les logs) */
#define SENSOR_NAME "AHT21"

/* ==========================================
 * GPIO — Écran TFT ILI9341 (SPI HSPI)
 * ========================================== */
/* Note : les pins TFT sont configurées via build_flags dans platformio.ini
 * pour TFT_eSPI. Elles sont redéfinies ici pour référence uniquement. */
constexpr int PIN_TFT_BL    = 21;   /* Rétroéclairage (HIGH = ON) */

/* Rotation TFT_eSPI : 0 = portrait 240x320 */
constexpr int TFT_ROTATION  = 0;

/* ==========================================
 * GPIO — Écran tactile XPT2046 (SPI VSPI dédié)
 * ========================================== */
constexpr int PIN_TOUCH_IRQ  = 36;
constexpr int PIN_TOUCH_MOSI = 32;
constexpr int PIN_TOUCH_MISO = 39;
constexpr int PIN_TOUCH_CLK  = 25;
constexpr int PIN_TOUCH_CS   = 33;

/* ==========================================
 * Calibration tactile XPT2046
 * Valeurs typiques pour ESP32-2432S028R (CYD).
 * À ajuster sur le matériel réel en lisant les valeurs brutes.
 * ========================================== */
constexpr int TS_MINX = 200;
constexpr int TS_MAXX = 3700;
constexpr int TS_MINY = 300;
constexpr int TS_MAXY = 3800;

/* ==========================================
 * GPIO — I2C externe (connecteur CN1)
 * ========================================== */
constexpr int PIN_I2C_SDA = 27;
constexpr int PIN_I2C_SCL = 22;

/* ==========================================
 * GPIO — LED RGB (actives à l'état bas)
 * ========================================== */
constexpr int PIN_LED_RED   = 4;
constexpr int PIN_LED_GREEN = 16;
constexpr int PIN_LED_BLUE  = 17;

/* ==========================================
 * GPIO — Autres
 * ========================================== */
constexpr int PIN_LDR     = 34;    /* Capteur de lumière (ADC) */
constexpr int PIN_SPEAKER = 26;    /* Haut-parleur */
constexpr int PIN_BOOT    = 0;     /* Bouton BOOT (strapping pin) */

#endif /* HW_CROWPANEL */

/* ==========================================
 * Écran — Dimensions
 * ========================================== */
constexpr int SCREEN_WIDTH  = 240;
constexpr int SCREEN_HEIGHT = 320;

/* ==========================================
 * UI — Dimensions des zones d'écran
 * ========================================== */
constexpr int HEADER_HEIGHT     = 30;
constexpr int ACTION_BAR_HEIGHT = 60;
constexpr int CONTENT_HEIGHT    = SCREEN_HEIGHT - HEADER_HEIGHT - ACTION_BAR_HEIGHT;

/* ==========================================
 * UI — Couleurs "Muted Industrial" (format 0xRRGGBB)
 *
 * Thème : dashboard industriel sobre
 * Fond charbon chaud, panneaux sombres, accents teal/or brun éteints.
 * Design flat, pas d'ombres, coins quasi-carrés.
 * ========================================== */
constexpr uint32_t COLOR_BG        = 0x272626;   /* Fond principal charbon chaud */
constexpr uint32_t COLOR_BG_HEADER = 0x2F2E2E;   /* Fond header/panneaux */
constexpr uint32_t COLOR_CARD      = 0x2F2E2E;   /* Fond des cartes/tuiles */
constexpr uint32_t COLOR_BORDER    = 0x3A3939;   /* Bordure/séparateurs */

constexpr uint32_t COLOR_TEAL      = 0x5A9B9B;   /* Accent primaire — teal éteint */
constexpr uint32_t COLOR_TEAL_DARK = 0x223B3B;   /* Accent teal sombre (panneaux, boutons start) */
constexpr uint32_t COLOR_WARM      = 0x906D0C;   /* Accent secondaire — or brun (chauffage ON) */
constexpr uint32_t COLOR_RED       = 0x8B3A3A;   /* Danger — rouge éteint */
constexpr uint32_t COLOR_GREEN     = 0x3A6B5A;   /* Succès — vert éteint */

constexpr uint32_t COLOR_TEXT      = 0xC8D0D0;   /* Texte principal (gris clair) */
constexpr uint32_t COLOR_TEXT_DIM  = 0x8A9A9A;   /* Texte secondaire */
constexpr uint32_t COLOR_TEXT_LABEL= 0x93ADAE;   /* Labels/titres tertiaires */

/* ==========================================
 * UI — Dimensions des widgets
 * ========================================== */
constexpr int BTN_HEIGHT      = 44;     /* Hauteur bouton d'action */
constexpr int BTN_RADIUS      = 3;      /* Rayon de coin des boutons (quasi-carré) */
constexpr int CARD_RADIUS     = 3;      /* Rayon de coin des cartes (quasi-carré) */
constexpr int TILE_HEIGHT     = 62;     /* Hauteur des tuiles menu */
constexpr int BTN_SPACING     = 8;      /* Espacement entre boutons */
constexpr int ARC_SIZE        = 130;    /* Diamètre de la jauge arc */
constexpr int ARC_WIDTH       = 8;      /* Épaisseur de l'arc */
constexpr int BAR_HEIGHT      = 6;      /* Hauteur barre de progression */
constexpr int BAR_WIDTH       = 200;    /* Largeur barre de progression */
constexpr int BAR_RADIUS      = 0;      /* Barres angulaires (pas d'arrondi) */
constexpr int SHADOW_WIDTH    = 0;      /* Pas d'ombre — design flat */

/* ==========================================
 * UI — Animations (durées en ms)
 * ========================================== */
constexpr int ANIM_SCREEN_FADE  = 250;   /* Transition entre écrans */
constexpr int ANIM_PRESS        = 100;   /* Feedback appui bouton */
constexpr int ANIM_FLAME        = 1200;  /* Pulsation icône flamme */
constexpr int ANIM_APPEAR       = 200;   /* Apparition bouton ARRÊTER */
constexpr int ANIM_LOCK         = 300;   /* Grisage verrouillage */

/* ==========================================
 * UI — Opacités
 * ========================================== */
constexpr int OPA_FULL          = 255;   /* 100% */
constexpr int OPA_TEXT_PRIMARY  = 242;   /* ~95% */
constexpr int OPA_TEXT_DIM      = 153;   /* ~60% */
constexpr int OPA_DISABLED      = 76;    /* ~30% */
constexpr int OPA_FLAME_LOW     = 153;   /* ~60% pour pulsation flamme */
constexpr int OPA_PRESSED_BG    = 76;    /* ~30% fond bouton appuyé */

/* ==========================================
 * Capteur de température (commun aux deux variantes)
 * ========================================== */
constexpr float SENSOR_EMA_ALPHA      = 0.1f;     /* Coefficient lissage EMA */
/* Intervalle par défaut, utilisé seulement si sensor_update() est appelé
 * avec 0. En pratique la boucle principale impose toujours son propre
 * intervalle : 5 s écran allumé, 10 s en veille avec mode actif. */
constexpr unsigned long SENSOR_READ_INTERVAL_MS = 2000;
constexpr unsigned long SENSOR_ERROR_TIMEOUT_MS = 300000; /* 5 min avant arrêt sécurité */

/* Âge maximal de la dernière lecture VALIDE pour autoriser le démarrage
 * d'un mode de chauffage piloté par la température (thermostat, consigne).
 * En veille sans mode actif, le capteur n'est plus lu du tout : au réveil,
 * la valeur peut dater de plusieurs heures. 60 s couvre largement les
 * intervalles de lecture normaux (5 s écran allumé, 10 s en veille avec
 * mode actif) et la conversion asynchrone du DS18B20 (~800 ms) déclenchée
 * au réveil, tout en refusant les valeurs gelées de la veille. */
constexpr unsigned long SENSOR_MAX_AGE_BEFORE_START_MS = 60000;

/* DS18B20 uniquement (CrowPanel) : budget de conversion 12 bits.
 * La lecture est asynchrone : on lance la conversion, puis on relit
 * le résultat au passage suivant de la boucle, après ce délai.
 * 800 ms = tCONV max datasheet (750 ms) + marge pour les sondes en
 * limite de spécification et les clones. */
constexpr unsigned long DS18B20_CONVERSION_MS = 800;

/* ==========================================
 * Sécurité — Température maximale absolue
 * Coupe le chauffage quel que soit le mode si dépassée.
 * ========================================== */
constexpr float TEMP_SAFETY_MAX = 40.0f;  /* °C — seuil de sécurité absolu */

/* ==========================================
 * Modes — Valeurs par défaut
 * ========================================== */
constexpr float DEFAULT_SETPOINT     = 20.0f;  /* Consigne par défaut (°C) */
constexpr int   DEFAULT_HYSTERESIS   = 3;      /* Hystérésis par défaut (°C) */
constexpr int   DEFAULT_TIMER_MIN    = 30;     /* Durée minuteur (minutes) */

/* Plages de réglage */
constexpr float SETPOINT_MIN = 10.0f;
constexpr float SETPOINT_MAX = 35.0f;
constexpr float SETPOINT_STEP = 0.5f;

constexpr int HYSTERESIS_MIN  = 1;
constexpr int HYSTERESIS_MAX  = 5;
constexpr int HYSTERESIS_STEP = 1;

constexpr int TIMER_MIN_MIN  = 1;
constexpr int TIMER_MIN_MAX  = 120;
constexpr int TIMER_MIN_STEP = 1;

/* NOTE : l'ancien intervalle de décision de 60 s
 * (THERMOSTAT_DECISION_INTERVAL_MS) a été supprimé : les modes
 * thermostat et consigne décident désormais à chaque nouvelle lecture
 * du capteur. L'anti-cyclage du relais est garanti par la bande
 * d'hystérésis (thermostat) et par le caractère définitif de la
 * coupure (consigne), plus le verrou 3 min côté relais. */

/* Filet de sécurité de l'état LOCKED : si l'ACK_UNLOCKED du relais
 * n'arrive jamais (perte radio, ou verrou jamais armé côté relais),
 * le contrôleur revient en IDLE après ce délai — choisi supérieur au
 * verrou nominal du relais (3 min) pour ne jamais le court-circuiter. */
constexpr unsigned long HEATER_LOCKED_FAILSAFE_MS = 210000; /* 3 min 30 */

/* ==========================================
 * Communication ESP-NOW
 * ========================================== */
constexpr unsigned long ESPNOW_ACK_TIMEOUT_MS   = 1000;  /* Timeout ACK */
constexpr int           ESPNOW_MAX_RETRIES      = 3;     /* Nombre de tentatives */
constexpr unsigned long ESPNOW_RETRY_DELAY_MS   = 500;   /* Délai entre tentatives */
constexpr unsigned long ESPNOW_PING_INTERVAL_MS = 60000; /* Ping toutes les 60s */
constexpr int           ESPNOW_MAX_PING_FAILS   = 2;     /* Alerter après 2 échecs (2 min, avant le watchdog relay de 3 min) */
constexpr unsigned long ESPNOW_DISCOVERY_INTERVAL_MS = 5000; /* Retry découverte */

/* ==========================================
 * Économie d'énergie
 * ========================================== */
constexpr int           CPU_FREQ_MHZ           = 80;     /* Fréquence CPU réduite */
constexpr unsigned long SCREEN_TIMEOUT_MS      = 60000;  /* Veille écran après 60s */
constexpr unsigned long DISPLAY_WAKE_DELAY_MS  = 120;    /* Délai après SLPOUT */

/* ==========================================
 * Tactile — Anti-rebond
 * ========================================== */
constexpr unsigned long TOUCH_DEBOUNCE_MS      = 50;     /* Anti-rebond tactile (réduit pour réactivité) */
constexpr unsigned long TOUCH_LONG_PRESS_MS    = 500;    /* Seuil appui long */

/* ==========================================
 * NVS — Clés de stockage
 * ========================================== */
#define NVS_NAMESPACE     "espbasto"
#define NVS_KEY_SETPOINT  "setpoint"
#define NVS_KEY_HYST      "hysteresis"
#define NVS_KEY_TIMER     "timer_min"
#define NVS_KEY_LAST_MODE "last_mode"
#define NVS_KEY_RELAY_MAC "relay_mac"

/* ==========================================
 * LVGL — Buffer d'affichage
 * ========================================== */
/* Buffer de 240 x 40 lignes = 19200 pixels (38400 octets en 16 bits) */
constexpr int LV_BUF_LINES = 40;

#endif /* CONFIG_H */
