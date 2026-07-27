/**
 * @file ui_common.h
 * @brief Thème LVGL, styles globaux et utilitaires UI
 *
 * Définit le thème cockpit moderne épuré avec les styles réutilisables
 * pour l'ensemble de l'interface. Initialise les couleurs et les styles
 * LVGL à partir des constantes de config.h.
 */

#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <lvgl.h>

/* ==========================================
 * Couleurs LVGL pré-calculées
 * ========================================== */
extern lv_color_t cl_bg;
extern lv_color_t cl_bg_header;
extern lv_color_t cl_card;
extern lv_color_t cl_border;
extern lv_color_t cl_cyan;
extern lv_color_t cl_amber;
extern lv_color_t cl_red;
extern lv_color_t cl_green;
extern lv_color_t cl_text;
extern lv_color_t cl_text_dim;

/* ==========================================
 * Styles LVGL réutilisables
 * ========================================== */

/** Style des cartes / tuiles (fond #1A1A2E, radius 12, ombre) */
extern lv_style_t style_card;

/** Style des boutons (fond #1A1A2E, radius 8, bordure) */
extern lv_style_t style_btn;

/** Style des boutons appuyés */
extern lv_style_t style_btn_pressed;

/** Style des boutons désactivés (verrouillage) */
extern lv_style_t style_btn_disabled;

/** Style du bouton DÉMARRER (bordure cyan) */
extern lv_style_t style_btn_start;

/** Style du bouton ARRÊTER (bordure ambre) */
extern lv_style_t style_btn_stop;

/** Style du bouton danger (bordure rouge) */
extern lv_style_t style_btn_danger;

/* ==========================================
 * Fonctions
 * ========================================== */

/**
 * Initialise le thème LVGL et tous les styles globaux.
 * Doit être appelé après lv_init() et hal_display_init().
 */
void ui_common_init();

/**
 * Applique ou retire l'état verrouillé sur un objet LVGL.
 * En mode verrouillé, l'objet devient semi-transparent (30%)
 * et non cliquable.
 *
 * @param obj    Objet LVGL à modifier
 * @param locked true pour verrouiller, false pour déverrouiller
 */
void ui_set_locked(lv_obj_t *obj, bool locked);

/**
 * Crée un bouton standard avec label centré.
 * Applique automatiquement le style style_btn.
 *
 * @param parent Conteneur parent
 * @param text   Texte du bouton
 * @param width  Largeur en pixels
 * @param height Hauteur en pixels
 * @return Pointeur vers le bouton créé
 */
lv_obj_t* ui_create_btn(lv_obj_t *parent, const char *text, int width, int height);

/**
 * Crée un bouton +/- pour le réglage de valeur.
 *
 * @param parent Conteneur parent
 * @param text   "+" ou "-"
 * @param width  Largeur
 * @param height Hauteur
 * @return Pointeur vers le bouton créé
 */
lv_obj_t* ui_create_adjust_btn(lv_obj_t *parent, const char *text, int width, int height);

/**
 * Affiche un message temporaire (toast) en bas de l'écran, au-dessus
 * de la barre d'action. Disparaît automatiquement après 2 secondes.
 * Un nouvel appel remplace le toast en cours et relance le délai.
 *
 * Créé sur lv_layer_top() : survit aux changements d'écran et reste
 * visible par-dessus tous les widgets.
 *
 * Utilisé notamment quand le démarrage d'un mode est refusé parce que
 * la mesure de température n'est pas encore fraîche (réveil d'écran).
 *
 * @param text Message à afficher (copié par LVGL)
 */
void ui_toast(const char *text);

#endif /* UI_COMMON_H */
