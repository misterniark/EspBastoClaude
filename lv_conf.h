/**
 * @file lv_conf.h
 * @brief Configuration LVGL 8.x pour ESPBasto (ESP32-2432S028R)
 *
 * Écran ILI9341 240x320 en mode portrait.
 * Optimisé pour faible consommation mémoire sur ESP32-WROOM-32.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ==========================================
 * COULEUR
 * ========================================== */

/* Profondeur de couleur : 16 bits (RGB565), natif ILI9341 */
#define LV_COLOR_DEPTH 16

/* Ordre des octets : big-endian pour SPI vers ILI9341 */
#define LV_COLOR_16_SWAP 1

/* Couleur de transparence (chroma key) */
#define LV_COLOR_CHROMA_KEYED 0

/* ==========================================
 * MÉMOIRE
 * ========================================== */

/* Taille du heap LVGL interne (48 KB) */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)

/* Alignement mémoire */
#define LV_MEM_ADR 0
#define LV_MEM_BUF_MAX_NUM 16

/* ==========================================
 * HAL (Hardware Abstraction Layer)
 * ========================================== */

/* Utiliser millis() d'Arduino comme source de tick */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Fréquence de rafraîchissement du handler (en ms).
 * 100ms = 10 FPS : suffisant pour un contrôleur de chauffage.
 * Économie CPU significative par rapport à 30 FPS (33ms). */
#define LV_DISP_DEF_REFR_PERIOD 100

/* Période de lecture du touchpad (en ms).
 * 20ms pour un tactile résistif réactif. */
#define LV_INDEV_DEF_READ_PERIOD 20

/* ==========================================
 * DESSIN / RENDU
 * ========================================== */

/* Taille max du buffer de dessin temporaire (en octets) */
#define LV_DISP_ROT_MAX_BUF (10U * 1024U)

/* Anti-aliasing pour un rendu plus lisse */
#define LV_DRAW_COMPLEX 1

/* Nombre max de masques de dessin simultanés */
#define LV_DRAW_SW_SHADOW_CACHE_SIZE 0
#define LV_DRAW_SW_COMPLEX 1

/* ==========================================
 * POLICES
 * ========================================== */

/* Polices Montserrat intégrées — tailles utilisées dans l'UI */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1   /* Sous-titres, labels secondaires */
#define LV_FONT_MONTSERRAT_14 1   /* Texte courant, header */
#define LV_FONT_MONTSERRAT_16 1   /* Titres de tuiles */
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1   /* Température dans l'arc */
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 1   /* Minuteur MM:SS */
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* Polices spéciales */
#define LV_FONT_MONTSERRAT_12_SUBPX 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK 0
#define LV_FONT_UNSCII_8 0
#define LV_FONT_UNSCII_16 0

/* Police par défaut */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Activer le formatage de texte (retour à la ligne, etc.) */
#define LV_USE_FONT_PLACEHOLDER 1

/* ==========================================
 * WIDGETS ACTIVÉS
 * ========================================== */

/* Widgets de base */
#define LV_USE_ARC        1   /* Jauge température (modes A et C) */
#define LV_USE_BAR        1   /* Barre progression minuteur */
#define LV_USE_BTN        1   /* Boutons d'action */
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        0
#define LV_USE_LABEL      1   /* Textes et valeurs */
#define LV_USE_LINE       1   /* Séparateur header */
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SWITCH     0
#define LV_USE_TABLE      0
#define LV_USE_TEXTAREA   0

/* Widgets extra */
#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1   /* Animation recherche relais */
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/* ==========================================
 * THÈME
 * ========================================== */

/* Thème par défaut LVGL (on le personnalise par-dessus) */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 0
#define LV_THEME_DEFAULT_TRANSITION_TIME 200

/* Thème monochromatique désactivé */
#define LV_USE_THEME_MONO 0
#define LV_USE_THEME_BASIC 0

/* ==========================================
 * ANIMATIONS
 * ========================================== */

#define LV_USE_ANIMATION 1

/* ==========================================
 * LOGS (désactivé en production)
 * ========================================== */

#define LV_USE_LOG 0

/* ==========================================
 * DIVERS
 * ========================================== */

/* Nombre max de groupes (pour navigation clavier — pas utilisé ici) */
#define LV_USE_GROUP 0

/* GPU — pas de GPU sur ESP32, tout en software */
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL 0

/* Snapshot */
#define LV_USE_SNAPSHOT 0

/* File system — pas utilisé pour l'instant */
#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_FATFS 0

/* Décodeur d'images PNG/BMP/GIF — pas utilisé */
#define LV_USE_PNG 0
#define LV_USE_BMP 0
#define LV_USE_SJPG 0
#define LV_USE_GIF 0
#define LV_USE_QRCODE 0
#define LV_USE_FREETYPE 0
#define LV_USE_RLOTTIE 0
#define LV_USE_FFMPEG 0

#endif /* LV_CONF_H */
