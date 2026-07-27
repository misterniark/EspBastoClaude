/**
 * @file test_main.cpp
 * @brief Tests unitaires du filtrage de source ESP-NOW
 *        (comm/peer_filter.h)
 *
 * Constat bloquant de l'audit du 27/07/2026 : aucune vérification de
 * l'adresse MAC de l'expéditeur. Le relais exécutait toute trame reçue,
 * de n'importe quelle source ; le contrôleur acceptait n'importe quel
 * ACK. Le scénario le plus probable n'est pas malveillant : deux vans
 * équipés du même kit, côte à côte.
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/comm/peer_filter.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t RELAIS[6]  = {0x3C, 0xDC, 0x75, 0xAE, 0xCD, 0xA4};
static const uint8_t VOISIN[6]  = {0x3C, 0xDC, 0x75, 0xAE, 0xCD, 0xA5};
static const uint8_t INCONNU[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

/* --- Comparaison de MAC --- */
static void test_mac_equal(void)
{
    TEST_ASSERT_TRUE(peer_mac_equal(RELAIS, RELAIS));
    TEST_ASSERT_FALSE(peer_mac_equal(RELAIS, INCONNU));
    /* Différence sur le DERNIER octet seulement : cas réaliste de deux
     * cartes d'une même série (kit du van voisin) */
    TEST_ASSERT_FALSE(peer_mac_equal(RELAIS, VOISIN));
}

/* --- Non appairé : accepter la première réponse (découverte) --- */
static void test_unpaired_accepts_anyone(void)
{
    TEST_ASSERT_TRUE(peer_source_accepted(false, RELAIS, INCONNU));
    TEST_ASSERT_TRUE(peer_source_accepted(false, RELAIS, RELAIS));
}

/* --- Appairé : n'accepter que l'appareil appairé --- */
static void test_paired_accepts_only_known(void)
{
    TEST_ASSERT_TRUE(peer_source_accepted(true, RELAIS, RELAIS));
    TEST_ASSERT_FALSE(peer_source_accepted(true, RELAIS, INCONNU));
}

/* --- LE SCÉNARIO RÉEL : le kit du van voisin est rejeté --- */
static void test_paired_rejects_neighbour_kit(void)
{
    TEST_ASSERT_FALSE(peer_source_accepted(true, RELAIS, VOISIN));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_mac_equal);
    RUN_TEST(test_unpaired_accepts_anyone);
    RUN_TEST(test_paired_accepts_only_known);
    RUN_TEST(test_paired_rejects_neighbour_kit);
    return UNITY_END();
}
