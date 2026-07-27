/**
 * @file scr_thermostat.cpp
 * @brief Ecran du mode A — Thermostat avec hystérésis
 *
 * Layout :
 *   [HEADER 30px] Titre "THERMOSTAT" + indicateurs
 *   [CONTENU]     Arc température + réglages consigne/hystérésis
 *   [ACTION 60px] RETOUR | DÉMARRER/ARRÊTER
 *
 * L'arc affiche la température courante mappée sur 10-35°C.
 * Sa couleur change selon la zone thermique :
 *   - Rouge  : en dessous de (consigne - hystérésis) → doit chauffer
 *   - Ambre  : zone morte (entre seuil bas et consigne)
 *   - Vert   : au-dessus de la consigne → température OK
 *   - Cyan   : couleur par défaut quand le thermostat est inactif
 *
 * Les boutons +/- modifient consigne et hystérésis avec sauvegarde
 * différée (5 secondes d'inactivité avant écriture NVS).
 */

#include "scr_thermostat.h"
#include "ui_common.h"
#include "ui_header.h"
#include "ui_action_bar.h"
#include "../config.h"
#include "../core/heater_fsm.h"
#include "../core/mode_thermostat.h"
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

/** Barre d'action (pour mise à jour DÉMARRER/ARRÊTER) */
static lv_obj_t *action_bar = NULL;

/** Jauge arc de température */
static lv_obj_t *arc = NULL;

/** Label température au centre de l'arc (ex: "21.5°C") */
static lv_obj_t *lbl_temp = NULL;

/** Label cible sous la température (ex: "cible: 20.0°C") */
static lv_obj_t *lbl_target = NULL;

/** Label valeur de la consigne dans la rangée de réglage */
static lv_obj_t *lbl_setpoint_val = NULL;

/** Label valeur de l'hystérésis dans la rangée de réglage */
static lv_obj_t *lbl_hyst_val = NULL;

/** Consigne locale (copiée depuis storage au démarrage) */
static float local_setpoint = DEFAULT_SETPOINT;

/** Hystérésis locale */
static int local_hysteresis = DEFAULT_HYSTERESIS;

/** Timer LVGL de mise à jour périodique (500ms) */
static lv_timer_t *update_timer = NULL;

/** Timer LVGL de sauvegarde différée (5s après dernier réglage) */
static lv_timer_t *save_timer = NULL;

/* ==========================================
 * Prototypes des fonctions internes
 * ========================================== */
static void update_timer_cb(lv_timer_t *timer);
static void save_timer_cb(lv_timer_t *timer);
static void update_arc_color();
static void update_action_button();
static void refresh_setpoint_label();
static void refresh_hysteresis_label();
static void schedule_deferred_save();

/* ==========================================
 * Nettoyage de l'écran
 * ========================================== */

/**
 * Supprime les timers de l'écran. Si une sauvegarde différée est en
 * attente, elle est exécutée immédiatement (et non abandonnée) pour
 * ne pas perdre un réglage fait moins de 5 s avant de quitter l'écran.
 */
static void cleanup_thermostat()
{
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    if (save_timer) {
        /* Flush : écrire en NVS maintenant plutôt que de perdre le réglage */
        lv_timer_del(save_timer);
        save_timer = NULL;
        storage_save_setpoint(local_setpoint);
        storage_save_hysteresis(local_hysteresis);
    }
}

/**
 * Callback LV_EVENT_DELETE de l'écran : filet de sécurité quand
 * l'écran est remplacé sans passer par le bouton RETOUR — cas du
 * dispatcher d'alertes (ui_alerts) qui charge un écran d'alerte
 * par-dessus. Sans ce nettoyage, update_timer_cb continuerait de
 * tourner sur des widgets détruits (crash).
 * Le test sur `scr` évite de supprimer les timers d'une NOUVELLE
 * instance de l'écran créée avant la destruction effective de
 * l'ancienne (l'auto-delete arrive en fin d'animation de fondu).
 */
