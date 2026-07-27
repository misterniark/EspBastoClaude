/**
 * @file test_main.cpp
 * @brief Tests unitaires de la politique d'alertes (ui/alert_policy.h)
 *
 * Vérifie la logique de dispatch des alertes plein écran :
 *   - détection de front (une alerte affichée une seule fois par épisode)
 *   - priorités (sécurité > connexion > état capteur)
 *   - réarmement après acquittement (drapeau retombé)
 *   - file d'attente implicite quand une alerte en remplace une autre
 *   - suppression du doublon capteur (état déjà couvert par C1)
 *
 * Exécution : pio test -e native
 */

#include <unity.h>
#include "../../src/ui/alert_policy.h"

void setUp(void) {}
void tearDown(void) {}

/* --- Aucune alerte : rien à afficher, quel que soit le nombre de cycles --- */
static void test_no_alert(void)
{
    AlertPolicyState st;
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, false, false));
    }
}

/* --- C1 : l'alerte sécurité capteur s'affiche une seule fois tant qu'elle est levée --- */
static void test_safety_sensor_shown_once(void)
{
    AlertPolicyState st;
    /* Le drapeau C1 s'accompagne toujours de l'état capteur critique */
    TEST_ASSERT_EQUAL(ALERT_SAFETY_SENSOR, alert_policy_step(st, true, false, false, true));
    /* Cycles suivants : l'écran est déjà affiché, ne pas le recréer */
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, true, false, false, true));
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, true, false, false, true));
}

/* --- C1 acquittée : pas de doublon « câblage » pour le même épisode d'erreur --- */
static void test_safety_sensor_ack_suppresses_state_alert(void)
{
    AlertPolicyState st;
    TEST_ASSERT_EQUAL(ALERT_SAFETY_SENSOR, alert_policy_step(st, true, false, false, true));
    /* L'utilisateur appuie OK : le drapeau C1 retombe, mais le capteur
     * est toujours en erreur critique. L'info a déjà été montrée :
     * ne pas enchaîner avec l'écran câblage. */
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, false, true));
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, false, true));
}

/* --- Capteur réparé puis nouvelle panne : l'alerte d'état se réarme --- */
static void test_sensor_state_rearms_after_recovery(void)
{
    AlertPolicyState st;
    /* Première panne (sans chauffe : pas de C1, juste l'état) */
    TEST_ASSERT_EQUAL(ALERT_SENSOR_STATE, alert_policy_step(st, false, false, false, true));
    /* Épisode vu : plus de re-affichage (pas de boucle « OK → re-alerte ») */
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, false, true));
    /* Guérison */
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, false, false));
    /* Rechute : nouvel épisode, nouvelle alerte */
    TEST_ASSERT_EQUAL(ALERT_SENSOR_STATE, alert_policy_step(st, false, false, false, true));
}

/* --- C4 : alerte température max, une fois, réarmée après acquittement --- */
static void test_safety_overtemp(void)
{
    AlertPolicyState st;
    TEST_ASSERT_EQUAL(ALERT_SAFETY_OVERTEMP, alert_policy_step(st, false, true, false, false));
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, true, false, false));
    /* Acquittement (OK) : le drapeau retombe */
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, false, false));
    /* Nouvelle surchauffe : nouvelle alerte */
    TEST_ASSERT_EQUAL(ALERT_SAFETY_OVERTEMP, alert_policy_step(st, false, true, false, false));
}

/* --- Priorité : la sécurité passe avant la connexion --- */
static void test_priority_safety_over_connection(void)
{
    AlertPolicyState st;
    /* Les deux drapeaux levés en même temps : sécurité d'abord */
    TEST_ASSERT_EQUAL(ALERT_SAFETY_OVERTEMP, alert_policy_step(st, false, true, true, false));
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, true, true, false));
    /* OK sur l'alerte sécurité : la connexion, toujours pendante, prend le relais */
    TEST_ASSERT_EQUAL(ALERT_CONNECTION, alert_policy_step(st, false, false, true, false));
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, true, false));
    /* OK sur l'alerte connexion : plus rien */
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, false, false));
}

/* --- Remplacement : une alerte sécurité écrase l'écran connexion affiché,
 *     qui revient après l'acquittement de la sécurité --- */
static void test_higher_priority_replaces_then_requeues(void)
{
    AlertPolicyState st;
    /* Connexion perdue : écran connexion affiché */
    TEST_ASSERT_EQUAL(ALERT_CONNECTION, alert_policy_step(st, false, false, true, false));
    /* C1 survient : l'écran sécurité remplace l'écran connexion */
    TEST_ASSERT_EQUAL(ALERT_SAFETY_SENSOR, alert_policy_step(st, true, false, true, true));
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, true, false, true, true));
    /* OK sécurité : l'écran connexion, jamais acquitté, revient */
    TEST_ASSERT_EQUAL(ALERT_CONNECTION, alert_policy_step(st, false, false, true, true));
    /* OK connexion : plus rien (capteur toujours critique mais déjà vu via C1) */
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, false, true));
}

/* --- Alerte connexion seule : une fois, réarmée après acquittement --- */
static void test_connection_alert(void)
{
    AlertPolicyState st;
    TEST_ASSERT_EQUAL(ALERT_CONNECTION, alert_policy_step(st, false, false, true, false));
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, true, false));
    /* Acquittement puis nouvelle perte : nouvelle alerte */
    TEST_ASSERT_EQUAL(ALERT_NONE, alert_policy_step(st, false, false, false, false));
    TEST_ASSERT_EQUAL(ALERT_CONNECTION, alert_policy_step(st, false, false, true, false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_no_alert);
    RUN_TEST(test_safety_sensor_shown_once);
    RUN_TEST(test_safety_sensor_ack_suppresses_state_alert);
    RUN_TEST(test_sensor_state_rearms_after_recovery);
    RUN_TEST(test_safety_overtemp);
    RUN_TEST(test_priority_safety_over_connection);
    RUN_TEST(test_higher_priority_replaces_then_requeues);
    RUN_TEST(test_connection_alert);
    return UNITY_END();
}
