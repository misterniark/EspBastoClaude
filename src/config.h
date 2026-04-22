/**
 * @file config.h
 * @brief Constantes globales du projet ESPBasto
 *
 * Centralise toutes les constantes matérielles (pins GPIO),
 * les paramètres de timing, les couleurs UI et les valeurs par défaut.
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

/* ==========================================
 * GPIO — Écran TFT ILI9341 (SPI HSPI)
 * ========================================== */
/* Note : les pins TFT sont configurées via build_flags dans platformio.ini
 * pour TFT_eSPI. Elles sont redéfinies ici pour référence uniquement. */
constexpr int PIN_TFT_BL    = 21;   /* Rétroéclairage (HIGH = ON) */

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
 * Capteur — AHT21
 * ========================================== */
constexpr float SENSOR_EMA_ALPHA      = 0.1f;     /* Coefficient lissage EMA */
constexpr unsigned long SENSOR_READ_INTERVAL_MS = 2000;  /* Lecture toutes les 2s */
constexpr unsigned long SENSOR_ERROR_TIMEOUT_MS = 300000; /* 5 min avant arrêt sécurité */

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

/* Intervalle de décision thermostat */
constexpr unsigned long THERMOSTAT_DECISION_INTERVAL_MS = 60000; /* 60s */

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