static void on_screen_delete(lv_event_t *e)
{
    if (lv_event_get_target(e) != scr) return;
    scr = NULL;
    cleanup_thermostat();
}

/* ==========================================
 * Callbacks des événements boutons
 * ========================================== */

/**
 * Callback bouton RETOUR : retour au menu principal.
 * Supprime les timers (avec flush de la sauvegarde différée)
 * avant de quitter l'écran.
 */
static void cb_back(lv_event_t *e)
{
    (void)e;
    cleanup_thermostat();
    scr_menu_create();
}

/**
 * Callback bouton DÉMARRER/ARRÊTER.
 * Bascule entre démarrage et arrêt du thermostat.
 */
static void cb_start_stop(lv_event_t *e)
{
    (void)e;
    if (thermostat_is_active()) {
        /* Arrêter le thermostat et demander l'arrêt du chauffage */
        thermostat_stop();
        heater_request_off();
    } else {
        /* Démarrer le thermostat avec les paramètres locaux.
         * Refus possible (garde I4) : capteur en erreur, ou mesure
         * obsolète juste après un réveil d'écran — dans ce dernier cas
         * une lecture fraîche est déjà en cours (forcée au réveil),
         * un nouvel appui ~1 s plus tard réussira. */
        if (!thermostat_start(local_setpoint, local_hysteresis)) {
            ui_toast(sensor_is_error() ? "Capteur indisponible"
                                       : "Mesure en cours...");
        }
    }
    /* Mise à jour immédiate du bouton */
    update_action_button();
}

/**
 * Callback bouton [-] consigne : diminue la consigne de SETPOINT_STEP.
 */
static void cb_setpoint_minus(lv_event_t *e)
{
    (void)e;
    if (local_setpoint > SETPOINT_MIN) {
        local_setpoint -= SETPOINT_STEP;
        /* Clamper à la borne minimale */
        if (local_setpoint < SETPOINT_MIN) local_setpoint = SETPOINT_MIN;
        thermostat_set_setpoint(local_setpoint);
        refresh_setpoint_label();
        schedule_deferred_save();
    }
}

/**
 * Callback bouton [+] consigne : augmente la consigne de SETPOINT_STEP.
 */
static void cb_setpoint_plus(lv_event_t *e)
{
    (void)e;
    if (local_setpoint < SETPOINT_MAX) {
        local_setpoint += SETPOINT_STEP;
        if (local_setpoint > SETPOINT_MAX) local_setpoint = SETPOINT_MAX;
        thermostat_set_setpoint(local_setpoint);
        refresh_setpoint_label();
        schedule_deferred_save();
    }
}

/**
 * Callback bouton [-] hystérésis : diminue l'hystérésis de HYSTERESIS_STEP.
 */
static void cb_hyst_minus(lv_event_t *e)
{
    (void)e;
    if (local_hysteresis > HYSTERESIS_MIN) {
        local_hysteresis -= HYSTERESIS_STEP;
        if (local_hysteresis < HYSTERESIS_MIN) local_hysteresis = HYSTERESIS_MIN;
        thermostat_set_hysteresis(local_hysteresis);
        refresh_hysteresis_label();
        schedule_deferred_save();
    }
}

/**
 * Callback bouton [+] hystérésis : augmente l'hystérésis de HYSTERESIS_STEP.
 */
static void cb_hyst_plus(lv_event_t *e)
{
    (void)e;
    if (local_hysteresis < HYSTERESIS_MAX) {
        local_hysteresis += HYSTERESIS_STEP;
        if (local_hysteresis > HYSTERESIS_MAX) local_hysteresis = HYSTERESIS_MAX;
        thermostat_set_hysteresis(local_hysteresis);
        refresh_hysteresis_label();
        schedule_deferred_save();
    }
}

/* ==========================================
 * Fonctions utilitaires internes
 * ========================================== */

