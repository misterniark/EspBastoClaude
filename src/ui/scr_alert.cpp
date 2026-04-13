/**
 * @file scr_alert.cpp
 * @brief Implémentation des écrans d'alerte
 *
 * Design commun : fond noir, carte centrale avec bordure rouge 2px,
 * ombre rouge diffuse, titre rouge 16px, texte explicatif gris,
 * bouton OK pleine largeur en bas avec bordure rouge.
 *
 * Les deux alertes (connexion et capteur) utilisent la même structure,
 * seuls les textes changent.
 */

#include "scr_alert.h"
#include "ui_common.h"
#include "../config.h"
#include "../core/heater_fsm.h"
#include "../hal/backlight.h"
#include <lvgl.h>

/* Référence vers le menu pour le retour après acquittement */
extern void scr_menu_create();

/**
 * Callback du bouton OK : acquitte l'alerte et retourne au menu.
 */
static void ok_cb(lv_event_t *e)
{
    (void)e;

    /* Acquitter l'alerte de connexion si active */
    heater_clear_connection_alert();

    /* Retourner au menu principal */
    scr_menu_create();
}

/**
 * Crée un écran d'alerte générique avec titre, message et bouton OK.
 * Réveille l'écran si en veille.
 *
 * @param title   Titre de l'alerte (ex: "CONNEXION PERDUE")
 * @param line1   Première ligne de texte explicatif
 * @param line2   Deuxième ligne de texte explicatif (ou NULL)
 * @param line3   Troisième ligne (ou NULL)
 */
static void create_alert_screen(const char *title,
                                const char *line1,
                                const char *line2,
                                const char *line3)
{
    /* Réveiller l'écran si endormi */
    if (is_display_sleeping()) {
        display_wake();
    }

    /* Créer l'écran */
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, cl_bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Carte centrale avec bordure gauche rouge (style Muted Industrial) */
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_size(card, 208, 160);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(card, cl_card, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(card, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(card, cl_red, 0);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_radius(card, CARD_RADIUS, 0);
    lv_obj_set_style_shadow_width(card, 0, 0); /* Flat — pas d'ombre */
    lv_obj_set_style_pad_all(card, 15, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Titre rouge */
    lv_obj_t *lbl_title = lv_label_create(card);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, cl_red, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Texte explicatif (blanc atténué) */
    int y_offset = 30;

    if (line1) {
        lv_obj_t *lbl1 = lv_label_create(card);
        lv_label_set_text(lbl1, line1);
        lv_obj_set_style_text_color(lbl1, cl_text, 0);
        lv_obj_set_style_text_opa(lbl1, OPA_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl1, LV_ALIGN_TOP_LEFT, 0, y_offset);
        y_offset += 22;
    }

    if (line2) {
        lv_obj_t *lbl2 = lv_label_create(card);
        lv_label_set_text(lbl2, line2);
        lv_obj_set_style_text_color(lbl2, cl_text, 0);
        lv_obj_set_style_text_opa(lbl2, OPA_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl2, LV_ALIGN_TOP_LEFT, 0, y_offset);
        y_offset += 22;
    }

    if (line3) {
        lv_obj_t *lbl3 = lv_label_create(card);
        lv_label_set_text(lbl3, line3);
        lv_obj_set_style_text_color(lbl3, cl_text, 0);
        lv_obj_set_style_text_opa(lbl3, OPA_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl3, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl3, LV_ALIGN_TOP_LEFT, 0, y_offset);
    }

    /* Bouton OK pleine largeur en bas */
    lv_obj_t *btn_ok = ui_create_btn(scr, "OK", 200, BTN_HEIGHT);
    lv_obj_add_style(btn_ok, &style_btn_danger, 0);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_add_event_cb(btn_ok, ok_cb, LV_EVENT_CLICKED, NULL);

    /* Afficher avec transition fondu */
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_SCREEN_FADE, 0, true);
}

void scr_alert_connection_lost()
{
    create_alert_screen(
        "CONNEXION PERDUE",
        "Verifier le module",
        "relais et la portee",
        "radio."
    );
}

void scr_alert_sensor_error()
{
    create_alert_screen(
        "ERREUR CAPTEUR",
        "Verifier le cablage",
        "AHT21 sur CN1",
        "SDA=GPIO27  SCL=GPIO22"
    );
}
