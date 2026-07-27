/**
 * @file power_manager.cpp
 * @brief Implémentation du gestionnaire d'économie d'énergie
 *
 * Logique de veille écran :
 *   - Timer d'inactivité de 60 secondes
 *   - Si aucune action tactile pendant 60s → backlight OFF + TFT SLPIN
 *   - Au toucher suivant → SLPOUT + 120ms + backlight ON
 *   - Le toucher de réveil est consommé via hal_touchpad_ignore_until_release() :
 *     le driver tactile masque tous les contacts pour LVGL jusqu'au
 *     relâchement complet du doigt (sans cela, LVGL verrait le doigt
 *     encore posé et émettrait un clic sur le widget sous le doigt)
 *
 * Optimisations :
 *   - CPU réduit à 80 MHz (suffisant pour LVGL + ESP-NOW)
 *   - CYD : LEDs RGB éteintes (actives à l'état bas, donc HIGH = OFF)
 *     et haut-parleur forcé LOW
 *   - CrowPanel : buzzer forcé LOW. Les GPIO des LEDs du CYD (4/16/17)
 *     ne doivent PAS être touchés ici : sur le CrowPanel, 16 = SCL du
 *     tactile et 17 = TX du port UART1-OUT (sonde DS18B20).
 */

#include "power_manager.h"
#include "../config.h"
#include "../hal/backlight.h"
#include "../hal/touchpad.h" /* hal_touchpad_ignore_until_release() */
#include "../hal/sensor.h"   /* sensor_force_read() au réveil */
#include <Arduino.h>
#include <lvgl.h>

/* Timer d'inactivité */
static unsigned long last_activity_ms = 0;
static bool screen_off = false;

void power_init()
{
    /* Réduire la fréquence CPU pour économiser l'énergie */
    setCpuFrequencyMhz(CPU_FREQ_MHZ);
    Serial.printf("[POWER] CPU à %d MHz\n", CPU_FREQ_MHZ);

#ifdef HW_CROWPANEL
    /* Forcer le buzzer passif en LOW pour éviter tout sifflement */
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
#else
    /* Éteindre les LEDs RGB (actives à l'état bas → HIGH = éteint) */
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_BLUE, HIGH);

    /* Forcer la pin speaker en LOW pour éviter les oscillations */
    pinMode(PIN_SPEAKER, OUTPUT);
    digitalWrite(PIN_SPEAKER, LOW);
#endif

    /* Initialiser le timer d'inactivité */
    last_activity_ms = millis();
    screen_off = false;

    Serial.println("[POWER] Gestionnaire d'énergie initialisé");
}

bool power_update()
{
    unsigned long now = millis();

    if (screen_off) {
        /*
         * Écran en veille : surveiller le toucher pour le réveil.
         * Si un toucher est détecté, réveiller l'écran et
         * consommer ce toucher (retourner true).
         */
        if (hal_touchpad_is_touched()) {
            display_wake();
            screen_off = false;
            last_activity_ms = now;

            /* Consommer réellement le toucher de réveil : masquer les
             * contacts pour LVGL jusqu'au relâchement du doigt. Lire
             * l'état du contrôleur ne suffit pas (il rapporte le contact
             * tant que le doigt est posé) et lv_timer_handler() tourne
             * dès cette itération, l'écran étant rallumé. */
            hal_touchpad_ignore_until_release();

            /* Forcer LVGL à redessiner tout l'écran au réveil
             * (le rendu était stoppé pendant la veille) */
            lv_obj_invalidate(lv_scr_act());

            /* Forcer une lecture capteur immédiate : en veille sans mode
             * actif, le capteur n'était plus lu du tout et la température
             * affichée/décidée serait gelée depuis la mise en veille.
             * Le sensor_update() de cette même itération de loop()
             * (l'écran étant rallumé) lira l'AHT21 tout de suite, ou
             * lancera une conversion DS18B20 neuve (~800 ms). Tant que
             * la valeur fraîche n'est pas arrivée, les gardes I4 des
             * modes refusent le démarrage (SENSOR_MAX_AGE_BEFORE_START_MS). */
            sensor_force_read();

            Serial.println("[POWER] Écran réveillé (toucher consommé)");
            return true; /* Toucher consommé */
        }
    } else {
        /*
         * Écran allumé : vérifier le timeout d'inactivité.
         * Mettre en veille après SCREEN_TIMEOUT_MS sans toucher.
         */
        if (now - last_activity_ms >= SCREEN_TIMEOUT_MS) {
            display_sleep();
            screen_off = true;
            Serial.println("[POWER] Écran mis en veille (inactivité 60s)");
        }
    }

    return false; /* Pas de toucher consommé */
}

void power_reset_inactivity()
{
    last_activity_ms = millis();
}

void power_force_wake()
{
    /* Dans tous les cas, repartir pour un délai d'inactivité complet :
     * l'appelant affiche une alerte que l'utilisateur doit avoir le
     * temps de lire avant la prochaine mise en veille. */
    last_activity_ms = millis();

    if (!screen_off) {
        return; /* Écran déjà allumé : rien d'autre à faire */
    }

    /* Même séquence de réveil que pour un toucher, SANS consommation
     * de toucher (le réveil vient du code, pas du doigt) : rallumer
     * la dalle puis forcer un redessin complet, le rendu LVGL ayant
     * été stoppé pendant la veille. */
    display_wake();
    screen_off = false;
    lv_obj_invalidate(lv_scr_act());

    /* Comme pour le réveil tactile (garde I4) : forcer une lecture
     * capteur immédiate pour que la température affichée après
     * l'acquittement de l'alerte ne soit pas gelée depuis la mise en
     * veille. Sans effet utile si le capteur est mort (cas C1), mais
     * inoffensif. */
    sensor_force_read();

    Serial.println("[POWER] Réveil forcé de l'écran (alerte)");
}

bool power_is_screen_off()
{
    return screen_off;
}
