/**
 * @file test_cli.cpp
 * @brief Interface série du banc de test — BANC DE TEST UNIQUEMENT
 *
 * Voir test_cli.h. L'analyse des commandes est dans test_cli_parser.h
 * (pur, testé nativement) ; ce fichier ne fait que l'exécution :
 * appels aux mêmes fonctions publiques que l'UI tactile
 * (thermostat_start, setpoint_mode_start, ...), aucun contournement
 * des gardes de sécurité.
 */

#ifdef TEST_CLI

#include "test_cli.h"
#include "test_cli_parser.h"
#include "sensor.h"
#include "../core/heater_fsm.h"
#include "../core/mode_thermostat.h"
#include "../core/mode_setpoint.h"
#include "../core/mode_timer.h"
#include "../comm/relay_link.h"
#include "../ui/scr_menu.h"
#include "../ui/scr_thermostat.h"
#include "../ui/scr_timer.h"
#include "../ui/scr_setpoint.h"
#include <lvgl.h>
#include <Arduino.h>

/* Tampon d'assemblage de la ligne en cours (une commande par ligne) */
static char   line_buf[64];
static size_t line_len = 0;

/** Nom lisible d'un état de la FSM chauffage. */
static const char *heater_state_name(HeaterState s)
{
    switch (s) {
        case HEATER_IDLE:    return "IDLE";
        case HEATER_HEATING: return "HEATING";
        case HEATER_LOCKED:  return "LOCKED";
        default:             return "?";
    }
}

/** Affiche l'état complet du système (réponse à `status`). */
static void print_status()
{
    /* t=millis() : permet au banc hôte de distinguer un gel de la boucle
     * (deltas device irréguliers) d'un retard de transport USB CDC
     * (deltas device réguliers mais arrivée hôte tardive). */
    Serial.printf("[TCLI] status: t=%lu heater=%s thermo=%d consigne=%d timer=%d "
                  "relay=%d sim=%d raw=%.2f ema=%.2f\n",
                  (unsigned long)millis(),
                  heater_state_name(heater_get_state()),
                  (int)thermostat_is_active(),
                  (int)setpoint_mode_is_active(),
                  (int)timer_mode_is_running(),
                  (int)relay_is_connected(),
                  (int)sensor_sim_is_active(),
                  sensor_get_raw_temperature(),
                  sensor_get_temperature());
}

/** Arrête tous les modes (chacun coupe le chauffage si nécessaire). */
static void stop_all_modes()
{
    if (thermostat_is_active())     thermostat_stop();
    if (setpoint_mode_is_active())  setpoint_mode_stop();
    if (timer_mode_is_running())    timer_mode_stop();
    /* Filet : si le chauffage tourne hors mode (ne devrait pas), couper */
    if (heater_get_state() == HEATER_HEATING) heater_request_off();
}

