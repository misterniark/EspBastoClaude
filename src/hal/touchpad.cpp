/**
 * @file touchpad.cpp
 * @brief Implémentation du driver tactile XPT2046 pour LVGL
 *
 * Le XPT2046 est câblé sur un bus SPI séparé du TFT (VSPI).
 * Les coordonnées brutes sont mappées vers les pixels écran
 * via des constantes de calibration définies dans config.h.
 */

#include "touchpad.h"
#include "../config.h"
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

/* Bus SPI dédié au tactile (VSPI) */
static SPIClass touchSPI(VSPI);

/* Instance du driver XPT2046 (CS, IRQ) */
static XPT2046_Touchscreen ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ);

/* Driver d'entrée LVGL */
static lv_indev_drv_t indev_drv;

/* Dernier état du toucher pour le debounce */
static unsigned long last_touch_time = 0;

/**
 * Callback de lecture du touchpad pour LVGL.
 * Mappe les coordonnées brutes XPT2046 vers les pixels écran
 * et applique un anti-rebond de 150ms.
 *
 * @param drv  Pointeur vers le driver d'entrée
 * @param data Structure de données d'entrée à remplir
 */
static void touchpad_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv; /* Paramètre non utilisé */

    if (ts.touched()) {
        TS_Point p = ts.getPoint();

        /* Anti-rebond : ignorer les touches trop rapprochées */
        unsigned long now = millis();
        if (now - last_touch_time < TOUCH_DEBOUNCE_MS) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }
        last_touch_time = now;

        /* Mapping des coordonnées brutes vers les pixels écran */
        int16_t x = map(p.x, TS_MINX, TS_MAXX, 0, SCREEN_WIDTH - 1);
        int16_t y = map(p.y, TS_MINY, TS_MAXY, 0, SCREEN_HEIGHT - 1);

        /* Clamp pour éviter les valeurs hors écran */
        if (x < 0) x = 0;
        if (x >= SCREEN_WIDTH) x = SCREEN_WIDTH - 1;
        if (y < 0) y = 0;
        if (y >= SCREEN_HEIGHT) y = SCREEN_HEIGHT - 1;

        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void hal_touchpad_init()
{
    /* Initialiser le bus SPI dédié au tactile */
    touchSPI.begin(PIN_TOUCH_CLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);

    /* Initialiser le XPT2046 sur ce bus */
    ts.begin(touchSPI);
    ts.setRotation(0); /* Doit correspondre à la rotation du TFT */

    /* Enregistrer le driver d'entrée LVGL */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read_cb;
    lv_indev_drv_register(&indev_drv);
}

bool hal_touchpad_is_touched()
{
    return ts.touched();
}
