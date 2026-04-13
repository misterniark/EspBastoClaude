/**
 * @file mode_timer.h
 * @brief Mode B — Minuteur (timer)
 *
 * Chauffage pendant une duree fixe sans controle de temperature.
 * Le minuteur demarre le chauffage puis l'arrete automatiquement
 * quand le decompte atteint zero.
 *
 * La duree est modifiable avant le demarrage mais verrouillee
 * pendant le decompte pour eviter toute confusion.
 */

#ifndef CORE_MODE_TIMER_H
#define CORE_MODE_TIMER_H

/**
 * Demarre le minuteur et allume le chauffage.
 * Le decompte commence immediatement.
 *
 * @param duration_min Duree du minuteur en minutes
 */
void timer_mode_start(int duration_min);

/**
 * Arrete le minuteur et eteint le chauffage.
 * Peut etre appele manuellement avant la fin du decompte.
 */
void timer_mode_stop();

/**
 * Mise a jour du minuteur.
 * A appeler dans la boucle principale (loop).
 * Verifie si le decompte est termine et arrete le chauffage le cas echeant.
 */
void timer_mode_update();

/** Retourne true si le minuteur est en cours de decompte. */
bool timer_mode_is_running();

/** Retourne le temps restant en secondes (0 si arrete). */
int timer_mode_get_remaining_s();

/** Retourne la duree configuree en minutes. */
int timer_mode_get_duration_min();

/**
 * Modifie la duree du minuteur.
 * Ne prend effet qu'avant le prochain demarrage ;
 * sans effet si le decompte est en cours.
 *
 * @param duration_min Nouvelle duree en minutes
 */
void timer_mode_set_duration_min(int duration_min);

#endif /* CORE_MODE_TIMER_H */
