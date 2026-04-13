/**
 * @file scr_timer.cpp
 * @brief Ecran du mode B — Minuteur (timer)
 *
 * Layout :
 *   [HEADER 30px] Titre "MINUTEUR" + indicateurs
 *   [CONTENU]     Décompte MM:SS + barre de progression + réglage durée
 *   [ACTION 60px] RETOUR | DÉMARRER/ARRÊTER
 *
 * Le décompte MM:SS change de couleur selon le temps restant :
 *   - Cyan  : plus de 5 minutes restantes
 *   - Ambre : moins de 5 minutes
 *   - Rouge : moins de 1 minute
 *
 * La barre de progression change de couleur selon le pourcentage :
 *   - Cyan  : plus de 50%
 *   - Ambre : entre 10% et 50%
 *   - Rouge : moins de 10%
 *
 * Le réglage de durée est grisé pendant le décompte pour
 * éviter toute modification accidentelle.
 */

#include "scr_timer.h"
#include "ui_common.h"
#include "ui_header.h"
#include "ui_action_bar.h"
#include "../config.h"
#include "../core/heater_fsm.h"
#include "../core/mode_timer.h"
#include "../hal/sensor.h"
#include "../comm/relay_link.h"
#include "../core/storage.h"
#include <lvgl.h>
#include <cstdio>

/* Déclaration externe pour la navigation retour */
extern void scr_menu_create();

/* ==========================================
 * Variables statiques de l'écran
 * ========================================== */

/** Ecran LVGL principal */
static lv_obj_t *scr = NULL;

/** Barre d'action */
static lv_obj_t *action_bar = NULL;

/** Label du décompte MM:SS (police 32px) */
static lv_obj_t *lbl_countdown = NULL;

/** Barre de progression */
static lv_obj_t *bar_progress = NULL;

/** Label valeur de la durée dans la rangée de réglage */
static lv_obj_t *lbl_duration_val = NULL;

/** Boutons +/- de la durée (pour grisage pendant le décompte) */
static lv_obj_t *btn_dur_minus = NULL;
static lv_obj_t *btn_dur_plus = NULL;

/** Durée locale en minutes (copiée depuis storage) */
static int local_duration_min = DEFAULT_TIMER_MIN;

/** Timer LVGL de mise à jour périodique (1000ms) */
static lv_timer_t *update_timer = NULL;

/* ==========================================
 * Prototypes des fonctions internes
 * ========================================== */
static void update_timer_cb(lv_timer_t *timer);
static void update_action_button();
static void update_duration_controls();
static void refresh_duration_label();

/* ==========================================
 * Callbacks des événements boutons
 * ========================================== */

/**
 * Callback bouton RETOUR : retour au menu principal.
 */
static void cb_back(lv_event_t *e)
{
    (void)e;
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    scr_menu_create();
}

/**
 * Callback bouton DÉMARRER/ARRÊTER.
 * Démarre ou arrête le minuteur.
 */
static void cb_start_stop(lv_event_t *e)
{
    (void)e;
    if (timer_mode_is_running()) {
        timer_mode_stop();
    } else {
        timer_mode_start(local_duration_min);
    }
    update_action_button();
    update_duration_controls();
}

/**
 * Callback bouton [-] durée : diminue la durée de TIMER_MIN_STEP.
 */
static void cb_duration_minus(lv_event_t *e)
{
    (void)e;
    if (local_duration_min > TIMER_MIN_MIN) {
        local_duration_min -= TIMER_MIN_STEP;
        if (local_duration_min < TIMER_MIN_MIN) local_duration_min = TIMER_MIN_MIN;
        timer_mode_set_duration_min(local_duration_min);
        refresh_duration_label();
        storage_save_timer_min(local_duration_min);
    }
}

/**
 * Callback bouton [+] durée : augmente la durée de TIMER_MIN_STEP.
 */
static void cb_duration_plus(lv_event_t *e)
{
    (void)e;
    if (local_duration_min < TIMER_MIN_MAX) {
        local_duration_min += TIMER_MIN_STEP;
        if (local_duration_min > TIMER_MIN_MAX) local_duration_min = TIMER_MIN_MAX;
        timer_mode_set_duration_min(local_duration_min);
        refresh_duration_label();
        storage_save_timer_min(local_duration_min);
    }
}

