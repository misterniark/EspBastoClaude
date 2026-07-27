/**
 * @file scr_setpoint.cpp
 * @brief Écran Mode C — Consigne simple (one-shot)
 *
 * Layout :
 *   - Header "CONSIGNE"
 *   - Arc LVGL (120px) : cyan normalement, vert quand consigne atteinte
 *   - Label central : température (24px) + "cible: XX°C" (12px)
 *   - Ligne de réglage : [-] + valeur + [+] (pas 0.5°C)
 *   - Barre d'action : RETOUR + DEMARRER/ARRETER
 */

#include "scr_setpoint.h"
#include "ui_common.h"
#include "ui_header.h"
#include "ui_action_bar.h"
#include "../config.h"
#include "../core/heater_fsm.h"
#include "../core/mode_setpoint.h"
#include "../hal/sensor.h"
#include "../comm/relay_link.h"
#include "../core/storage.h"
#include <lvgl.h>
#include <cstdio>

extern void scr_menu_create();

static lv_obj_t *scr           = nullptr;
static lv_obj_t *arc           = nullptr;
static lv_obj_t *lbl_temp      = nullptr;
static lv_obj_t *lbl_target    = nullptr;
static lv_obj_t *lbl_value     = nullptr;
static lv_obj_t *lbl_reached   = nullptr;
static lv_obj_t *action_bar    = nullptr;
static lv_timer_t *update_timer = nullptr;
static lv_timer_t *save_timer   = nullptr;

/* Sauvegarde différée (5s après dernier réglage) */
static void save_cb(lv_timer_t *t)
{
    (void)t;
    storage_save_setpoint(setpoint_mode_get_target());
    if (save_timer) { lv_timer_del(save_timer); save_timer = nullptr; }
}

static void schedule_save()
{
    if (save_timer) lv_timer_del(save_timer);
    save_timer = lv_timer_create(save_cb, 5000, NULL);
    lv_timer_set_repeat_count(save_timer, 1);
}

static void update_value_label()
{
    if (lbl_value) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f°C", setpoint_mode_get_target());
        lv_label_set_text(lbl_value, buf);
    }
}

