/**
 * @file touch_gate.h
 * @brief Filtre pur "ignorer jusqu'au relâchement" pour le tactile
 *
 * Logique sans dépendance Arduino/matériel, testable nativement
 * (pio test -e native).
 *
 * Problème résolu : au réveil de la veille écran, le doigt de
 * l'utilisateur est encore posé sur la dalle quand LVGL reprend son
 * rendu. Lire simplement l'état du contrôleur tactile ne "consomme"
 * rien (les registres d'état rapportent le contact tant que le doigt
 * est posé) : LVGL verrait un PRESSED puis un RELEASED et émettrait
 * un LV_EVENT_CLICKED sur le widget situé sous le doigt — sur ce
 * contrôleur de chauffage, le tap de réveil pourrait activer un
 * bouton DÉMARRER/ARRÊTER affiché avant la veille.
 *
 * Solution : au réveil, le gestionnaire d'énergie arme ce filtre.
 * Tant qu'un relâchement complet n'a pas été observé, tous les
 * contacts sont masqués (rapportés RELEASED à LVGL).
 */

#ifndef HAL_TOUCH_GATE_H
#define HAL_TOUCH_GATE_H

/** État du filtre : ignoring = true tant que le relâchement
 *  du toucher de réveil n'a pas été observé. */
struct TouchGate {
    bool ignoring = false;
};

/** Arme le filtre : les contacts seront masqués jusqu'au
 *  prochain relâchement complet. À appeler au réveil de l'écran. */
inline void touch_gate_arm(TouchGate &gate)
{
    gate.ignoring = true;
}

/**
 * Filtre un échantillon tactile.
 *
 * @param gate    État du filtre (modifié en place)
 * @param touched true si le contrôleur rapporte un contact actif
 * @return true si le contact doit être transmis à LVGL,
 *         false s'il doit être masqué (rapporter RELEASED)
 */
inline bool touch_gate_pass(TouchGate &gate, bool touched)
{
    if (gate.ignoring) {
        /* Le relâchement met fin au filtrage ; le contact courant
         * (toucher de réveil) reste masqué dans tous les cas. */
        if (!touched) gate.ignoring = false;
        return false;
    }
    return touched;
}

#endif /* HAL_TOUCH_GATE_H */
