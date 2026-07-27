/**
 * @file touchpad.cpp
 * @brief Implémentation du driver tactile pour LVGL
 *
 * Deux backends matériels, sélectionnés à la compilation :
 *
 *   - CYD (défaut) : XPT2046 résistif sur un bus SPI dédié (VSPI).
 *     Coordonnées brutes 12 bits mappées via calibration (config.h).
 *
 *   - CROWPANEL (-DHW_CROWPANEL) : FT6236 capacitif sur I2C
 *     (adresse 0x38, SDA=15, SCL=16). Le contrôleur renvoie
 *     directement des coordonnées en pixels (natif portrait 240x320),
 *     éventuellement à réorienter via touch_mapping.h.
 *     Ce bus I2C est aussi celui du connecteur externe I2C-OUT
 *     (jauge batterie MAX17048) : Wire est initialisé ici, une
 *     seule fois, pour tous les périphériques.
 */

#include "touchpad.h"
#include "../config.h"
#include "touch_gate.h"
#include <lvgl.h>

/* Filtre "ignorer jusqu'au relâchement", commun aux deux backends.
 * Armé par power_update() au réveil de l'écran pour que le toucher
 * de réveil ne soit jamais transmis à LVGL (voir touch_gate.h). */
static TouchGate touch_gate;

void hal_touchpad_ignore_until_release()
{
    touch_gate_arm(touch_gate);
}

#ifdef HW_CROWPANEL
/* ================================================================
 * Backend FT6236 (capacitif I2C) — CrowPanel Advance 2.8
 * ================================================================ */

#include <Arduino.h> /* Serial — non inclus par Wire.h sur le core ESP32-S3 */
#include <Wire.h>
#include "touch_mapping.h"

/* Registres FT6x36 utiles (datasheet FocalTech) :
 *   0x02 TD_STATUS : nombre de points de contact (bits 0-3)
 *   0x03 P1_XH     : bits hauts X du point 1 (bits 0-3) + drapeaux
 *   0x04 P1_XL     : bits bas X
 *   0x05 P1_YH     : bits hauts Y (bits 0-3)
 *   0x06 P1_YL     : bits bas Y
 */
static constexpr uint8_t FT_REG_TD_STATUS = 0x02;

/* Driver d'entrée LVGL */
static lv_indev_drv_t indev_drv;

/**
 * Lit le nombre de points de contact actifs (0 si erreur I2C).
 * Lecture d'un seul octet : assez rapide (~50µs à 400kHz) pour
 * être appelée à chaque tour de boucle, y compris en veille.
 */
static uint8_t ft_touch_count()
{
    Wire.beginTransmission(FT6X36_I2C_ADDR);
    Wire.write(FT_REG_TD_STATUS);
    if (Wire.endTransmission(false) != 0) return 0; /* Erreur bus */

    if (Wire.requestFrom((int)FT6X36_I2C_ADDR, 1) != 1) return 0;

    /* Bits 0-3 = nombre de contacts. Le FT6x36 peut renvoyer 0x0F
     * de façon transitoire au boot : on ne garde que 1 ou 2. */
    uint8_t n = Wire.read() & 0x0F;
    return (n <= 2) ? n : 0;
}

/**
 * Lit les coordonnées brutes du premier point de contact.
 *
 * @param raw_x Coordonnée X brute (0-239 en natif portrait)
 * @param raw_y Coordonnée Y brute (0-319 en natif portrait)
 * @return true si un contact est actif et la lecture valide
 */
static bool ft_read_point(int16_t &raw_x, int16_t &raw_y)
{
    Wire.beginTransmission(FT6X36_I2C_ADDR);
    Wire.write(FT_REG_TD_STATUS);
    if (Wire.endTransmission(false) != 0) return false;

    /* Lecture groupée : TD_STATUS + P1_XH/XL + P1_YH/YL */
    uint8_t buf[5];
    if (Wire.requestFrom((int)FT6X36_I2C_ADDR, 5) != 5) return false;
    for (uint8_t &b : buf) b = Wire.read();

    uint8_t touches = buf[0] & 0x0F;
    if (touches == 0 || touches > 2) return false;

    /* Coordonnées 12 bits : 4 bits hauts + 8 bits bas */
    raw_x = ((buf[1] & 0x0F) << 8) | buf[2];
    raw_y = ((buf[3] & 0x0F) << 8) | buf[4];
    return true;
}

/**
 * Callback de lecture du touchpad pour LVGL.
 * Le FT6236 renvoie déjà des pixels : pas de calibration nécessaire,
 * seulement la réorientation éventuelle (swap/miroir) + bornage.
 * Pas d'anti-rebond : un contrôleur capacitif ne rebondit pas.
 * Le filtre touch_gate masque le toucher de réveil (voir touch_gate.h).
 */
static void touchpad_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv; /* Paramètre non utilisé */

    int16_t raw_x, raw_y;
    bool touched = ft_read_point(raw_x, raw_y);

    /* Masquer le toucher de réveil tant que le doigt n'a pas été levé */
    if (!touch_gate_pass(touch_gate, touched)) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    TouchPoint p = touch_map_point(raw_x, raw_y,
                                   SCREEN_WIDTH, SCREEN_HEIGHT,
                                   TOUCH_SWAP_XY, TOUCH_MIRROR_X, TOUCH_MIRROR_Y);
    data->point.x = p.x;
    data->point.y = p.y;
    data->state = LV_INDEV_STATE_PRESSED;
}

void hal_touchpad_init()
{
    /* Initialiser le bus I2C partagé (tactile + I2C-OUT externe).
     * Appelé avant battery_init() dans main.cpp : la jauge MAX17048
     * réutilise ce même bus sans refaire Wire.begin(). */
    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    Wire.setClock(400000); /* Fast mode, supporté par le FT6236 */

    /* La broche INT n'est pas utilisée (polling I2C), mais on la
     * configure en entrée pour la laisser dans un état défini. */
    pinMode(PIN_TOUCH_INT, INPUT);

    /* Vérification de présence du contrôleur (log de diagnostic) */
    Wire.beginTransmission(FT6X36_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("[TOUCH] FT6236 detecte (I2C 0x38)");
    } else {
        Serial.println("[TOUCH] ATTENTION : FT6236 non detecte sur I2C !");
    }

    /* Enregistrer le driver d'entrée LVGL */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read_cb;
    lv_indev_drv_register(&indev_drv);
}

bool hal_touchpad_is_touched()
{
    return ft_touch_count() > 0;
}

#else
/* ================================================================
 * Backend XPT2046 (résistif SPI) — CYD ESP32-2432S028R
 * ================================================================ */

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

    /* Masquer le toucher de réveil tant que le doigt n'a pas été levé
     * (voir touch_gate.h) */
    if (!touch_gate_pass(touch_gate, ts.touched())) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    {
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

#endif /* HW_CROWPANEL */
