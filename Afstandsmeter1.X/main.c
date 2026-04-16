/**
 * main.c ? Hoofd applicatie: Lengte / Oppervlakte / Volume meter
 * Hardware : ATmega32U4 @ 8 MHz, HD44780 via PCF8574T I2C, VL53L1X ToF, 2 knoppen
 * Compiler : AVR-GCC, -DF_CPU=8000000UL -mmcu=atmega32u4
 *
 * Knop-rollen (context-afhankelijk):
 *   MENU-modus  : BTN_UP   = navigeren (volgende menu-item)
 *                 BTN_DOWN = bevestigen (selecteer)
 *   MEET-modus  : BTN_UP   = meting uitvoeren
 *                 BTN_DOWN = terug naar menu
 *
 * State-machine:
 *   STATE_MENU
 *     ?? BTN_UP   ? cursor +1 (wrap)
 *     ?? BTN_DOWN ? bevestig ? STATE_READY
 *   STATE_READY
 *     ?? BTN_UP   ? meet ? sla op ? STATE_RESULT (of volgende deelmeting)
 *     ?? BTN_DOWN ? terug ? STATE_MENU
 *   STATE_RESULT
 *     ?? BTN_UP   ? nieuwe meting ? STATE_READY
 *     ?? BTN_DOWN ? terug ? STATE_MENU
 *
 * Meet-logica:
 *   LENGTE    : 1 meting  ? mm
 *   OPPERVLAK : 2 metingen ? breedte x hoogte ? mm2 / cm2
 *   VOLUME    : 3 metingen ? b x h x d        ? mm3 / cm3
 *
 * FIXES t.o.v. vorige versie:
 *   1. Dubbele definitie van do_single_measurement() verwijderd.
 *   2. Alle lcd_set_cursor()-aanroepen volledig aanwezig in elke display-functie.
 *   3. Volume-berekening gebruikt expliciete (uint32_t)-cast op alle operanden.
 *   4. Functie-prototypes bovenaan zodat volgorde in het bestand niet uitmaakt.
 *   5. Sensor-init foutpad toont foutmelding en stopt correct.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#include "i2c.h"
#include "lcd.h"
#include "buttons.h"
#include "timer0.h"
#include "VL53L1X_api.h"

/* ?? Constanten ??????????????????????????????????????????????????? */
#define MENU_ITEMS    3u
#define BTN_POLL_MS   10u

/*
 * 8-bit I2C-adres voor de VL53L1X_api functies.
 * De ST-driver verwacht het 8-bit adres (7-bit adres << 1).
 * Default 7-bit adres van de VL53L1X is 0x29, dus 8-bit = 0x52.
 */
#define SENSOR_ADDR   0x52u

/* ?? Menu-teksten ?????????????????????????????????????????????????? */
static const char * const MENU_LABELS[MENU_ITEMS] = {
    "1. Lengte",
    "2. Oppervlakte",
    "3. Volume"
};

/* ?? Applicatie-types ?????????????????????????????????????????????? */
typedef enum {
    STATE_MENU   = 0,
    STATE_READY  = 1,
    STATE_RESULT = 2
} app_state_t;

typedef enum {
    MODE_LENGTE    = 0,
    MODE_OPPERVLAK = 1,
    MODE_VOLUME    = 2
} meas_mode_t;

/* ?? Globale toestand ?????????????????????????????????????????????? */
static app_state_t  g_state       = STATE_MENU;
static meas_mode_t  g_mode        = MODE_LENGTE;
static uint8_t      g_menu_cursor = 0u;
static uint16_t     g_meas[3];
static uint8_t      g_meas_count  = 0u;

/* ?? Functie-prototypes ???????????????????????????????????????????? */
static void    display_menu(void);
static void    display_ready(void);
static void    display_result(void);
static void    display_error(const char *msg);
static uint8_t do_single_measurement(uint16_t *out_mm);
static void    handle_measure(void);

/* ???????????????????????????????????????????????????????????????????
 * DISPLAY-FUNCTIES
 * ??????????????????????????????????????????????????????????????????? */

