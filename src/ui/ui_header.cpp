/**
 * @file ui_header.cpp
 * @brief Implémentation du widget header
 *
 * Layout : [TITRE]                    [TEMP] [HEAT] [CONN] [LOCK]
 * Fond : #0A0A0F avec ligne cyan 1px en bas
 * Hauteur : 30px
 *
 * L'animation de la flamme utilise lv_anim pour faire osciller
 * l'opacité entre 100% et 60% toutes les 1200ms.
 */

#include "ui_header.h"
#include "ui_common.h"
#include "../config.h"
#include <cstdio>

/* Labels du header (références globales pour mise à jour) */
static lv_obj_t *lbl_title    = nullptr;
static lv_obj_t *lbl_temp     = nullptr;
static lv_obj_t *lbl_batt     = nullptr;
static lv_obj_t *lbl_heat     = nullptr;
static lv_obj_t *lbl_conn     = nullptr;
static lv_obj_t *lbl_lock     = nullptr;

/* Animation de la flamme */
static lv_anim_t flame_anim;
static bool flame_anim_running = false;

/**
 * Callback d'animation pour la pulsation de la flamme.
 * Fait osciller l'opacité du label chauffage.
 */
static void flame_anim_cb(void *obj, int32_t value)
{
    lv_obj_set_style_text_opa((lv_obj_t *)obj, value, 0);
}

/** Démarre l'animation de pulsation de la flamme. */
static void flame_start_anim()
{
    if (flame_anim_running || !lbl_heat) return;

    lv_anim_init(&flame_anim);
    lv_anim_set_var(&flame_anim, lbl_heat);
    lv_anim_set_exec_cb(&flame_anim, flame_anim_cb);
    lv_anim_set_values(&flame_anim, OPA_FULL, OPA_FLAME_LOW);
    lv_anim_set_time(&flame_anim, ANIM_FLAME / 2);
    lv_anim_set_playback_time(&flame_anim, ANIM_FLAME / 2);
    lv_anim_set_repeat_count(&flame_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&flame_anim);

    flame_anim_running = true;
}

/** Arrête l'animation de pulsation de la flamme. */
static void flame_stop_anim()
{
    if (!flame_anim_running || !lbl_heat) return;

    lv_anim_del(lbl_heat, flame_anim_cb);
    lv_obj_set_style_text_opa(lbl_heat, LV_OPA_COVER, 0);
    flame_anim_running = false;
}

lv_obj_t* header_create(lv_obj_t *parent)
{
    /* Nouveau bandeau : l'ancien label de chauffage a été détruit avec
     * son écran, et l'animation qui le faisait pulser est morte avec
     * lui. Sans cette remise à zéro, le drapeau restait à « en cours »
     * et flame_start_anim() se désistait : la flamme ne pulsait plus
     * jamais après une navigation en cours de chauffe. */
    flame_anim_running = false;

    /* Conteneur header */
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, cl_bg_header, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_pad_left(header, 6, 0);
    lv_obj_set_style_pad_right(header, 6, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* Ligne de séparation sobre en bas du header (bordure, pas cyan) */
    static lv_point_t line_points[] = {{0, 0}, {SCREEN_WIDTH, 0}};
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, line_points, 2);
    lv_obj_set_style_line_color(line, cl_border, 0);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, HEADER_HEIGHT);

    /* Titre (aligné à gauche, style monospace industriel) */
    lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, "WEBASTO");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_title, cl_text_dim, 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    /* Indicateurs (alignés à droite, de droite à gauche) */

    /*
     * Indicateurs (alignés à droite, de droite à gauche) :
     *   [TITLE]          [TEMP] [BATT] [HEAT] [CONN] [LOCK]
     *                     -90    -58    -40    -20     0
     */

    /* Verrou (le plus à droite, masqué par défaut) */
    lbl_lock = lv_label_create(header);
    lv_label_set_text(lbl_lock, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_color(lbl_lock, cl_red, 0);
    lv_obj_set_style_text_font(lbl_lock, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_lock, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(lbl_lock, LV_OBJ_FLAG_HIDDEN);

    /* Connexion */
    lbl_conn = lv_label_create(header);
    lv_label_set_text(lbl_conn, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_conn, cl_green, 0);
    lv_obj_set_style_text_font(lbl_conn, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_conn, LV_ALIGN_RIGHT_MID, -20, 0);

    /* Chauffage (masqué par défaut) */
    lbl_heat = lv_label_create(header);
    lv_label_set_text(lbl_heat, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(lbl_heat, cl_amber, 0);
    lv_obj_set_style_text_font(lbl_heat, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_heat, LV_ALIGN_RIGHT_MID, -40, 0);
    lv_obj_add_flag(lbl_heat, LV_OBJ_FLAG_HIDDEN);

    /* Batterie (pourcentage compact, ex: "78%") */
    lbl_batt = lv_label_create(header);
    lv_label_set_text(lbl_batt, "--%");
    lv_obj_set_style_text_color(lbl_batt, cl_text_dim, 0);
    lv_obj_set_style_text_font(lbl_batt, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_batt, LV_ALIGN_RIGHT_MID, -58, 0);

    /* Température */
    lbl_temp = lv_label_create(header);
    lv_label_set_text(lbl_temp, "--.-\xC2\xB0""C");
    lv_obj_set_style_text_color(lbl_temp, cl_text, 0);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_RIGHT_MID, -90, 0);

    return header;
}

void header_set_title(const char *title)
{
    if (lbl_title) {
        lv_label_set_text(lbl_title, title);
    }
}

void header_set_temp(float temp)
{
    if (lbl_temp) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f\xC2\xB0""C", temp);
        lv_label_set_text(lbl_temp, buf);
    }
}

void header_set_heating(bool on)
{
    if (!lbl_heat) return;

    if (on) {
        lv_obj_clear_flag(lbl_heat, LV_OBJ_FLAG_HIDDEN);
        flame_start_anim();
    } else {
        flame_stop_anim();
        lv_obj_add_flag(lbl_heat, LV_OBJ_FLAG_HIDDEN);
    }
}

void header_set_connected(bool connected)
{
    if (!lbl_conn) return;

    if (connected) {
        lv_obj_set_style_text_color(lbl_conn, cl_green, 0);
    } else {
        lv_obj_set_style_text_color(lbl_conn, cl_red, 0);
    }
}

void header_set_locked(bool locked)
{
    if (!lbl_lock) return;

    if (locked) {
        lv_obj_clear_flag(lbl_lock, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_lock, LV_OBJ_FLAG_HIDDEN);
    }
}

void header_set_battery(float percent, bool available)
{
    if (!lbl_batt) return;

    if (!available) {
        lv_label_set_text(lbl_batt, "--%");
        lv_obj_set_style_text_color(lbl_batt, cl_text_dim, 0);
        return;
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f%%", percent);
    lv_label_set_text(lbl_batt, buf);

    /* Couleur selon le niveau : vert > 20%, or brun > 10%, rouge < 10% */
    if (percent > 20.0f) {
        lv_obj_set_style_text_color(lbl_batt, cl_text_dim, 0);
    } else if (percent > 10.0f) {
        lv_obj_set_style_text_color(lbl_batt, cl_amber, 0);
    } else {
        lv_obj_set_style_text_color(lbl_batt, cl_red, 0);
    }
}