/**
 * Met à jour le label de la consigne avec la valeur locale.
 *
 * NOTE : formatage via le snprintf de la libc et non
 * lv_label_set_text_fmt — le printf interne de LVGL ne supporte pas
 * les flottants (LV_SPRINTF_USE_FLOAT désactivé) et afficherait
 * littéralement « f » à la place de la valeur.
 */
static void refresh_setpoint_label()
{
    if (lbl_setpoint_val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f\xC2\xB0""C", local_setpoint);
        lv_label_set_text(lbl_setpoint_val, buf);
    }
}

/**
 * Met à jour le label de l'hystérésis avec la valeur locale.
 */
static void refresh_hysteresis_label()
{
    if (lbl_hyst_val) {
        lv_label_set_text_fmt(lbl_hyst_val, "%d\xC2\xB0""C", local_hysteresis);
    }
}

/**
 * Programme (ou reprogramme) la sauvegarde différée.
 * La sauvegarde NVS se déclenche 5 secondes après le dernier
 * appui sur un bouton +/-, pour éviter des écritures flash trop fréquentes.
 */
static void schedule_deferred_save()
{
    if (save_timer) {
        /* Réinitialiser le compteur du timer existant */
        lv_timer_reset(save_timer);
    } else {
        /* Créer un timer one-shot de 5 secondes */
        save_timer = lv_timer_create(save_timer_cb, 5000, NULL);
        lv_timer_set_repeat_count(save_timer, 1);
    }
}

/**
 * Callback du timer de sauvegarde différée.
 * Ecrit la consigne et l'hystérésis en NVS.
 */
static void save_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    storage_save_setpoint(local_setpoint);
    storage_save_hysteresis(local_hysteresis);
    save_timer = NULL;
}

/**
 * Met à jour la couleur de l'arc selon la zone thermique.
 * - Thermostat inactif : cyan
 * - Sous (consigne - hystérésis) : rouge (doit chauffer)
 * - Zone morte : ambre
 * - Au-dessus de la consigne : vert (température OK)
 */
static void update_arc_color()
{
    lv_color_t color;

    if (!thermostat_is_active()) {
        /* Mode inactif : cyan par défaut */
        color = cl_cyan;
    } else {
        float temp = sensor_get_temperature();
        float seuil_bas = local_setpoint - (float)local_hysteresis;

        if (temp < seuil_bas) {
            /* En dessous du seuil bas → rouge (chauffage nécessaire) */
            color = cl_red;
        } else if (temp < local_setpoint) {
            /* Zone morte → ambre */
            color = cl_amber;
        } else {
            /* Au-dessus de la consigne → vert */
            color = cl_green;
        }
    }

    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
}

/**
 * Met à jour le texte et le style du bouton d'action droit
 * selon l'état du thermostat et du chauffage.
 */
static void update_action_button()
{
    if (!action_bar) return;

    if (thermostat_is_active()) {
        action_bar_update_right(action_bar, LV_SYMBOL_STOP " ARRETER", &style_btn_stop);
    } else {
        action_bar_update_right(action_bar, LV_SYMBOL_PLAY " DEMARRER", &style_btn_start);
    }
}

/**
 * Callback du timer de mise à jour périodique (500ms).
 * Actualise la température affichée, la valeur de l'arc,
 * les indicateurs du header et l'état du bouton d'action.
 */
static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    float temp = sensor_get_temperature();
    bool heating = (heater_get_state() == HEATER_HEATING);

    /* Mise à jour du label température central (snprintf libc : le
     * printf LVGL ne gère pas %f, voir refresh_setpoint_label) */
    if (lbl_temp) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f\xC2\xB0""C", temp);
        lv_label_set_text(lbl_temp, buf);
    }

    /* Mise à jour du label cible */
    if (lbl_target) {
        char buf[24];
        snprintf(buf, sizeof(buf), "cible: %.1f\xC2\xB0""C", local_setpoint);
        lv_label_set_text(lbl_target, buf);
    }

    /* Mise à jour de la valeur de l'arc (mappée 10-35°C) */
    if (arc) {
        int arc_val = (int)((temp - SETPOINT_MIN) / (SETPOINT_MAX - SETPOINT_MIN) * 100);
        if (arc_val < 0)   arc_val = 0;
        if (arc_val > 100) arc_val = 100;
        lv_arc_set_value(arc, arc_val);
    }

    /* Mise à jour de la couleur de l'arc selon la zone thermique */
    update_arc_color();

    /* Mise à jour des indicateurs du header */
    header_set_temp(temp);
    header_set_heating(heating);
    header_set_connected(relay_is_connected());
    header_set_locked(heater_get_state() == HEATER_LOCKED);

    /* Mise à jour du bouton d'action */
    update_action_button();
}

