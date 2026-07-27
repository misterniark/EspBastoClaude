/**
 * @file test_cli.h
 * @brief Interface série du banc de test — BANC DE TEST UNIQUEMENT
 *
 * Compilé exclusivement avec -DTEST_CLI (environnement PlatformIO
 * crowpanel_test). JAMAIS dans les firmwares normaux : cette
 * interface permet de piloter le chauffage par USB, elle n'a rien à
 * faire en production.
 *
 * Grammaire des commandes : voir test_cli_parser.h.
 * Toutes les réponses sont préfixées « [TCLI] » pour que
 * l'orchestrateur de test puisse les distinguer des logs normaux.
 */

#ifndef HAL_TEST_CLI_H
#define HAL_TEST_CLI_H

#ifdef TEST_CLI

/** Lit et exécute les commandes série en attente.
 *  À appeler à chaque tour de loop(). */
void test_cli_update();

#endif /* TEST_CLI */

#endif /* HAL_TEST_CLI_H */
