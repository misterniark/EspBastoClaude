/**
 * @file backlight.h
 * @brief Contrôle du rétroéclairage et mise en veille de l'écran ILI9341
 *
 * Gère l'allumage/extinction du backlight (GPIO21)
 * et les commandes SLPIN/SLPOUT du contrôleur ILI9341
 * pour réduire la consommation en veille.
 */

#ifndef HAL_BACKLIGHT_H
#define HAL_BACKLIGHT_H

/** Initialise la pin du rétroéclairage (GPIO21 en OUTPUT). */
void backlight_init();

/** Allume le rétroéclairage. */
void backlight_on();

/** Éteint le rétroéclairage. */
void backlight_off();

/** Met l'écran ILI9341 en veille (commande SLPIN). */
void display_sleep();

/**
 * Réveille l'écran ILI9341 (commande SLPOUT + délai 120ms).
 * Bloquant à cause du délai requis par le contrôleur.
 */
void display_wake();

/** Retourne true si l'écran est actuellement en veille. */
bool is_display_sleeping();

#endif /* HAL_BACKLIGHT_H */
