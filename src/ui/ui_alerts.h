/**
 * @file ui_alerts.h
 * @brief Dispatcher central des alertes plein écran
 *
 * Unique consommateur des drapeaux d'alerte du core (arrêts de
 * sécurité C1/C4, perte de connexion I5, erreur capteur critique).
 * Appelé à chaque itération de loop() — donc y compris pendant la
 * veille écran, contrairement aux timers LVGL qui ne tournent que
 * lorsque l'écran est allumé.
 *
 * Quand une alerte doit être montrée (décision : ui/alert_policy.h),
 * le dispatcher réveille l'écran si nécessaire puis charge l'écran
 * d'alerte correspondant par-dessus l'écran courant, quel qu'il soit
 * (menu, écran de mode, recherche).
 */

#ifndef UI_UI_ALERTS_H
#define UI_UI_ALERTS_H

/**
 * Évalue les drapeaux d'alerte et affiche l'écran d'alerte approprié
 * si un nouvel événement le justifie. À appeler dans loop(), après
 * heater_fsm_update() et les mises à jour de mode, avant le rendu
 * LVGL (pour que l'alerte soit dessinée dès ce cycle).
 */
void ui_alerts_update();

#endif /* UI_UI_ALERTS_H */
