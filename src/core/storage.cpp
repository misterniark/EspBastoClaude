/**
 * @file storage.cpp
 * @brief Implémentation du stockage NVS
 *
 * Utilise la bibliothèque Preferences d'Arduino-ESP32
 * pour stocker les paramètres dans la partition NVS.
 * Les valeurs par défaut sont utilisées si la clé n'existe pas.
 */

#include "storage.h"
#include "../config.h"
#include <Preferences.h>

static Preferences prefs;

void storage_init()
{
    prefs.begin(NVS_NAMESPACE, false); /* false = lecture/écriture */
    Serial.println("[STORAGE] NVS initialisé (namespace: " NVS_NAMESPACE ")");
}

/* --- Setpoint --- */

float storage_load_setpoint()
{
    return prefs.getFloat(NVS_KEY_SETPOINT, DEFAULT_SETPOINT);
}

void storage_save_setpoint(float value)
{
    prefs.putFloat(NVS_KEY_SETPOINT, value);
}

/* --- Hystérésis --- */

int storage_load_hysteresis()
{
    return prefs.getInt(NVS_KEY_HYST, DEFAULT_HYSTERESIS);
}

void storage_save_hysteresis(int value)
{
    prefs.putInt(NVS_KEY_HYST, value);
}

/* --- Durée minuteur --- */

int storage_load_timer_min()
{
    return prefs.getInt(NVS_KEY_TIMER, DEFAULT_TIMER_MIN);
}

void storage_save_timer_min(int value)
{
    prefs.putInt(NVS_KEY_TIMER, value);
}

/* --- Dernier mode --- */

int storage_load_last_mode()
{
    return prefs.getInt(NVS_KEY_LAST_MODE, 0);
}

void storage_save_last_mode(int mode)
{
    prefs.putInt(NVS_KEY_LAST_MODE, mode);
}

/* --- MAC relais --- */

bool storage_load_relay_mac(uint8_t *mac_out)
{
    size_t len = prefs.getBytes(NVS_KEY_RELAY_MAC, mac_out, 6);
    if (len != 6) return false;

    /* Vérifier que la MAC n'est pas toute à zéro (non initialisée) */
    bool all_zero = true;
    for (int i = 0; i < 6; i++) {
        if (mac_out[i] != 0) {
            all_zero = false;
            break;
        }
    }
    return !all_zero;
}

void storage_save_relay_mac(const uint8_t *mac)
{
    prefs.putBytes(NVS_KEY_RELAY_MAC, mac, 6);
    Serial.printf("[STORAGE] MAC relais sauvegardée : %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
