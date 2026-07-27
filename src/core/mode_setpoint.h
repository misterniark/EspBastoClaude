/**
 * @file mode_setpoint.h
 * @brief Mode C — Consigne simple (sans hysterese, arret definitif)
 *
 * Chauffe jusqu'a atteindre la temperature cible, puis arret definitif.
 * Contrairement au thermostat, ce mode ne redemarrera PAS automatiquement
 * si la temperature redescend sous la cible apres l'avoir atteinte.
 *
 * Le flag "reached" indique que la consigne a ete atteinte et que
 * le chauffage est definitivement arrete pour cette session.
 */

#ifndef CORE_MODE_SETPOINT_H
#define CORE_MODE_SETPOINT_H

/**
 * Demarre le mode consigne et allume le chauffage.
 * Le flag "reached" est reinitialise.
 *
 * Garde I4 : le demarrage est refuse si le capteur n'est pas pret —
 * aucune lecture valide, erreur en cours, ou derniere lecture valide
 * plus vieille que SENSOR_MAX_AGE_BEFORE_START_MS (cas typique :
 * reveil d'ecran apres une veille sans mode actif ; voir
 * mode_thermostat.h pour le detail). Reessayer ~1 s plus tard
 * suffit en general.
 *
 * @param target Temperature cible en °C
 * @return true si le mode a demarre, false si refuse (capteur non pret)
 */
bool setpoint_mode_start(float target);

/**
 * Arrete le mode consigne et eteint le chauffage.
 * Peut etre appele manuellement avant que la cible ne soit atteinte.
 */
void setpoint_mode_stop();

/**
 * Mise a jour periodique du mode consigne.
 * A appeler dans la boucle principale (loop).
 * Evalue la temperature a chaque nouvelle lecture du capteur et
 * arrete definitivement le chauffage si la cible est atteinte
 * (criteres : EMA >= cible, ou SETPOINT_CUT_READINGS lectures brutes
 * consecutives >= cible — voir setpoint_cut.h).
 */
void setpoint_mode_update();

/** Retourne true si le mode consigne est actif (en cours de chauffe). */
bool setpoint_mode_is_active();

/**
 * Retourne true si la temperature cible a ete atteinte.
 * Ce flag reste a true meme apres l'arret : il indique un arret definitif.
 */
bool setpoint_mode_is_reached();

/** Retourne la temperature cible actuelle en °C. */
float setpoint_mode_get_target();

/**
 * Modifie la temperature cible.
 * Prend effet au prochain cycle d'evaluation.
 *
 * @param target Nouvelle temperature cible en °C
 */
void setpoint_mode_set_target(float target);

#endif /* CORE_MODE_SETPOINT_H */