static void display_menu(void)
{
    uint8_t next = (uint8_t)((g_menu_cursor + 1u) % MENU_ITEMS);

    lcd_clear();

    /* Regel 0: geselecteerd item met cursor-pijl */
    lcd_set_cursor(0, 0);
    lcd_print("> ");
    lcd_print(MENU_LABELS[g_menu_cursor]);

    /* Regel 1: volgend item als preview */
    lcd_set_cursor(0, 1);
    lcd_print("  ");
    lcd_print(MENU_LABELS[next]);
}

static void display_ready(void)
{
    lcd_clear();

    /* Regel 0: instructie afhankelijk van modus en deelmeting */
    lcd_set_cursor(0, 0);
    if (g_mode == MODE_LENGTE) {
        lcd_print("Lengte meten");
    } else if (g_mode == MODE_OPPERVLAK) {
        lcd_print(g_meas_count == 0u ? "Breedte meten" : "Hoogte meten");
    } else {
        /* MODE_VOLUME */
        if      (g_meas_count == 0u) lcd_print("Breedte meten");
        else if (g_meas_count == 1u) lcd_print("Hoogte meten");
        else                         lcd_print("Diepte meten");
    }

    /* Regel 1: knop-instructies ? precies 16 tekens */
    lcd_set_cursor(0, 1);
    lcd_print("UP=Meet DN=Terug");
}

static void display_result(void)
{
    lcd_clear();

    if (g_mode == MODE_LENGTE)
    {
        /* ?? Lengte: toon mm ??????????????????????????????????????? */
        lcd_set_cursor(0, 0);
        lcd_print("Lengte:");

        lcd_set_cursor(0, 1);
        lcd_print_uint(g_meas[0]);
        lcd_print(" mm");
    }
    else if (g_mode == MODE_OPPERVLAK)
    {
        /* ?? Oppervlakte: breedte x hoogte ???????????????????????? */
        uint32_t area_mm2 = (uint32_t)g_meas[0] * (uint32_t)g_meas[1];
        uint32_t area_cm2 = area_mm2 / 100u;

        lcd_set_cursor(0, 0);
        lcd_print("Opp:");
        lcd_print_uint(area_mm2);
        lcd_print("mm2");

        lcd_set_cursor(0, 1);
        lcd_print("=");
        lcd_print_uint(area_cm2);
        lcd_print(" cm2");
    }
    else
    {
        /*
         * ?? Volume: breedte x hoogte x diepte ????????????????????
         * FIX #3: alle operanden expliciet casten naar uint32_t
         * VOOR de vermenigvuldiging, anders overflow bij >1625 mm.
         */
        uint64_t vol_mm3 = (uint64_t)g_meas[0] * (uint64_t)g_meas[1] * (uint64_t)g_meas[2];

        /* Deel door 1000 om cm3 te krijgen. 
           64 miljard / 1000 = 64 miljoen. Dat past weer prima in een uint32_t! */
        uint32_t vol_cm3 = (uint32_t)(vol_mm3 / 1000u);

        lcd_set_cursor(0, 0);
        lcd_print("Vol:");
        lcd_print_uint(vol_mm3);
        lcd_print("mm3");

        lcd_set_cursor(0, 1);
        lcd_print("=");
        lcd_print_uint(vol_cm3);
        lcd_print(" cm3");
    }
}

static void display_error(const char *msg)
{
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("! Fout:");
    lcd_set_cursor(0, 1);
    lcd_print(msg);
}

/* ???????????????????????????????????????????????????????????????????
 * SENSOR-METING
 * ??????????????????????????????????????????????????????????????????? */

/**
 * do_single_measurement()
 *   Start een VL53L1X-meting, wacht op resultaat (max 300 ms),
 *   leest de afstand uit en stopt de sensor.
 *   Retourneert 0 bij succes, niet-nul bij fout.
 *
 *   FIX #1: er bestaat nu slechts EEN definitie van deze functie.
 */
static uint8_t do_single_measurement(uint16_t *out_mm)
{
    uint8_t  ready   = 0u;
    uint32_t t_start = get_ticks();

    if (VL53L1X_StartRanging(SENSOR_ADDR) != 0) {
        return 1u;
    }

    /* Polling: wacht tot data klaar is, max 300 ms */
    while (!has_elapsed(t_start, 300u)) {
        if (VL53L1X_CheckForDataReady(SENSOR_ADDR, &ready) != 0) {
            VL53L1X_StopRanging(SENSOR_ADDR);
            return 2u;
        }
        if (ready) break;
        _delay_ms(5);
    }

    if (!ready) {
        VL53L1X_StopRanging(SENSOR_ADDR);
        return 3u; /* timeout */
    }

    if (VL53L1X_GetDistance(SENSOR_ADDR, out_mm) != 0) {
        VL53L1X_StopRanging(SENSOR_ADDR);
        return 4u;
    }

    VL53L1X_ClearInterrupt(SENSOR_ADDR);
    VL53L1X_StopRanging(SENSOR_ADDR);
    return 0u;
}

