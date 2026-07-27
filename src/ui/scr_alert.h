/**
 * @file scr_alert.h
 * @brief Écrans d'alerte plein écran
 *
 * Quatre alertes partagent la même structure visuelle (carte à bordure
 * gauche rouge, style Muted Industrial) :
 *   - connexion relais perdue (I5)
 *   - erreur capteur (état : infos câblage)
 *   - arrêt de sécurité capteur (C1 : chauffage coupé, capteur HS)
 *   - arrêt de sécurité température (C4 : chauffage coupé, temp max)
 *
 * Chaque écran réveille l'affichage si nécessaire (les événements
 * peuvent survenir pendant la veille) et s'acquitte par un bouton OK
 * qui lève le drapeau d'alerte correspondant puis revient au menu
 * (ou à l'écran de recherche si le relais n'est plus connecté).
 *
 * L'affichage est piloté exclusivement par ui_alerts_update() (appelé
 * dans loop()), unique consommateur des drapeaux d'alerte.
 */

#ifndef UI_SCR_ALERT_H
#define UI_SCR_ALERT_H

/**
 * Affiche l'alerte de connexion perdue (I5).
 * Texte : "CONNEXION PERDUE" + instructions vérification.
 * OK acquitte heater_clear_connection_alert().
 */
void scr_alert_connection_lost();

/**
 * Affiche l'alerte d'erreur capteur (état, non acquittable côté core).
 * Texte : "ERREUR CAPTEUR" + infos câblage.
 * OK revient simplement au menu ; la politique d'alerte évite le
 * re-affichage en boucle tant que l'épisode d'erreur dure.
 */
void scr_alert_sensor_error();

/**
 * Affiche l'alerte d'arrêt de sécurité capteur (C1) :
 * capteur HS depuis plus de 5 min, chauffage coupé par sécurité.
 * OK acquitte heater_clear_sensor_safety_alert().
 */
void scr_alert_safety_sensor();

/**
 * Affiche l'alerte d'arrêt de sécurité température (C4) :
 * TEMP_SAFETY_MAX atteinte, chauffage coupé par sécurité.
 * OK acquitte heater_clear_sensor_safety_alert().
 */
void scr_alert_safety_overtemp();

#endif /* UI_SCR_ALERT_H */
