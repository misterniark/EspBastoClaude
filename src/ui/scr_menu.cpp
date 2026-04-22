/**
 * @file scr_menu.cpp
 * @brief Implémentation de l'écran menu principal
 *
 * Layout :
 *   - Header "WEBASTO" avec indicateurs (temp, chauffage, connexion, verrou)
 *   - 3 tuiles : Thermostat, Minuteur, Consigne
 *   - Bouton ARRÊTER en bas (visible uniquement si chauffage ON)
 *
 * Un timer LVGL (500ms) met à jour :
 *   - Les indicateurs du header
 *   - La visibilité du bouton ARRÊTER
 *   - L'état de verrouillage (grisage des tuiles)
 *   - Les alertes (connexion perdue, erreur capteur)
 */

#include "scr_menu.h"
#include "ui_common.h"
#include "ui_header.h"
#include "scr_alert.h"
#include "../hal/battery.h"
#include "../config.h"
#include "../core/heater_fsm.h"
#include "../hal/sensor.h"
#include "../comm/relay_link.h"
#include <lvgl.h>

/* Références vers les écrans de mode */
extern void scr_thermostat_create();
extern void scr_timer_create();
extern void scr_setpoint_create();

/* Widgets du menu (références pour mise à jour dynamique) */
static lv_obj_t *tile_thermo  = nullptr;
static lv_obj_t *tile_timer   = nullptr;
static lv_obj_t *tile_setpt   = nullptr;
static lv_obj_t *btn_stop     = nullptr;
static lv_timer_t *update_timer = nullptr;

/* ==========================================
 * Callbacks de navigation
 * ========================================== */

/**
 * Nettoie les ressources du menu avant de naviguer.
 * Supprime le timer de mise à jour pour éviter un crash
 * sur des objets LVGL détruits par la transition d'écran.
 */
static void cleanup_menu()
{
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = nullptr;
    }
    /* Invalider les pointeurs (l'écran sera détruit par lv_scr_load_anim) */
    tile_thermo = nullptr;
    tile_timer  = nullptr;
    tile_setpt  = nullptr;
    btn_stop    = nullptr;
}

static void on_tile_thermostat(lv_event_t *e)
{
    (void)e;
    if (heater_get_state() == HEATER_LOCKED) return;
    cleanup_menu();
    scr_thermostat_create();
}

static void on_tile_timer(lv_event_t *e)
{
    (void)e;
    if (heater_get_state() == HEATER_LOCKED) return;
    cleanup_menu();
    scr_timer_create();
}

static void on_tile_setpoint(lv_event_t *e)
{
    (void)e;
    if (heater_get_state() == HEATER_LOCKED) return;
    cleanup_menu();
    scr_setpoint_create();
}

/** Callback du bouton ARRÊTER : envoie HEAT_OFF */
static void on_stop(lv_event_t *e)
{
    (void)e;
    heater_request_off();
}

/* ==========================================
 * Création d'une tuile de mode
 * ========================================== */

/**
 * Crée une tuile cliquable pour un mode de fonctionnement.
 *
 * @param parent   Conteneur parent
 * @param title    Nom du mode (ex: "THERMOSTAT")
 * @param subtitle Description courte (ex: "Hysteresis auto")
 * @param y_pos    Position Y dans le conteneur
 * @param cb       Callback au clic
 * @return Pointeur vers la tuile créée
 */
static lv_obj_t* create_tile(lv_obj_t *parent, const char *title,
                             const char *subtitle, int y_pos, lv_event_cb_t cb)
{
    lv_obj_t *tile = lv_obj_create(parent);
    lv_obj_set_size(tile, SCREEN_WIDTH - 16, TILE_HEIGHT);
    lv_obj_set_pos(tile, 8, y_pos);
    lv_obj_add_style(tile, &style_card, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);

    /* Bordure gauche accent seule (style Muted Industrial) */
    lv_obj_set_style_border_side(tile, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(tile, 3, 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(COLOR_BORDER), 0);

    /* Style pressed : bordure gauche teal + fond légèrement éclairci */
    lv_obj_set_style_border_color(tile, cl_cyan, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x353434), LV_STATE_PRESSED);

    /* Titre du mode */
    lv_obj_t *lbl_title = lv_label_create(tile);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, cl_text, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 4);

    /* Sous-titre */
    lv_obj_t *lbl_sub = lv_label_create(tile);
    lv_label_set_text(lbl_sub, subtitle);
    lv_obj_set_style_text_color(lbl_sub, cl_text_dim, 0);
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_sub, LV_ALIGN_BOTTOM_LEFT, 0, -4);

    /* Callback de clic */
    lv_obj_add_event_cb(tile, cb, LV_EVENT_CLICKED, NULL);

    return tile;
}

/* ==========================================
 * Timer de mise à jour (500ms)
 * ========================================== */

/**
 * Met à jour les indicateurs et l'état visuel du menu.
 * Appelé toutes les 500ms par un lv_timer.
 */
static void update_cb(lv_timer_t *timer)
{
    (void)timer;

    /* Mettre à jour le header */
    header_set_temp(sensor_get_temperature());
    header_set_battery(battery_get_percent(), battery_is_available());
    header_set_heating(heater_get_state() == HEATER_HEATING);
    header_set_connected(relay_is_connected());
    header_set_locked(heater_get_state() == HEATER_LOCKED);

    /* Bouton ARRÊTER : visible uniquement si chauffage actif */
    if (btn_stop) {
        if (heater_get_state() == HEATER_HEATING) {
            lv_obj_clear_flag(btn_stop, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(btn_stop, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Verrouillage : griser les tuiles si LOCKED */
    bool locked = (heater_get_state() == HEATER_LOCKED);
    if (tile_thermo) ui_set_locked(tile_thermo, locked);
    if (tile_timer)  ui_set_locked(tile_timer, locked);
    if (tile_setpt)  ui_set_locked(tile_setpt, locked);

    /* Alertes prioritaires — nettoyer avant de changer d'écran */
    if (heater_has_connection_alert()) {
        cleanup_menu();
        scr_alert_connection_lost();
        return;
    }

    if (sensor_is_critical_error()) {
        cleanup_menu();
        scr_alert_sensor_error();
        return;
    }
}

/* ==========================================
 * Création de l'écran menu
 * ========================================== */

void scr_menu_create()
{
    /* Créer l'écran */
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, cl_bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Header */
    header_create(scr);
    header_set_title("WEBASTO");

    /* 3 tuiles de mode */
    int y_start = HEADER_HEIGHT + 10;
    int spacing = TILE_HEIGHT + 10;

    tile_thermo = create_tile(scr, "THERMOSTAT", "Hysteresis auto",
                              y_start, on_tile_thermostat);

    tile_timer = create_tile(scr, "MINUTEUR", "Duree programmee",
                             y_start + spacing, on_tile_timer);

    tile_setpt = create_tile(scr, "CONSIGNE", "Temperature cible",
                             y_start + spacing * 2, on_tile_setpoint);

    /* Bouton ARRÊTER (bas, masqué par défaut) */
    btn_stop = ui_create_btn(scr, "ARRETER", SCREEN_WIDTH - 20, BTN_HEIGHT);
    lv_obj_add_style(btn_stop, &style_btn_danger, 0);
    lv_obj_align(btn_stop, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btn_stop, on_stop, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(btn_stop, LV_OBJ_FLAG_HIDDEN);

    /* Timer de mise à jour (500ms) */
    update_timer = lv_timer_create(update_cb, 500, NULL);

    /* Mise à jour initiale */
    update_cb(NULL);

    /* Afficher avec transition fondu */
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_SCREEN_FADE, 0, true);
}