/** Exécute une ligne de commande complète. */
static void execute_line(const char *line)
{
    TestCliCmd cmd = test_cli_parse(line);

    switch (cmd.type) {
        case TCLI_EMPTY:
            break;

        case TCLI_SIM_SET:
            sensor_sim_set(cmd.a);
            /* Appliquer immédiatement (sans attendre l'intervalle) :
             * le prochain sensor_update() injecte la valeur */
            sensor_force_read();
            Serial.printf("[TCLI] sim=%.2f\n", cmd.a);
            break;

        case TCLI_SIM_ERROR:
            sensor_sim_set_error();
            Serial.println("[TCLI] sim erreur capteur (lectures en echec)");
            break;

        case TCLI_SIM_OFF:
            sensor_sim_off();
            sensor_force_read();
            Serial.println("[TCLI] sim off (retour sonde reelle)");
            break;

        case TCLI_THERMO:
            if (thermostat_start(cmd.a, (int)cmd.b)) {
                Serial.printf("[TCLI] thermo demarre (sp=%.1f hyst=%d)\n",
                              cmd.a, (int)cmd.b);
            } else {
                Serial.println("[TCLI] thermo REFUSE (capteur non pret ?)");
            }
            break;

        case TCLI_CONSIGNE:
            if (setpoint_mode_start(cmd.a)) {
                Serial.printf("[TCLI] consigne demarree (cible=%.1f)\n", cmd.a);
            } else {
                Serial.println("[TCLI] consigne REFUSEE (capteur non pret ?)");
            }
            break;

        case TCLI_TIMER:
            timer_mode_set_duration_min((int)cmd.a);
            timer_mode_start((int)cmd.a);
            Serial.printf("[TCLI] timer demarre (%d min)\n", (int)cmd.a);
            break;

        case TCLI_STOP:
            stop_all_modes();
            Serial.println("[TCLI] stop: tous les modes arretes");
            break;

        case TCLI_STATUS:
            print_status();
            break;

        case TCLI_MEM: {
            /* NOMBRE DE TIMERS LVGL VIVANTS : c'est la métrique qui
             * détecte les fuites de cycle de vie des écrans. Le tas
             * seul ne convient pas — sa variation normale (objets
             * d'écran, fragmentation) dépasse largement la taille d'un
             * timer orphelin, et masquerait donc la fuite.
             * Un timer orphelin ne se contente pas d'occuper la
             * mémoire : son callback continue de s'exécuter à vie. */
            int timers = 0;
            for (lv_timer_t *t = lv_timer_get_next(NULL); t != NULL;
                 t = lv_timer_get_next(t)) {
                timers++;
            }

            lv_mem_monitor_t mon;
            lv_mem_monitor(&mon);
            Serial.printf("[TCLI] mem: timers=%d lv_libre=%u lv_frag=%u%% heap=%u\n",
                          timers, (unsigned)mon.free_size,
                          (unsigned)mon.frag_pct, (unsigned)ESP.getFreeHeap());
            break;
        }

        case TCLI_OWDIAG:
            /* Diagnostic du câblage de la sonde : sert notamment à
             * vérifier la présence d'une vraie résistance de rappel
             * externe, et à comparer le taux d'échec avant/après. */
            sensor_diag_onewire((int)cmd.a);
            break;

        case TCLI_SCREEN:
        case TCLI_SCREENDUP: {
            /* screendup : DEUX créations dos à dos, sans repasser par
             * lv_timer_handler entre les deux. C'est la reproduction
             * fidèle du double appui pendant le fondu de chargement —
             * espacer les deux appels laisserait LVGL détruire la
             * première instance, et il n'y aurait plus rien à tester. */
            int repeats = (cmd.type == TCLI_SCREENDUP) ? 2 : 1;
            for (int i = 0; i < repeats; i++) {
                switch ((int)cmd.a) {
                    case 0: scr_menu_create();       break;
                    case 1: scr_thermostat_create(); break;
                    case 2: scr_timer_create();      break;
                    case 3: scr_setpoint_create();   break;
                    default:
                        Serial.println("[TCLI] ERREUR ecran inconnu (0-3)");
                        return;
                }
            }
            Serial.printf("[TCLI] ecran %d cree x%d\n", (int)cmd.a, repeats);
            break;
        }

        case TCLI_ERROR:
        default:
            Serial.printf("[TCLI] ERREUR commande invalide: '%s'\n", line);
            break;
    }
}

void test_cli_update()
{
    while (Serial.available() > 0) {
        char c = (char)Serial.read();

        if (c == '\n' || c == '\r') {
            if (line_len > 0) {
                line_buf[line_len] = '\0';
                execute_line(line_buf);
                line_len = 0;
            }
        } else if (line_len < sizeof(line_buf) - 1) {
            line_buf[line_len++] = c;
        } else {
            /* Ligne trop longue : purger jusqu'au prochain retour */
            line_len = 0;
            Serial.println("[TCLI] ERREUR ligne trop longue, ignoree");
        }
    }
}

#endif /* TEST_CLI */
