/**
 * @file backlight.cpp
 * @brief Implémentation du contrôle rétroéclairage et veille TFT
 *
 * Utilise les commandes SPI directes vers le contrôleur TFT
 * (identiques sur ILI9341/CYD et ST7789/CrowPanel) :
 * - 0x10 (SLPIN) : mise en veille du contrôleur TFT
 * - 0x11 (SLPOUT) : sortie de veille, nécessite un délai de 120ms
 */

#include "backlight.h"
#include "display.h"
#include "../config.h"
#include <Arduino.h>

/* Commandes ILI9341 pour la gestion du sommeil.
 * On utilise des noms différents pour éviter le conflit
 * avec les macros définies dans TFT_eSPI/TFT_Drivers/ILI9341_Defines.h */
static constexpr uint8_t CMD_SLPIN  = 0x10;
static constexpr uint8_t CMD_SLPOUT = 0x11;

/* État interne de l'écran */
static bool sleeping = false;

/* Horodatage du dernier SLPIN : les datasheets ILI9341 et ST7789V
 * exigent un délai minimal de 120 ms entre SLPIN et SLPOUT, sinon
 * le contrôleur peut ignorer le SLPOUT (écran noir, backlight allumé). */
static unsigned long slpin_time_ms = 0;

void backlight_init()
{
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH); /* Allumé au démarrage */
    sleeping = false;
}

void backlight_on()
{
    digitalWrite(PIN_TFT_BL, HIGH);
}

void backlight_off()
{
    digitalWrite(PIN_TFT_BL, LOW);
}

void display_sleep()
{
    if (sleeping) return;

    /* Éteindre le rétroéclairage d'abord */
    backlight_off();

    /* Envoyer la commande SLPIN au contrôleur TFT */
    TFT_eSPI& tft = hal_display_get_tft();
    tft.writecommand(CMD_SLPIN);
    slpin_time_ms = millis(); /* Point de départ du délai minimal avant SLPOUT */

    sleeping = true;
}

void display_wake()
{
    if (!sleeping) return;

    /* Respecter les 120 ms minimum depuis le SLPIN (datasheet) : si le
     * réveil survient juste après la mise en veille (toucher au moment
     * exact du timeout d'inactivité), attendre le reliquat avant
     * d'envoyer SLPOUT, sinon la commande peut être ignorée. */
    unsigned long since_slpin = millis() - slpin_time_ms;
    if (since_slpin < DISPLAY_WAKE_DELAY_MS) {
        delay(DISPLAY_WAKE_DELAY_MS - since_slpin);
    }

    /* Envoyer la commande SLPOUT au contrôleur TFT */
    TFT_eSPI& tft = hal_display_get_tft();
    tft.writecommand(CMD_SLPOUT);

    /* Délai obligatoire après SLPOUT (datasheet ILI9341/ST7789) */
    delay(DISPLAY_WAKE_DELAY_MS);

    /* Rallumer le rétroéclairage */
    backlight_on();

    sleeping = false;
}

bool is_display_sleeping()
{
    return sleeping;
}
