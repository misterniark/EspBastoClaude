/**
 * @file ui_common.cpp
 * @brief Implémentation du thème et des styles LVGL
 *
 * Thème "Muted Industrial" :
 *   - Fond charbon chaud (#272626)
 *   - Panneaux sombres (#2F2E2E) sans bordure visible
 *   - Accent teal éteint (#5A9B9B) pour info/données
 *   - Accent or brun (#906D0C) pour chauffage actif
 *   - Rouge éteint (#8B3A3A) pour erreurs/danger
 *   - Vert éteint (#3A6B5A) pour connexion OK
 *   - Design flat : pas d'ombres, coins quasi-carrés (3px)
 */

#include "ui_common.h"
#include "../config.h"
#include <Arduino.h>

/* ==========================================
 * Couleurs LVGL pré-calculées
 * ========================================== */
lv_color_t cl_bg        = lv_color_hex(COLOR_BG);
lv_color_t cl_bg_header = lv_color_hex(COLOR_BG_HEADER);
lv_color_t cl_card      = lv_color_hex(COLOR_CARD);
lv_color_t cl_border    = lv_color_hex(COLOR_BORDER);
lv_color_t cl_cyan      = lv_color_hex(COLOR_TEAL);       /* Teal éteint (remplace cyan) */
lv_color_t cl_amber     = lv_color_hex(COLOR_WARM);        /* Or brun (remplace ambre) */
lv_color_t cl_red       = lv_color_hex(COLOR_RED);
lv_color_t cl_green     = lv_color_hex(COLOR_GREEN);
lv_color_t cl_text      = lv_color_hex(COLOR_TEXT);
lv_color_t cl_text_dim  = lv_color_hex(COLOR_TEXT_DIM);

/* ==========================================
 * Styles LVGL
 * ========================================== */
lv_style_t style_card;
lv_style_t style_btn;
lv_style_t style_btn_pressed;
lv_style_t style_btn_disabled;
lv_style_t style_btn_start;
lv_style_t style_btn_stop;
lv_style_t style_btn_danger;

void ui_common_init()
{
    /* --- Style carte / tuile ---
     * Fond panneau sombre, pas de bordure visible, pas d'ombre.
     * Les tuiles utilisent une bordure gauche accent (ajoutée dans scr_menu).
     */
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, cl_card);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_width(&style_card, 0);
    lv_style_set_radius(&style_card, CARD_RADIUS);
    lv_style_set_shadow_width(&style_card, 0); /* Flat — pas d'ombre */
    lv_style_set_pad_all(&style_card, 10);

    /* --- Style bouton normal ---
     * Fond panneau, pas de bordure, pas d'ombre. Design flat.
     */
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x363535));
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_border_width(&style_btn, 0);
    lv_style_set_radius(&style_btn, BTN_RADIUS);
    lv_style_set_shadow_width(&style_btn, 0);
    lv_style_set_text_color(&style_btn, cl_text_dim);
    lv_style_set_pad_ver(&style_btn, 8);
    lv_style_set_pad_hor(&style_btn, 12);

    /* --- Style bouton appuyé --- */
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, lv_color_hex(COLOR_TEAL_DARK));
    lv_style_set_bg_opa(&style_btn_pressed, LV_OPA_COVER);

    /* --- Style bouton désactivé (verrouillage) --- */
    lv_style_init(&style_btn_disabled);
    lv_style_set_bg_color(&style_btn_disabled, cl_bg);
    lv_style_set_bg_opa(&style_btn_disabled, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_disabled, cl_text);
    lv_style_set_text_opa(&style_btn_disabled, OPA_DISABLED);

    /* --- Style bouton START (accent teal sombre) --- */
    lv_style_init(&style_btn_start);
    lv_style_set_bg_color(&style_btn_start, lv_color_hex(COLOR_TEAL_DARK));
    lv_style_set_text_color(&style_btn_start, lv_color_hex(0x7AB8A8));

    /* --- Style bouton STOP (accent or brun) --- */
    lv_style_init(&style_btn_stop);
    lv_style_set_bg_color(&style_btn_stop, lv_color_hex(0x3A2A1A));
    lv_style_set_text_color(&style_btn_stop, lv_color_hex(0xC89A3A));

    /* --- Style bouton danger (accent rouge éteint) --- */
    lv_style_init(&style_btn_danger);
    lv_style_set_bg_color(&style_btn_danger, lv_color_hex(0x3A2020));
    lv_style_set_text_color(&style_btn_danger, lv_color_hex(0xC07070));

    Serial.println("[UI] Thème Muted Industrial initialisé");
}

void ui_set_locked(lv_obj_t *obj, bool locked)
{
    if (locked) {
        lv_obj_set_style_opa(obj, OPA_DISABLED, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
}

lv_obj_t* ui_create_btn(lv_obj_t *parent, const char *text, int width, int height)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, width, height);
    lv_obj_add_style(btn, &style_btn, 0);
    lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return btn;
}

lv_obj_t* ui_create_adjust_btn(lv_obj_t *parent, const char *text, int width, int height)
{
    lv_obj_t *btn = ui_create_btn(parent, text, width, height);

    /* Police plus grande pour les boutons +/- */
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);

    return btn;
}

/* ==========================================
 * Toast — Message temporaire
 * ========================================== */

/** Conteneur du toast en cours (NULL si aucun) */
static lv_obj_t *toast_box = NULL;

/** Timer one-shot de disparition du toast */
static lv_timer_t *toast_timer = NULL;

/** Durée d'affichage du toast en ms */
static constexpr uint32_t TOAST_DURATION_MS = 2000;

/**
 * Callback du timer : supprime le toast et libère les références.
 * Le timer est one-shot (repeat_count = 1), LVGL le supprime lui-même
 * après ce callback — il ne faut PAS le supprimer ici.
 */
static void toast_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (toast_box) {
        lv_obj_del(toast_box);
        toast_box = NULL;
    }
    toast_timer = NULL;
}

void ui_toast(const char *text)
{
    /* Remplacer un éventuel toast déjà affiché */
    if (toast_box) {
        lv_obj_del(toast_box);
        toast_box = NULL;
    }
    if (toast_timer) {
        lv_timer_del(toast_timer);
        toast_timer = NULL;
    }

    /* Conteneur sur la couche du dessus : visible par-dessus tous les
     * widgets et indépendant de l'écran courant */
    toast_box = lv_obj_create(lv_layer_top());
    lv_obj_add_style(toast_box, &style_card, 0);
    lv_obj_set_style_border_width(toast_box, 1, 0);
    lv_obj_set_style_border_color(toast_box, cl_border, 0);
    lv_obj_set_size(toast_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(toast_box, 8, 0);
    lv_obj_clear_flag(toast_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(toast_box, LV_OBJ_FLAG_CLICKABLE);
    /* Juste au-dessus de la barre d'action */
    lv_obj_align(toast_box, LV_ALIGN_BOTTOM_MID, 0, -(ACTION_BAR_HEIGHT + 8));

    lv_obj_t *label = lv_label_create(toast_box);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label, cl_text, 0);
    lv_obj_center(label);

    /* Disparition automatique après TOAST_DURATION_MS */
    toast_timer = lv_timer_create(toast_timer_cb, TOAST_DURATION_MS, NULL);
    lv_timer_set_repeat_count(toast_timer, 1);
}
