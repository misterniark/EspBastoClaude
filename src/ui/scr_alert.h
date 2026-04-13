/**
 * @file scr_alert.h
 * @brief Écrans d'alerte (connexion perdue, erreur capteur)
 *
 * Écrans plein écran superposés à l'interface principale.
 * Réveillent l'écran si nécessaire.
 * Acquittables par un bouton OK large.
 */

#ifndef UI_SCR_ALERT_H
#define UI_SCR_ALERT_H

/**
 * Affiche l'alerte de connexion perdue.
 * Texte : "CONNEXION PERDUE" + instructions vérification.
 * Bouton OK bordure rouge. Réveille l'écran.
 */
void scr_alert_connection_lost();

/**
 * Affiche l'alerte d'erreur capteur AHT21.
 * Texte : "ERREUR CAPTEUR" + infos câblage (SDA=27, SCL=22).
 * Bouton OK bordure rouge. Réveille l'écran.
 */
void scr_alert_sensor_error();

#endif /* UI_SCR_ALERT_H */
