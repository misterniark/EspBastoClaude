/**
 * @file touch_mapping.h
 * @brief Transformation pure des coordonnées tactiles brutes → écran
 *
 * Fonction sans dépendance Arduino/matériel, ce qui permet de la
 * tester nativement (pio test -e native).
 *
 * Utilisée par le driver FT6236 (CrowPanel) : le contrôleur renvoie
 * des coordonnées natives portrait (240x320) qui peuvent nécessiter
 * un échange d'axes et/ou un miroir selon le montage de la dalle.
 * Les drapeaux sont définis dans config.h (TOUCH_SWAP_XY,
 * TOUCH_MIRROR_X, TOUCH_MIRROR_Y).
 */

#ifndef HAL_TOUCH_MAPPING_H
#define HAL_TOUCH_MAPPING_H

/* stdint.h plutôt que <cstdint> : portable entre le toolchain Xtensa
 * (firmware) et le compilateur natif macOS/Linux (tests unitaires) */
#include <stdint.h>

/** Point tactile en coordonnées écran (pixels). */
struct TouchPoint {
    int16_t x;
    int16_t y;
};

/**
 * Transforme un point tactile brut en coordonnées écran.
 *
 * Ordre des opérations :
 *   1. Échange des axes X/Y (si la dalle est câblée en paysage)
 *   2. Miroir horizontal et/ou vertical
 *   3. Bornage dans [0, largeur-1] x [0, hauteur-1]
 *
 * @param raw_x    Coordonnée X brute renvoyée par le contrôleur
 * @param raw_y    Coordonnée Y brute renvoyée par le contrôleur
 * @param screen_w Largeur de l'écran en pixels
 * @param screen_h Hauteur de l'écran en pixels
 * @param swap_xy  true = échanger X et Y avant le miroir
 * @param mirror_x true = inverser l'axe X (x devient largeur-1-x)
 * @param mirror_y true = inverser l'axe Y (y devient hauteur-1-y)
 * @return Point en coordonnées écran, borné aux dimensions
 */
inline TouchPoint touch_map_point(int16_t raw_x, int16_t raw_y,
                                  int16_t screen_w, int16_t screen_h,
                                  bool swap_xy, bool mirror_x, bool mirror_y)
{
    int16_t x = raw_x;
    int16_t y = raw_y;

    /* 1. Échange des axes si la dalle est orientée différemment de l'écran */
    if (swap_xy) {
        int16_t tmp = x;
        x = y;
        y = tmp;
    }

    /* 2. Miroir : inverse le sens d'un axe */
    if (mirror_x) x = static_cast<int16_t>(screen_w - 1 - x);
    if (mirror_y) y = static_cast<int16_t>(screen_h - 1 - y);

    /* 3. Bornage : jamais de coordonnée hors écran (protège LVGL) */
    if (x < 0) x = 0;
    if (x >= screen_w) x = static_cast<int16_t>(screen_w - 1);
    if (y < 0) y = 0;
    if (y >= screen_h) y = static_cast<int16_t>(screen_h - 1);

    return TouchPoint{x, y};
}

#endif /* HAL_TOUCH_MAPPING_H */
