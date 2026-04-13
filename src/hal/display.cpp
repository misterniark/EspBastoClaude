/**
 * @file display.cpp
 * @brief Implémentation du driver d'affichage TFT ILI9341 pour LVGL
 *
 * Configure TFT_eSPI en mode portrait (240x320) et enregistre
 * le callback flush pour que LVGL puisse dessiner à l'écran.
 * Le buffer de dessin fait 240 x 40 lignes (compromis mémoire/perf).
 */

#include "display.h"
#include "../config.h"
#include <lvgl.h>

/* Instance TFT globale — les pins sont configurées via build_flags */
static TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);

/* Buffer de dessin LVGL (un seul buffer, 240x40 lignes) */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * LV_BUF_LINES];

/* Driver d'affichage LVGL */
static lv_disp_drv_t disp_drv;

/**
 * Callback flush : transfère le contenu du buffer LVGL vers l'écran TFT.
 * Appelé automatiquement par LVGL quand une zone doit être redessinée.
 *
 * @param drv     Pointeur vers le driver d'affichage
 * @param area    Zone rectangulaire à mettre à jour
 * @param color_p Pointeur vers les pixels à afficher
 */
static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)color_p, w * h, true);
    tft.endWrite();

    /* Signaler à LVGL que le flush est terminé */
    lv_disp_flush_ready(drv);
}

void hal_display_init()
{
    /* Initialisation TFT */
    tft.init();
    tft.setRotation(0); /* Portrait : 240 large x 320 haut */
    tft.fillScreen(TFT_BLACK);

    /* Allumer le rétroéclairage */
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);

    /* Initialisation du buffer de dessin LVGL */
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_WIDTH * LV_BUF_LINES);

    /* Configuration du driver d'affichage LVGL */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_WIDTH;
    disp_drv.ver_res  = SCREEN_HEIGHT;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;

    lv_disp_drv_register(&disp_drv);
}

TFT_eSPI& hal_display_get_tft()
{
    return tft;
}