/* ==========================================
 * Fonctions utilitaires internes
 * ========================================== */

/**
 * Met à jour le label de la durée avec la valeur locale.
 */
static void refresh_duration_label()
{
    if (lbl_duration_val) {
        lv_label_set_text_fmt(lbl_duration_val, "%d min", local_duration_min);
    }
}

/**
 * Met à jour le texte et le style du bouton d'action droit.
 */
static void update_action_button()
{
    if (!action_bar) return;

    if (timer_mode_is_running()) {
        action_bar_update_right(action_bar, LV_SYMBOL_STOP " ARRETER", &style_btn_stop);
    } else {
        action_bar_update_right(action_bar, LV_SYMBOL_PLAY " DEMARRER", &style_btn_start);
    }
}

/**
 * Active ou grise les contrôles de durée selon l'état du minuteur.
 * Pendant le décompte, les boutons +/- sont désactivés.
 */
static void update_duration_controls()
{
    bool running = timer_mode_is_running();
    if (btn_dur_minus) ui_set_locked(btn_dur_minus, running);
    if (btn_dur_plus)  ui_set_locked(btn_dur_plus, running);
    if (lbl_duration_val) {
        lv_obj_set_style_text_opa(lbl_duration_val,
                                  running ? OPA_DISABLED : LV_OPA_COVER, 0);
    }
}

/**
 * Callback du timer de mise à jour périodique (1000ms).
 * Actualise le décompte, la barre de progression,
 * les couleurs et les indicateurs du header.
 */
static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    bool running = timer_mode_is_running();
    int remaining_s = timer_mode_get_remaining_s();
    int total_s = local_duration_min * 60;

    /* --- Mise à jour du décompte MM:SS --- */
    if (lbl_countdown) {
        if (running) {
            int minutes = remaining_s / 60;
            int seconds = remaining_s % 60;
            lv_label_set_text_fmt(lbl_countdown, "%02d:%02d", minutes, seconds);

            /* Couleur selon le temps restant */
            lv_color_t color;
            if (remaining_s > 300) {
                /* Plus de 5 minutes : cyan */
                color = cl_cyan;
            } else if (remaining_s > 60) {
                /* Moins de 5 minutes : ambre */
                color = cl_amber;
            } else {
                /* Moins de 1 minute : rouge */
                color = cl_red;
            }
            lv_obj_set_style_text_color(lbl_countdown, color, 0);
        } else {
            /* Minuteur arrêté : afficher la durée configurée */
            lv_label_set_text_fmt(lbl_countdown, "%02d:00", local_duration_min);
            lv_obj_set_style_text_color(lbl_countdown, cl_cyan, 0);
        }
    }

    /* --- Mise à jour de la barre de progression --- */
    if (bar_progress) {
        if (running && total_s > 0) {
            int pct = (remaining_s * 100) / total_s;
            lv_bar_set_value(bar_progress, pct, LV_ANIM_ON);

            /* Couleur de l'indicateur selon le pourcentage */
            lv_color_t bar_color;
            if (pct > 50) {
                bar_color = cl_cyan;
            } else if (pct > 10) {
                bar_color = cl_amber;
            } else {
                bar_color = cl_red;
            }
            lv_obj_set_style_bg_color(bar_progress, bar_color, LV_PART_INDICATOR);
        } else {
            /* Minuteur arrêté : barre pleine, cyan */
            lv_bar_set_value(bar_progress, 100, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(bar_progress, cl_cyan, LV_PART_INDICATOR);
        }
    }

    /* --- Mise à jour des indicateurs du header --- */
    float temp = sensor_get_temperature();
    header_set_temp(temp);
    header_set_heating(heater_get_state() == HEATER_HEATING);
    header_set_connected(relay_is_connected());
    header_set_locked(heater_get_state() == HEATER_LOCKED);

    /* --- Mise à jour du bouton d'action et des contrôles --- */
    update_action_button();
    update_duration_controls();
}

