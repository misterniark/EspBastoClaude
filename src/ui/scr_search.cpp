/**
 * @file scr_search.cpp
 * @brief Implémentation de l'écran de recherche relais
 *
 * Écran simple avec :
 * - Header "WEBASTO CTRL"
 * - Spinner cyan au centre (animation rotation)
 * - Label "RECHERCHE RELAIS..." clignotant
 *
 * Un timer LVGL vérifie toutes les 500ms si le relais est connecté.
 * Dès que relay_is_connected() retourne true, transition vers le menu.
 */

#include "scr_search.h"
#include "ui_common.h"
#include "ui_header.h"
#include "../config.h"
#include "../comm/relay_link.h"
#include <lvgl.h>

/* Référence vers le menu pour la transition */
extern void scr_menu_create();

/* Écran courant (pour le filet de sécurité LV_EVENT_DELETE) */
static lv_obj_t *scr = nullptr;

/* Timer de vérification de connexion */
static lv_timer_t *check_timer = nullptr;

/**
 * Timer callback : vérifie si le relais est connecté.
 * Si oui, supprime le timer et transitionne vers le menu principal.
 */
static void check_connection_cb(lv_timer_t *timer)
{
    (void)timer;
    if (relay_is_connected()) {
        if (check_timer) {
            lv_timer_del(check_timer);
            check_timer = nullptr;
        }
        scr_menu_create();
    }
}

/**
 * Callback LV_EVENT_DELETE de l'écran : filet de sécurité quand
 * l'écran est remplacé sans transition locale — cas du dispatcher
 * d'alertes (ui_alerts), par exemple une erreur capteur critique
 * pendant la recherche du relais. Sans ce nettoyage, une reconnexion
 * ultérieure ferait surgir le menu par-dessus l'écran d'alerte.
 * Le test sur `scr` évite de supprimer le timer d'une NOUVELLE
 * instance de l'écran créée avant la destruction effective de
 * l'ancienne (l'auto-delete arrive en fin d'animation de fondu).
 */
static void on_screen_delete(lv_event_t *e)
{
    if (lv_event_get_target(e) != scr) return;
    scr = nullptr;
    if (check_timer) {
        lv_timer_del(check_timer);
        check_timer = nullptr;
    }
}

void scr_search_create()
{
    /* Nettoyer un timer résiduel d'une instance précédente encore
     * vivante — voir le commentaire de scr_thermostat_create(). */
    if (check_timer) {
        lv_timer_del(check_timer);
        check_timer = nullptr;
    }

    /* Créer l'écran */
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, cl_bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Nettoyage du timer si l'écran est détruit par un tiers
     * (dispatcher d'alertes) — voir on_screen_delete */
    lv_obj_add_event_cb(scr, on_screen_delete, LV_EVENT_DELETE, NULL);

    /* Header */
    header_create(scr);
    header_set_title("WEBASTO CTRL");

    /* Spinner central (animation de recherche) */
    lv_obj_t *spinner = lv_spinner_create(scr, 1000, 60);
    lv_obj_set_size(spinner, 48, 48);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_arc_color(spinner, cl_cyan, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, cl_card, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner, 4, LV_PART_MAIN);

    /* Label "RECHERCHE RELAIS..." */
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "RECHERCHE RELAIS...");
    lv_obj_set_style_text_color(lbl, cl_text, 0);
    lv_obj_set_style_text_opa(lbl, OPA_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 30);

    /* Animation de clignotement du label */
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, lbl);
    lv_anim_set_exec_cb(&anim, [](void *obj, int32_t v) {
        lv_obj_set_style_text_opa((lv_obj_t *)obj, v, 0);
    });
    lv_anim_set_values(&anim, OPA_TEXT_DIM, OPA_DISABLED);
    lv_anim_set_time(&anim, 800);
    lv_anim_set_playback_time(&anim, 800);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);

    /* Timer de vérification : toutes les 500ms */
    check_timer = lv_timer_create(check_connection_cb, 500, NULL);

    /* Afficher l'écran avec transition fondu */
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_SCREEN_FADE, 0, true);
}
