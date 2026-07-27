/**
 * @file power_manager.h
 * @brief Gestionnaire d'économie d'énergie
 *
 * Gère :
 * - Le CPU à 80 MHz
 * - La mise en veille de l'écran après 60s d'inactivité
 * - Le réveil de l'écran au toucher (premier toucher consommé)
 * - L'extinction des LEDs RGB
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

/**
 * Initialise le gestionnaire d'énergie :
 * - Réduit la fréquence CPU à 80 MHz
 * - Éteint les LEDs RGB
 * - Initialise le timer d'inactivité
 */
void power_init();

/**
 * Boucle de mise à jour à appeler dans loop().
 * Vérifie le timer d'inactivité et gère le sommeil/réveil de l'écran.
 *
 * @return true si un toucher de réveil a été consommé
 *         (l'appelant ne doit pas traiter ce toucher dans LVGL)
 */
bool power_update();

/**
 * Réinitialise le timer d'inactivité.
 * À appeler à chaque interaction tactile valide.
 */
void power_reset_inactivity();

/**
 * Réveil forcé de l'écran depuis le code (pas par un toucher) :
 * utilisé par les alertes qui peuvent survenir pendant la veille
 * (arrêt de sécurité C1/C4, perte de connexion).
 *
 * Contrairement à un appel direct à display_wake(), cette fonction
 * resynchronise l'état interne du gestionnaire d'énergie : sans cela,
 * le rétroéclairage se rallumerait mais loop() continuerait de sauter
 * lv_timer_handler() (écran considéré en veille) et l'alerte ne serait
 * jamais dessinée. Réinitialise aussi le timer d'inactivité pour
 * laisser à l'utilisateur le délai complet avant remise en veille.
 * Sans effet notable si l'écran est déjà allumé.
 */
void power_force_wake();

/** Retourne true si l'écran est actuellement en veille. */
bool power_is_screen_off();

#endif /* POWER_MANAGER_H */
