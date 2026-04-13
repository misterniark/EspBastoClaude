/**
 * @file ui_action_bar.h
 * @brief Barre d'action basse réutilisable (60px)
 *
 * Bandeau fixe en bas de chaque écran fonctionnel.
 * Contient un bouton gauche (RETOUR) et un bouton droit (action principale).
 * Le style du bouton droit change selon le contexte :
 *   - DÉMARRER : bordure cyan
 *   - ARRÊTER  : bordure ambre
 *   - OK       : bordure rouge (alertes)
 */

#ifndef UI_ACTION_BAR_H
#define UI_ACTION_BAR_H

#include <lvgl.h>

/**
 * Crée la barre d'action en bas d'un écran.
 *
 * @param parent Écran parent
 * @return Pointeur vers le conteneur de la barre
 */
lv_obj_t* action_bar_create(lv_obj_t *parent);

/**
 * Configure les deux boutons de la barre d'action.
 *
 * @param bar        Conteneur créé par action_bar_create()
 * @param left_text  Texte du bouton gauche (ex: "RETOUR")
 * @param left_cb    Callback du bouton gauche (LV_EVENT_CLICKED)
 * @param right_text Texte du bouton droit (ex: "DEMARRER")
 * @param right_cb   Callback du bouton droit
 * @param right_style Style additionnel pour le bouton droit
 *                     (style_btn_start, style_btn_stop ou style_btn_danger)
 */
void action_bar_set_buttons(lv_obj_t *bar,
                            const char *left_text, lv_event_cb_t left_cb,
                            const char *right_text, lv_event_cb_t right_cb,
                            lv_style_t *right_style);

/**
 * Met à jour le texte et le style du bouton droit.
 * Utile pour basculer entre DÉMARRER et ARRÊTER.
 *
 * @param bar        Conteneur de la barre
 * @param text       Nouveau texte
 * @param style      Nouveau style (style_btn_start ou style_btn_stop)
 */
void action_bar_update_right(lv_obj_t *bar, const char *text, lv_style_t *style);

/**
 * Cache ou affiche le bouton droit.
 *
 * @param bar    Conteneur de la barre
 * @param hidden true pour cacher
 */
void action_bar_set_right_hidden(lv_obj_t *bar, bool hidden);

#endif /* UI_ACTION_BAR_H */
