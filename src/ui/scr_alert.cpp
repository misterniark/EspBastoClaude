/**
 * @file scr_alert.cpp
 * @brief Implémentation des écrans d'alerte
 *
 * Design commun (Muted Industrial) : fond charbon, carte centrale avec
 * bordure gauche rouge 3px, titre rouge 16px, texte explicatif gris
 * atténué, bouton OK pleine largeur en bas avec bordure rouge.
 *
 * Les quatre alertes (connexion, capteur, sécurité capteur, sécurité
 * température) utilisent la même structure, seuls les textes et
 * l'action d'acquittement changent.
 */

#include "scr_alert.h"
#include "ui_common.h"
#include "../config.h"
#include "../core/heater_fsm.h"
#include "../comm/relay_link.h"
#include "../power/power_manager.h"
#include <lvgl.h>
#include <cstdio>

/* Références vers les écrans de retour après acquittement */
extern void scr_menu_create();
extern void scr_search_create();

/*
 * Action d'acquittement de l'alerte actuellement affichée.
 * Chaque écran d'alerte enregistre ici la fonction qui lève SON
 * drapeau (et uniquement le sien) : acquitter l'alerte visible ne
 * doit pas lever silencieusement les drapeaux d'alertes non vues,
 * qui doivent pouvoir s'afficher à leur tour.
 */
static void (*ack_cb)(void) = nullptr;

/**
 * Callback du bouton OK : acquitte l'alerte affichée puis quitte
 * l'écran d'alerte.
 *
 * Retour vers le menu si le relais est connecté, sinon vers l'écran
 * de recherche : depuis que les alertes peuvent s'afficher partout
 * (dispatcher central), l'acquittement d'une alerte de connexion
 * perdue doit ramener à la recherche du relais, pas à un menu
 * inutilisable sans liaison.
 */
static void ok_cb(lv_event_t *e)
{
    (void)e;

    /* Acquitter l'alerte affichée (lève son drapeau côté core) */
    if (ack_cb) {
        ack_cb();
        ack_cb = nullptr;
    }

    /* Quitter l'écran d'alerte */
    if (relay_is_connected()) {
        scr_menu_create();
    } else {
        scr_search_create();
    }
}

/**
 * Crée un écran d'alerte générique avec titre, message et bouton OK.
 * Réveille l'écran si en veille (via power_force_wake, qui resynchronise
 * le gestionnaire d'énergie — sans quoi LVGL ne rendrait pas l'alerte).
 *
 * @param title   Titre de l'alerte (ex: "CONNEXION PERDUE")
 * @param line1   Première ligne de texte explicatif
 * @param line2   Deuxième ligne de texte explicatif (ou NULL)
 * @param line3   Troisième ligne (ou NULL)
 * @param on_ack  Action d'acquittement appelée par le bouton OK
 *                (ou NULL pour une alerte d'état sans drapeau à lever)
 */
static void create_alert_screen(const char *title,
                                const char *line1,
                                const char *line2,
                                const char *line3,
                                void (*on_ack)(void))
{
    /* Réveiller l'écran si endormi : l'événement peut survenir pendant
     * la veille (chauffe en tâche de fond). power_force_wake() rallume
     * la dalle ET resynchronise power_manager pour que loop() reprenne
     * le rendu LVGL. */
    power_force_wake();

    /* Mémoriser l'action d'acquittement pour le bouton OK */
    ack_cb = on_ack;

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
        "radio.",
        heater_clear_connection_alert
    );
}

void scr_alert_sensor_error()
{
    /* Détail du câblage selon la cible matérielle :
     * CYD = AHT21 en I2C sur CN1, CrowPanel = DS18B20 en OneWire. */
#ifdef HW_CROWPANEL
    create_alert_screen(
        "ERREUR CAPTEUR",
        "Verifier le cablage",
        "DS18B20 sur le port",
        "UART1-OUT.",
        nullptr /* Alerte d'état : rien à acquitter côté core */
    );
#else
    create_alert_screen(
        "ERREUR CAPTEUR",
        "Verifier le cablage",
        "AHT21 sur CN1",
        "SDA=GPIO27  SCL=GPIO22",
        nullptr /* Alerte d'état : rien à acquitter côté core */
    );
#endif
}

void scr_alert_safety_sensor()
{
    create_alert_screen(
        "SECURITE CAPTEUR",
        "Capteur HS : chauffage",
        "coupe par securite.",
        "Verifier le capteur.",
        heater_clear_sensor_safety_alert
    );
}

void scr_alert_safety_overtemp()
{
    /* Inclure le seuil configuré (TEMP_SAFETY_MAX) dans le message.
     * Buffer statique : lv_label_set_text() copie le texte, mais le
     * formatage a lieu avant la création du label. */
    static char line1[32];
    snprintf(line1, sizeof(line1), "Limite %.1f\xC2\xB0""C atteinte :", TEMP_SAFETY_MAX);

    create_alert_screen(
        "TEMP MAX ATTEINTE",
        line1,
        "chauffage coupe",
        "par securite.",
        heater_clear_sensor_safety_alert
    );
}