/* Timer de mise à jour (500ms) */
static void update_cb(lv_timer_t *t)
{
    (void)t;
    float temp = sensor_get_temperature();

    /* Arc : valeur mappée sur 0-100 */
    if (arc) {
        int16_t val = (int16_t)((temp - SETPOINT_MIN) / (SETPOINT_MAX - SETPOINT_MIN) * 100);
        if (val < 0) val = 0;
        if (val > 100) val = 100;
        lv_arc_set_value(arc, val);

        /* Couleur selon état */
        lv_color_t col = setpoint_mode_is_reached() ? cl_green : cl_cyan;
        lv_obj_set_style_arc_color(arc, col, LV_PART_INDICATOR);
    }

    /* Labels */
    if (lbl_temp) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f°C", temp);
        lv_label_set_text(lbl_temp, buf);
    }

    if (lbl_target) {
        char buf[24];
        snprintf(buf, sizeof(buf), "cible: %.1f°C", setpoint_mode_get_target());
        lv_label_set_text(lbl_target, buf);
    }

    /* "CONSIGNE ATTEINTE" */
    if (lbl_reached) {
        if (setpoint_mode_is_reached()) {
            lv_obj_clear_flag(lbl_reached, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lbl_reached, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Header */
    header_set_temp(temp);
    header_set_heating(heater_get_state() == HEATER_HEATING);
    header_set_connected(relay_is_connected());
    header_set_locked(heater_get_state() == HEATER_LOCKED);

    /* Bouton action */
    if (action_bar) {
        if (setpoint_mode_is_active()) {
            action_bar_update_right(action_bar, "ARRETER", &style_btn_stop);
        } else {
            action_bar_update_right(action_bar, "DEMARRER", &style_btn_start);
        }
    }
}

/**
 * Supprime les timers de cet écran, en flushant la sauvegarde différée
 * (un réglage ne doit pas être perdu). Appelé au départ de l'écran, à
 * sa destruction, et en tête de sa création — voir le commentaire
 * détaillé de scr_thermostat_create() sur les doubles appuis.
 */
static void cleanup_setpoint()
{
    if (update_timer) { lv_timer_del(update_timer); update_timer = nullptr; }
    if (save_timer) { save_cb(nullptr); }
}

static void on_back(lv_event_t *e)
{
    (void)e;
    cleanup_setpoint();
    scr_menu_create();
}

/**
 * Callback LV_EVENT_DELETE de l'écran : filet de sécurité quand
 * l'écran est remplacé sans passer par le bouton RETOUR — cas du
 * dispatcher d'alertes (ui_alerts) qui charge un écran d'alerte
 * par-dessus. Supprime le timer de mise à jour et flush la
 * sauvegarde différée (comme on_back) pour ne pas perdre un réglage.
 * Le test sur `scr` évite de supprimer les timers d'une NOUVELLE
 * instance de l'écran créée avant la destruction effective de
 * l'ancienne (l'auto-delete arrive en fin d'animation de fondu).
 */
static void on_screen_delete(lv_event_t *e)
{
    if (lv_event_get_target(e) != scr) return;
    scr = nullptr;
    cleanup_setpoint();
}

static void on_action(lv_event_t *e)
{
    (void)e;
    if (setpoint_mode_is_active()) {
        setpoint_mode_stop();
    } else {
        /* Refus possible (garde I4) : capteur en erreur, ou mesure
         * obsolète juste après un réveil d'écran — dans ce dernier cas
         * une lecture fraîche est déjà en cours (forcée au réveil),
         * un nouvel appui ~1 s plus tard réussira. */
        if (!setpoint_mode_start(setpoint_mode_get_target())) {
            ui_toast(sensor_is_error() ? "Capteur indisponible"
                                       : "Mesure en cours...");
        }
    }
}

static void on_minus(lv_event_t *e)
{
    (void)e;
    float target = setpoint_mode_get_target() - SETPOINT_STEP;
    if (target < SETPOINT_MIN) target = SETPOINT_MIN;
    setpoint_mode_set_target(target);
    update_value_label();
    schedule_save();
}

static void on_plus(lv_event_t *e)
{
    (void)e;
    float target = setpoint_mode_get_target() + SETPOINT_STEP;
    if (target > SETPOINT_MAX) target = SETPOINT_MAX;
    setpoint_mode_set_target(target);
    update_value_label();
    schedule_save();
}

void scr_setpoint_create()
{
    /* Nettoyer une instance précédente encore vivante (double appui
     * pendant le fondu de chargement) — voir scr_thermostat_create(). */
    cleanup_setpoint();

    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, cl_bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Nettoyage des timers si l'écran est détruit par un tiers
     * (dispatcher d'alertes) — voir on_screen_delete */
    lv_obj_add_event_cb(scr, on_screen_delete, LV_EVENT_DELETE, NULL);

    /* Header */
    header_create(scr);
    header_set_title("CONSIGNE");

    /* Arc température */
    arc = lv_arc_create(scr);
    lv_obj_set_size(arc, ARC_SIZE, ARC_SIZE);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + 10);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_obj_set_style_arc_width(arc, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, cl_card, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, cl_cyan, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);

    /* Label température au centre */
    lbl_temp = lv_label_create(arc);
    lv_label_set_text(lbl_temp, "--.-°C");
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_temp, cl_text, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, -8);

    /* Label cible */
    lbl_target = lv_label_create(arc);
    lv_label_set_text(lbl_target, "cible: --.-°C");
    lv_obj_set_style_text_font(lbl_target, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_target, cl_text_dim, 0);
    lv_obj_align(lbl_target, LV_ALIGN_CENTER, 0, 14);

    /* Zone de réglage */
    int y_adj = HEADER_HEIGHT + ARC_SIZE + 20;

    lv_obj_t *lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "TEMPERATURE CIBLE");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_title, cl_text_dim, 0);
    lv_obj_set_pos(lbl_title, 15, y_adj);

    lv_obj_t *btn_m = ui_create_adjust_btn(scr, "-", 60, 40);
    lv_obj_set_pos(btn_m, 10, y_adj + 20);
    lv_obj_add_event_cb(btn_m, on_minus, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_m, on_minus, LV_EVENT_LONG_PRESSED_REPEAT, NULL);

    lbl_value = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_value, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_value, cl_cyan, 0);
    lv_obj_align(lbl_value, LV_ALIGN_TOP_MID, 0, y_adj + 28);
    update_value_label();

    lv_obj_t *btn_p = ui_create_adjust_btn(scr, "+", 60, 40);
    lv_obj_set_pos(btn_p, SCREEN_WIDTH - 70, y_adj + 20);
    lv_obj_add_event_cb(btn_p, on_plus, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_p, on_plus, LV_EVENT_LONG_PRESSED_REPEAT, NULL);

    /* Label consigne atteinte (masqué) */
    lbl_reached = lv_label_create(scr);
    lv_label_set_text(lbl_reached, "CONSIGNE ATTEINTE");
    lv_obj_set_style_text_font(lbl_reached, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_reached, cl_green, 0);
    lv_obj_align(lbl_reached, LV_ALIGN_TOP_MID, 0, y_adj + 70);
    lv_obj_add_flag(lbl_reached, LV_OBJ_FLAG_HIDDEN);

    /* Barre d'action */
    action_bar = action_bar_create(scr);
    action_bar_set_buttons(action_bar,
                           "RETOUR", on_back,
                           "DEMARRER", on_action,
                           &style_btn_start);

    /* Timer mise à jour */
    update_timer = lv_timer_create(update_cb, 500, NULL);
    update_cb(NULL);

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_SCREEN_FADE, 0, true);
}