/* ==========================================
 * Création de l'écran
 * ========================================== */

void scr_thermostat_create()
{
    /* --- Charger les valeurs sauvegardées --- */
    local_setpoint = storage_load_setpoint();
    local_hysteresis = storage_load_hysteresis();

    /* --- Création de l'écran principal --- */
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, cl_bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Nettoyage des timers si l'écran est détruit par un tiers
     * (dispatcher d'alertes) — voir on_screen_delete */
    lv_obj_add_event_cb(scr, on_screen_delete, LV_EVENT_DELETE, NULL);

    /* --- Header (bandeau haut 30px) --- */
    header_create(scr);
    header_set_title("THERMOSTAT");

    /* --- Zone de contenu (entre header et barre d'action) --- */
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, SCREEN_WIDTH, CONTENT_HEIGHT);
    lv_obj_set_pos(content, 0, HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(content, 4, 0);
    lv_obj_set_style_pad_row(content, 2, 0);

    /* --- Arc de température (120px, épaisseur 10px) --- */
    /* Conteneur pour positionner l'arc et les labels centraux */
    lv_obj_t *arc_cont = lv_obj_create(content);
    lv_obj_set_size(arc_cont, ARC_SIZE, ARC_SIZE - 10);
    lv_obj_set_style_bg_opa(arc_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(arc_cont, 0, 0);
    lv_obj_set_style_pad_all(arc_cont, 0, 0);
    lv_obj_clear_flag(arc_cont, LV_OBJ_FLAG_SCROLLABLE);

    arc = lv_arc_create(arc_cont);
    lv_obj_set_size(arc, ARC_SIZE, ARC_SIZE);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 50);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    /* Fond de l'arc (partie non remplie) */
    lv_obj_set_style_arc_width(arc, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, cl_border, LV_PART_MAIN);
    /* Indicateur de l'arc (partie remplie) */
    lv_obj_set_style_arc_width(arc, ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, cl_cyan, LV_PART_INDICATOR);

    /* Label température au centre de l'arc (police 24px) */
    lbl_temp = lv_label_create(arc_cont);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_temp, cl_text, 0);
    lv_label_set_text(lbl_temp, "--.-\xC2\xB0""C");
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, -8);

    /* Label cible sous la température (police 12px, dim) */
    lbl_target = lv_label_create(arc_cont);
    lv_obj_set_style_text_font(lbl_target, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_target, cl_text_dim, 0);
    {
        /* snprintf libc : le printf LVGL ne gère pas %f */
        char buf[24];
        snprintf(buf, sizeof(buf), "cible: %.1f\xC2\xB0""C", local_setpoint);
        lv_label_set_text(lbl_target, buf);
    }
    lv_obj_align(lbl_target, LV_ALIGN_CENTER, 0, 12);

    /* --- Rangée CONSIGNE : label + [-] + valeur + [+] --- */
    lv_obj_t *row_sp = lv_obj_create(content);
    lv_obj_set_size(row_sp, SCREEN_WIDTH - 16, 44);
    lv_obj_set_style_bg_opa(row_sp, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_sp, 0, 0);
    lv_obj_set_style_pad_all(row_sp, 0, 0);
    lv_obj_clear_flag(row_sp, LV_OBJ_FLAG_SCROLLABLE);

    /* Label titre de la rangée */
    lv_obj_t *lbl_sp_title = lv_label_create(row_sp);
    lv_label_set_text(lbl_sp_title, "CONSIGNE");
    lv_obj_set_style_text_font(lbl_sp_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_sp_title, cl_text_dim, 0);
    lv_obj_align(lbl_sp_title, LV_ALIGN_TOP_MID, 0, 0);

    /* Bouton [-] consigne */
    lv_obj_t *btn_sp_minus = ui_create_adjust_btn(row_sp, "-", 60, 40);
    lv_obj_align(btn_sp_minus, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(btn_sp_minus, cb_setpoint_minus, LV_EVENT_CLICKED, NULL);

    /* Valeur consigne (16px, cyan) */
    lbl_setpoint_val = lv_label_create(row_sp);
    lv_obj_set_style_text_font(lbl_setpoint_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_setpoint_val, cl_cyan, 0);
    lv_obj_align(lbl_setpoint_val, LV_ALIGN_BOTTOM_MID, 0, -8);
    refresh_setpoint_label();

    /* Bouton [+] consigne */
    lv_obj_t *btn_sp_plus = ui_create_adjust_btn(row_sp, "+", 60, 40);
    lv_obj_align(btn_sp_plus, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(btn_sp_plus, cb_setpoint_plus, LV_EVENT_CLICKED, NULL);

    /* --- Rangée HYSTERESIS : label + [-] + valeur + [+] --- */
    lv_obj_t *row_hy = lv_obj_create(content);
    lv_obj_set_size(row_hy, SCREEN_WIDTH - 16, 44);
    lv_obj_set_style_bg_opa(row_hy, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_hy, 0, 0);
    lv_obj_set_style_pad_all(row_hy, 0, 0);
    lv_obj_clear_flag(row_hy, LV_OBJ_FLAG_SCROLLABLE);

    /* Label titre de la rangée */
    lv_obj_t *lbl_hy_title = lv_label_create(row_hy);
    lv_label_set_text(lbl_hy_title, "HYSTERESIS");
    lv_obj_set_style_text_font(lbl_hy_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_hy_title, cl_text_dim, 0);
    lv_obj_align(lbl_hy_title, LV_ALIGN_TOP_MID, 0, 0);

    /* Bouton [-] hystérésis */
    lv_obj_t *btn_hy_minus = ui_create_adjust_btn(row_hy, "-", 60, 40);
    lv_obj_align(btn_hy_minus, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(btn_hy_minus, cb_hyst_minus, LV_EVENT_CLICKED, NULL);

    /* Valeur hystérésis (16px, cyan) */
    lbl_hyst_val = lv_label_create(row_hy);
    lv_obj_set_style_text_font(lbl_hyst_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_hyst_val, cl_cyan, 0);
    lv_obj_align(lbl_hyst_val, LV_ALIGN_BOTTOM_MID, 0, -8);
    refresh_hysteresis_label();

    /* Bouton [+] hystérésis */
    lv_obj_t *btn_hy_plus = ui_create_adjust_btn(row_hy, "+", 60, 40);
    lv_obj_align(btn_hy_plus, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(btn_hy_plus, cb_hyst_plus, LV_EVENT_CLICKED, NULL);

    /* --- Barre d'action (RETOUR + DÉMARRER/ARRÊTER) --- */
    action_bar = action_bar_create(scr);
    action_bar_set_buttons(action_bar,
                           LV_SYMBOL_LEFT " RETOUR", cb_back,
                           LV_SYMBOL_PLAY " DEMARRER", cb_start_stop,
                           &style_btn_start);

    /* Mettre à jour le bouton selon l'état actuel */
    update_action_button();

    /* --- Timer de mise à jour périodique (500ms) --- */
    update_timer = lv_timer_create(update_timer_cb, 500, NULL);

    /* --- Transition animée vers ce nouvel écran --- */
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_SCREEN_FADE, 0, true);
}
