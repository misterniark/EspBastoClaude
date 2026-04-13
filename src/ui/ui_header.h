/**
 * @file ui_header.h
 * @brief Widget header réutilisable (bandeau haut 30px)
 *
 * Affiché en haut de chaque écran. Contient :
 * - Titre de l'écran (aligné à gauche)
 * - Température actuelle (cyan)
 * - Icône chauffage (ambre pulsante si ON)
 * - Icône connexion (vert si OK, rouge si perdue)
 * - Icône verrou (rouge si LOCKED)
 * - Ligne de séparation cyan en bas
 */

#ifndef UI_HEADER_H
#define UI_HEADER_H

#include <lvgl.h>

/**
 * Crée le widget header sur un écran parent.
 * Le header fait 240px de large et HEADER_HEIGHT de haut.
 *
 * @param parent Écran parent (lv_obj créé avec lv_obj_create(NULL))
 * @return Pointeur vers le conteneur header
 */
lv_obj_t* header_create(lv_obj_t *parent);

/** Met à jour le titre de l'écran. */
void header_set_title(const char *title);

/** Met à jour l'affichage de la température. */
void header_set_temp(float temp);

/** Met à jour l'icône de chauffage (flamme ambre pulsante si ON). */
void header_set_heating(bool on);

/** Met à jour l'icône de connexion (vert/rouge). */
void header_set_connected(bool connected);

/** Met à jour l'icône de verrouillage. */
void header_set_locked(bool locked);

/**
 * Met à jour l'indicateur batterie.
 *
 * @param percent Pourcentage de charge (0-100)
 * @param available true si le MAX17048 est détecté
 */
void header_set_battery(float percent, bool available);

#endif /* UI_HEADER_H */
