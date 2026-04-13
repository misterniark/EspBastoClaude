/**
 * @file touchpad.h
 * @brief Driver tactile XPT2046 pour LVGL
 *
 * Initialise le touchpad XPT2046 sur son bus SPI dédié (VSPI)
 * et enregistre le driver d'entrée LVGL.
 * Intègre la calibration et l'anti-rebond.
 */

#ifndef HAL_TOUCHPAD_H
#define HAL_TOUCHPAD_H

/**
 * Initialise le XPT2046 et enregistre le driver d'entrée LVGL.
 * Doit être appelé après hal_display_init() et lv_init().
 */
void hal_touchpad_init();

/**
 * Indique si un toucher est actuellement détecté (avant calibration/debounce).
 * Utile pour le réveil de l'écran.
 */
bool hal_touchpad_is_touched();

#endif /* HAL_TOUCHPAD_H */
