/**
 * @file power_manager.cpp
 * @brief Implémentation du gestionnaire d'économie d'énergie
 *
 * Logique de veille écran :
 *   - Timer d'inactivité de 60 secondes
 *   - Si aucune action tactile pendant 60s → backlight OFF + ILI9341 SLPIN
 *   - Au toucher suivant → SLPOUT + 120ms + backlight ON
 *   - Ce premier toucher de réveil est CONSOMMÉ (ne déclenche pas d'action UI)
 *
 * Optimisations :
 *   - CPU réduit à 80 MHz (suffisant pour LVGL + ESP-NOW)
 *   - LEDs RGB éteintes (actives à l'état bas, donc HIGH = OFF)
 */

#include "power_manager.h"
#include "../config.h"
#include "../hal/backlight.h"
#include "../hal/touchpad.h"
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

            /* Forcer LVGL à redessiner tout l'écran au réveil
             * (le rendu était stoppé pendant la veille) */
            lv_obj_invalidate(lv_scr_act());

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

bool power_is_screen_off()
{
    return screen_off;
}
