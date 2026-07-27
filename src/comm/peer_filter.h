/**
 * @file peer_filter.h
 * @brief Filtrage de la source des messages ESP-NOW — logique pure
 *
 * PROBLÈME RÉSOLU (audit de production du 27/07/2026, constat
 * bloquant) : ni le contrôleur ni le relais ne vérifiaient l'adresse
 * MAC de l'expéditeur. Le relais exécutait donc TOUTE trame reçue, de
 * n'importe quelle source — une trame d'un octet suffisait à allumer un
 * appareil à combustion la nuit, van inoccupé. Symétriquement, le
 * contrôleur acceptait les ACK de n'importe qui (état falsifiable).
 *
 * Le scénario le plus probable n'est d'ailleurs pas malveillant : deux
 * vans équipés du même kit stationnés côte à côte. La découverte se
 * faisant par diffusion, un contrôleur pouvait adopter le relais du
 * voisin et piloter SON Webasto.
 *
 * RÈGLE : une fois l'appairage fait, n'accepter que l'appareil appairé.
 * Tant qu'aucun appairage n'existe, accepter la première réponse venue
 * — c'est le seul moyen de se découvrir, et cette fenêtre est bornée
 * (elle se referme dès le premier échange réussi).
 *
 * NOTE de sécurité : le filtrage par MAC seul reste contournable par
 * usurpation d'adresse. Il est complété par le chiffrement ESP-NOW
 * (PMK/LMK) ; ce filtre reste utile car il s'applique AUSSI aux trames
 * en clair de la phase de découverte.
 *
 * Header pur (aucune dépendance Arduino) : testé par
 * test/test_peer_filter/.
 */

#ifndef COMM_PEER_FILTER_H
#define COMM_PEER_FILTER_H

#include <stdint.h>

/** Longueur d'une adresse MAC, en octets. */
constexpr int PEER_MAC_LEN = 6;

/** Compare deux adresses MAC. */
inline bool peer_mac_equal(const uint8_t *a, const uint8_t *b)
{
    for (int i = 0; i < PEER_MAC_LEN; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

/**
 * Décide si un message reçu doit être traité.
 *
 * @param paired true si un appareil est déjà appairé (MAC connue)
 * @param known  MAC de l'appareil appairé (ignorée si !paired)
 * @param src    MAC de l'expéditeur du message reçu
 * @return true si le message doit être traité
 */
inline bool peer_source_accepted(bool paired, const uint8_t *known,
                                 const uint8_t *src)
{
    if (!paired) return true;  /* Découverte : première réponse acceptée */
    return peer_mac_equal(known, src);
}

#endif /* COMM_PEER_FILTER_H */