/* ???????????????????????????????????????????????????????????????????
 * EVENT-HANDLERS
 * ??????????????????????????????????????????????????????????????????? */

static void handle_measure(void)
{
    uint16_t dist = 0u;

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("Meten...");

    if (do_single_measurement(&dist) != 0u) {
        display_error("Sensor fout");
        _delay_ms(1500);
        g_meas_count = 0u;
        g_state      = STATE_MENU;
        display_menu();
        return;
    }

    g_meas[g_meas_count] = dist;
    g_meas_count++;

    /* Hoeveel metingen heeft deze modus nodig? */
    uint8_t needed;
    if      (g_mode == MODE_LENGTE)    needed = 1u;
    else if (g_mode == MODE_OPPERVLAK) needed = 2u;
    else                               needed = 3u;

    if (g_meas_count >= needed) {
        g_state = STATE_RESULT;
        display_result();
    } else {
        /* Vraag om volgende deelmeting */
        display_ready();
    }
}

/* ???????????????????????????????????????????????????????????????????
 * MAIN
 * ??????????????????????????????????????????????????????????????????? */

int main(void)
{
    /* 1. Hardware-initialisatie */
    i2c_init();
    lcd_init();
    timer0_init();
    buttons_init();
    sei();  /* globale interrupts aan ? timer0 ISR heeft dit nodig */

    /* 2. VL53L1X initialiseren (3 pogingen) */
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("Sensor init...");

    uint8_t sensor_ok = 0u;
    for (uint8_t attempt = 0u; attempt < 3u; attempt++) {
        if (VL53L1X_SensorInit(SENSOR_ADDR) == 0) {
            sensor_ok = 1u;
            break;
        }
        _delay_ms(200);
    }

    if (!sensor_ok) {
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_print("Sensor FOUT!");
        lcd_set_cursor(0, 1);
        lcd_print("Controleer I2C");
        while (1) { /* halt */ }
    }

    /* 3. Opstartscherm */
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("  Meetapparaat  ");
    lcd_set_cursor(0, 1);
    lcd_print("________________");
    _delay_ms(1000);

    /* 4. Toon hoofdmenu */
    display_menu();

    /* 5. Hoofd-lus */
    uint32_t last_poll = get_ticks();

    while (1)
    {
        if (!has_elapsed(last_poll, BTN_POLL_MS)) {
            continue;
        }
        last_poll = get_ticks();

        btn_event_t ev = buttons_get_event();
        if (ev == BTN_NONE) continue;

        /* ?? State-machine ???????????????????????????????????????? */
        switch (g_state)
        {
            case STATE_MENU:
                if (ev == BTN_UP) {
                    g_menu_cursor = (uint8_t)((g_menu_cursor + 1u) % MENU_ITEMS);
                    display_menu();
                } else if (ev == BTN_DOWN) {
                    g_mode       = (meas_mode_t)g_menu_cursor;
                    g_meas_count = 0u;
                    g_state      = STATE_READY;
                    display_ready();
                }
                break;

            case STATE_READY:
                if (ev == BTN_UP) {
                    handle_measure();
                } else if (ev == BTN_DOWN) {
                    g_meas_count = 0u;
                    g_state      = STATE_MENU;
                    display_menu();
                }
                break;

            case STATE_RESULT:
                if (ev == BTN_UP) {
                    /* Nieuwe meting met dezelfde modus */
                    g_meas_count = 0u;
                    g_state      = STATE_READY;
                    display_ready();
                } else if (ev == BTN_DOWN) {
                    g_meas_count = 0u;
                    g_state      = STATE_MENU;
                    display_menu();
                }
                break;

            default:
                /* Onbekende state: herstel veilig naar menu */
                g_meas_count = 0u;
                g_state      = STATE_MENU;
                display_menu();
                break;
        }
    }

    return 0; /* nooit bereikt */
}
