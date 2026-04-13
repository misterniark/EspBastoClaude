/**
 * @file ui_action_bar.cpp
 * @brief Implémentation de la barre d'action basse
 *
 * Layout :
 *   [  RETOUR  ]          [  ACTION  ]
 *   ← 110px →   espace    ← 110px →
 *
 * Fond : même que le header (#0A0A0F).
 * Les boutons font 110px de large et 44px de haut.
 * Le bouton droit utilise un style additionnel pour la couleur d'accent.
 */

#include "ui_action_bar.h"
#include "ui_common.h"
#include "../config.h"

/*
 * Convention : le bouton gauche est child[0] du conteneur,
 * le bouton droit est child[1].
 */

lv_obj_t* action_bar_create(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, SCREEN_WIDTH, ACTION_BAR_HEIGHT);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, cl_bg_header, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 8, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    return bar;
}

void action_bar_set_buttons(lv_obj_t *bar,
                            const char *left_text, lv_event_cb_t left_cb,
                            const char *right_text, lv_event_cb_t right_cb,
                            lv_style_t *right_style)
{
    /* Bouton gauche (RETOUR) */
    lv_obj_t *btn_left = ui_create_btn(bar, left_text, 110, BTN_HEIGHT);
    lv_obj_align(btn_left, LV_ALIGN_LEFT_MID, 0, 0);
    if (left_cb) {
        lv_obj_add_event_cb(btn_left, left_cb, LV_EVENT_CLICKED, NULL);
    }

    /* Bouton droit (action principale) */
    lv_obj_t *btn_right = ui_create_btn(bar, right_text, 110, BTN_HEIGHT);
    lv_obj_align(btn_right, LV_ALIGN_RIGHT_MID, 0, 0);
    if (right_style) {
        lv_obj_add_style(btn_right, right_style, 0);
    }
    if (right_cb) {
        lv_obj_add_event_cb(btn_right, right_cb, LV_EVENT_CLICKED, NULL);
    }
}

void action_bar_update_right(lv_obj_t *bar, const char *text, lv_style_t *style)
{
    /* Le bouton droit est le 2e enfant (index 1) */
    lv_obj_t *btn_right = lv_obj_get_child(bar, 1);
    if (!btn_right) return;

    /* Mettre à jour le texte du label (1er enfant du bouton) */
    lv_obj_t *label = lv_obj_get_child(btn_right, 0);
    if (label) {
        lv_label_set_text(label, text);
    }

    /*
     * Remplacer le style d'accent.
     * On supprime les anciens styles d'accent puis on ajoute le nouveau.
     * Les styles d'accent sont aux indices 2+ (0=style_btn, 1=style_btn_pressed).
     */
    lv_obj_remove_style(btn_right, &style_btn_start, 0);
    lv_obj_remove_style(btn_right, &style_btn_stop, 0);
    lv_obj_remove_style(btn_right, &style_btn_danger, 0);

    if (style) {
        lv_obj_add_style(btn_right, style, 0);
    }
}

void action_bar_set_right_hidden(lv_obj_t *bar, bool hidden)
{
    lv_obj_t *btn_right = lv_obj_get_child(bar, 1);
    if (!btn_right) return;

    if (hidden) {
        lv_obj_add_flag(btn_right, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(btn_right, LV_OBJ_FLAG_HIDDEN);
    }
}
