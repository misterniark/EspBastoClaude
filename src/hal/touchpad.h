/**
 * @file touchpad.h
 * @brief Driver tactile pour LVGL (deux backends selon le matériel)
 *
 *   - CYD : XPT2046 résistif sur bus SPI dédié (VSPI),
 *     avec calibration et anti-rebond.
 *   - CrowPanel (-DHW_CROWPANEL) : FT6236 capacitif sur I2C
 *     (adresse 0x38), coordonnées natives en pixels.
 *
 * Les deux backends intègrent le filtre "ignorer jusqu'au relâchement"
 * (touch_gate.h) utilisé pour consommer réellement le toucher de
 * réveil de l'écran.
 */

#ifndef HAL_TOUCHPAD_H
#define HAL_TOUCHPAD_H

/**
 * Initialise le contrôleur tactile et enregistre le driver d'entrée LVGL.
 * Doit être appelé après hal_display_init() et lv_init().
 * Sur CrowPanel : initialise aussi le bus I2C partagé (Wire, SDA=15,
 * SCL=16) utilisé ensuite par la jauge batterie MAX17048.
 */
void hal_touchpad_init();

/**
 * Indique si un toucher est actuellement détecté (état brut du
 * contrôleur, avant calibration/filtrage).
 * Utile pour le réveil de l'écran.
 */
bool hal_touchpad_is_touched();

/**
 * Masque tous les contacts pour LVGL jusqu'au prochain relâchement
 * complet du doigt. À appeler au réveil de l'écran pour que le
 * toucher de réveil ne déclenche aucune action UI (le doigt est
 * encore posé quand LVGL reprend son rendu).
 */
void hal_touchpad_ignore_until_release();

#endif /* HAL_TOUCHPAD_H */