/* ==========================================
 * Création de l'écran
 * ========================================== */

void scr_timer_create()
{
    /* --- Charger la durée sauvegardée --- */
    local_duration_min = storage_load_timer_min();

    /* --- Création de l'écran principal --- */
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, cl_bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* --- Header (bandeau haut 30px) --- */
    header_create(scr);
    header_set_title("MINUTEUR");

    /* --- Zone de contenu --- */
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, SCREEN_WIDTH, CONTENT_HEIGHT);
    lv_obj_set_pos(content, 0, HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 16, 0);

    /* --- Label décompte MM:SS (police 32px, cyan, centré) --- */
    lbl_countdown = lv_label_create(content);
    lv_obj_set_style_text_font(lbl_countdown, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_countdown, cl_cyan, 0);
    lv_label_set_text_fmt(lbl_countdown, "%02d:00", local_duration_min);

    /* --- Barre de progression (200x12px, arrondie) --- */
    bar_progress = lv_bar_create(content);
    lv_obj_set_size(bar_progress, BAR_WIDTH, BAR_HEIGHT);
    lv_bar_set_range(bar_progress, 0, 100);
    lv_bar_set_value(bar_progress, 100, LV_ANIM_OFF);
    /* Fond de la barre (partie non remplie) */
    lv_obj_set_style_bg_color(bar_progress, cl_border, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_progress, BAR_HEIGHT / 2, LV_PART_MAIN);
    /* Indicateur de la barre (partie remplie) */
    lv_obj_set_style_bg_color(bar_progress, cl_cyan, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_progress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_progress, BAR_HEIGHT / 2, LV_PART_INDICATOR);

    /* --- Rangée DUREE : label + [-] + valeur + [+] --- */
    lv_obj_t *row_dur = lv_obj_create(content);
    lv_obj_set_size(row_dur, SCREEN_WIDTH - 16, 50);
    lv_obj_set_style_bg_opa(row_dur, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_dur, 0, 0);
    lv_obj_set_style_pad_all(row_dur, 0, 0);
    lv_obj_clear_flag(row_dur, LV_OBJ_FLAG_SCROLLABLE);

    /* Label titre de la rangée */
    lv_obj_t *lbl_dur_title = lv_label_create(row_dur);
    lv_label_set_text(lbl_dur_title, "DUREE");
    lv_obj_set_style_text_font(lbl_dur_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_dur_title, cl_text_dim, 0);
    lv_obj_align(lbl_dur_title, LV_ALIGN_TOP_MID, 0, 0);

    /* Bouton [-] durée */
    btn_dur_minus = ui_create_adjust_btn(row_dur, "-", 60, 40);
    lv_obj_align(btn_dur_minus, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(btn_dur_minus, cb_duration_minus, LV_EVENT_CLICKED, NULL);

    /* Valeur durée (16px, cyan) */
    lbl_duration_val = lv_label_create(row_dur);
    lv_obj_set_style_text_font(lbl_duration_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_duration_val, cl_cyan, 0);
    lv_obj_align(lbl_duration_val, LV_ALIGN_BOTTOM_MID, 0, -8);
    refresh_duration_label();

    /* Bouton [+] durée */
    btn_dur_plus = ui_create_adjust_btn(row_dur, "+", 60, 40);
    lv_obj_align(btn_dur_plus, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(btn_dur_plus, cb_duration_plus, LV_EVENT_CLICKED, NULL);

    /* --- Barre d'action (RETOUR + DÉMARRER/ARRÊTER) --- */
    action_bar = action_bar_create(scr);
    action_bar_set_buttons(action_bar,
                           LV_SYMBOL_LEFT " RETOUR", cb_back,
                           LV_SYMBOL_PLAY " DEMARRER", cb_start_stop,
                           &style_btn_start);

    /* Mettre à jour selon l'état actuel */
    update_action_button();
    update_duration_controls();

    /* --- Timer de mise à jour périodique (1000ms) --- */
    update_timer = lv_timer_create(update_timer_cb, 1000, NULL);

    /* --- Transition animée --- */
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_SCREEN_FADE, 0, true);
}
