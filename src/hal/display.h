/**
 * @file display.h
 * @brief Driver d'affichage TFT ILI9341 pour LVGL
 *
 * Initialise TFT_eSPI et configure le driver d'affichage LVGL 8.x
 * avec un buffer de dessin et le callback flush.
 */

#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include <TFT_eSPI.h>

/**
 * Initialise l'écran TFT et enregistre le driver d'affichage LVGL.
 * Doit être appelé après lv_init().
 */
void hal_display_init();

/**
 * Accès à l'instance TFT pour les commandes directes
 * (ex: SLPIN/SLPOUT pour la mise en veille).
 */
TFT_eSPI& hal_display_get_tft();

#endif /* HAL_DISPLAY_H */
