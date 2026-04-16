/**
 * main_sim_test.c ? SIMULATOR TESTVERSIE
 * ============================================================
 * Gebruik dit bestand IN PLAATS VAN main.c tijdens simulator-tests.
 * De VL53L1X sensor is gemockt: geeft vaste testwaarden terug.
 *
 * ALLE FSM-variabelen zijn GLOBAAL gedeclareerd zodat MPLAB X
 * Simulator ze toont in het Watch/Variables venster via
 * "Global Symbols".
 *
 * HOE TOEVOEGEN AAN WATCH VENSTER:
 *   1. Variables tab ? klik "<Enter new watch>"
 *   2. Kies "Global Symbols" (staat al aan)
 *   3. Scroll naar: g_state, g_menu_sel, g_meas_0 ... g_vol_result
 *   4. Dubbelklik of klik OK
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#include "i2c.h"
#include "lcd.h"
#include "buttons.h"
#include "./API/core/VL53L1X_api.h"

/* ??????????????????????????????????????????????????????
 * MOCK CONFIGURATIE
 * ?????????????????????????????????????????????????????? */
#define MOCK_DIST_1   500u
#define MOCK_DIST_2   300u
#define MOCK_DIST_3   200u
#define MOCK_FAIL       0    /* 1 = simuleer sensor fout */

static uint8_t  mock_meas_count = 0u;
static uint16_t mock_distances[3] = {MOCK_DIST_1, MOCK_DIST_2, MOCK_DIST_3};

/* ??????????????????????????????????????????????????????
 * FSM TYPES ? moeten voor globale declaraties staan
 * ?????????????????????????????????????????????????????? */
typedef enum {
    STATE_MENU            = 0,
    STATE_MEASURE_LENGTH  = 1,
    STATE_MEASURE_AREA    = 2,
    STATE_MEASURE_VOLUME  = 3,
    STATE_RESULT          = 4
} app_state_t;

typedef enum {
    MENU_LENGTH = 0,
    MENU_AREA   = 1,
    MENU_VOLUME = 2,
    MENU_COUNT  = 3
} menu_item_t;

/* ??????????????????????????????????????????????????????
 * GLOBALE DEBUG-VARIABELEN
 * Zichtbaar in MPLAB X Watch venster via "Global Symbols"
 * Voeg toe: g_state, g_menu_sel, g_meas_0, g_meas_1,
 *           g_meas_2, g_meas_step, g_total_steps,
 *           g_area_result, g_vol_result,
 *           g_tick_counter, g_last_event
 * ?????????????????????????????????????????????????????? */
volatile uint8_t  g_tick_10ms     = 0u;
volatile uint32_t g_tick_counter  = 0u;  /* telt elke 10 ms tick */
volatile uint8_t  g_last_event    = 0u;  /* 0=NONE, 1=UP, 2=DOWN */

app_state_t  g_state       = STATE_MENU;
menu_item_t  g_menu_sel    = MENU_LENGTH;
uint16_t     g_meas_0      = 0u;
uint16_t     g_meas_1      = 0u;
uint16_t     g_meas_2      = 0u;
uint8_t      g_meas_step   = 0u;
uint8_t      g_total_steps = 1u;
uint32_t     g_area_result = 0u;
uint32_t     g_vol_result  = 0u;

/* ??????????????????????????????????????????????????????
 * TIMER0 ? 10 ms tick @ 8 MHz, prescaler 1024
 * OCR0A = (8000000 / (1024 * 100)) - 1 = 77
 * TEST 1: zet breakpoint op g_tick_counter++
 *         Stopwatch moet ~10.24 ms tonen
 * ?????????????????????????????????????????????????????? */
#define TIMER0_OCR  77u

ISR(TIMER0_COMPA_vect)
{
    g_tick_10ms = 1u;
    g_tick_counter++;   /* <-- BREAKPOINT HIER voor TEST 1 */
}

static void timer0_init(void)
{
    TCCR0A = (1u << WGM01);
    TCCR0B = (1u << CS02) | (1u << CS00);
    OCR0A  = TIMER0_OCR;
    TIMSK0 = (1u << OCIE0A);
}

/* ??????????????????????????????????????????????????????
 * MOCK SENSOR
 * ?????????????????????????????????????????????????????? */
/*
 
static vl53l1x_status_t mock_vl53l1x_init(void)
{
#if MOCK_FAIL
    return VL53L1X_ERROR;
#else
    mock_meas_count = 0u;
    return VL53L1X_OK;
#endif
}
*/
static uint16_t do_single_measurement(void)
{
    uint16_t dist = mock_distances[mock_meas_count % 3u];
    mock_meas_count++;
    return dist;
}

/* ??????????????????????????????????????????????????????
 * LCD HULPFUNCTIES
 * ?????????????????????????????????????????????????????? */
static void lcd_print_menu(menu_item_t selected)
{
    static const char *items[MENU_COUNT] = {
        "Lengte",
        "Oppervlakte",
        "Volume"
    };

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("UP:nav DOWN:ok  ");
    lcd_set_cursor(0, 1);
    lcd_print_char('>');
    lcd_print_char(' ');
    lcd_print(items[(uint8_t)selected]);

    uint8_t len = 0u;
    const char *p = items[(uint8_t)selected];
    while (*p++) len++;
    for (uint8_t i = (uint8_t)(len + 2u); i < 16u; i++) lcd_print_char(' ');
}

static void lcd_print_measuring(uint8_t step, uint8_t total)
{
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("UP:meet DOWN:bck");
    lcd_set_cursor(0, 1);
    lcd_print("Meting ");
    lcd_print_uint(step);
    lcd_print_char('/');
    lcd_print_uint(total);
    lcd_print("         ");
}

static void lcd_print_distance(uint16_t dist_mm)
{
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("Afstand:");
    lcd_set_cursor(0, 1);
    if (dist_mm == 0xFFFFu) {
        lcd_print("Fout / bereik   ");
    } else {
        lcd_print_uint(dist_mm);
        lcd_print(" mm         ");
    }
}

/* ??????????????????????????????????????????????????????
 * MAIN
 * ?????????????????????????????????????????????????????? */
int main(void)
{
    /* TEST 2: breakpoint na i2c_init() ? SFR: TWBR=32, TWCR=0x04 */
    timer0_init();
    i2c_init();

    /* TEST 3: breakpoint na buttons_init()
     * SFR: DDRE bit6=0, DDRC bit7=0, PORTE bit6=0, PORTC bit7=0 */
    lcd_init();
    buttons_init();
    sei();

    /* TEST sensor init fout: zet MOCK_FAIL=1 bovenaan */
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("Init sensor...");
    
/*
    if (mock_vl53l1x_init() != VL53L1X_OK) {
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_print("SENSOR FOUT!");
        lcd_set_cursor(0, 1);
        lcd_print("Check I2C/VCC");
        while (1) {}
    }
*/
    /* Initialiseer globale variabelen */
    g_state       = STATE_MENU;   /* TEST 4: breakpoint hier */
    g_menu_sel    = MENU_LENGTH;
    g_meas_0      = 0u;
    g_meas_1      = 0u;
    g_meas_2      = 0u;
    g_meas_step   = 0u;
    g_total_steps = 1u;
    g_area_result = 0u;
    g_vol_result  = 0u;
    mock_meas_count = 0u;

    lcd_print_menu(g_menu_sel);

    while (1)
    {
        if (!g_tick_10ms) continue;
        g_tick_10ms = 0u;

        btn_event_t ev = buttons_get_event();
        g_last_event   = (uint8_t)ev;

        if (ev == BTN_NONE) continue;

        /* TEST 5/6: breakpoint hier ? check g_state en g_last_event */
        switch (g_state)
        {
            /* ?? MENU ?? */
            case STATE_MENU:
                if (ev == BTN_UP) {
                    /* TEST 5: g_menu_sel moet 0?1?2?0 gaan */
                    g_menu_sel = (menu_item_t)(((uint8_t)g_menu_sel + 1u) % MENU_COUNT);
                    lcd_print_menu(g_menu_sel);
                }
                else if (ev == BTN_DOWN) {
                    /* TEST 6: g_state moet veranderen naar juiste meetstate */
                    g_meas_step = 0u;
                    g_meas_0 = g_meas_1 = g_meas_2 = 0u;
                    mock_meas_count = 0u;

                    if (g_menu_sel == MENU_LENGTH) {
                        g_state       = STATE_MEASURE_LENGTH;
                        g_total_steps = 1u;
                    } else if (g_menu_sel == MENU_AREA) {
                        g_state       = STATE_MEASURE_AREA;
                        g_total_steps = 2u;
                    } else {
                        g_state       = STATE_MEASURE_VOLUME;
                        g_total_steps = 3u;
                    }
                    lcd_print_measuring(g_meas_step + 1u, g_total_steps);
                }
                break;

            /* ?? METEN ?? */
            case STATE_MEASURE_LENGTH:
            case STATE_MEASURE_AREA:
            case STATE_MEASURE_VOLUME:
                if (ev == BTN_DOWN) {
                    /* TEST 10: g_state moet STATE_MENU worden */
                    g_state = STATE_MENU;
                    lcd_print_menu(g_menu_sel);
                }
                else if (ev == BTN_UP) {
                    lcd_clear();
                    lcd_set_cursor(0, 0);
                    lcd_print("Meting loopt...");

                    uint16_t dist = do_single_measurement();

                    /* Sla op in juiste globale variabele */
                    if      (g_meas_step == 0u) g_meas_0 = dist;
                    else if (g_meas_step == 1u) g_meas_1 = dist;
                    else                         g_meas_2 = dist;
                    /* TEST 7: breakpoint hier ? g_meas_0 moet 500 zijn */

                    lcd_print_distance(dist);
                    _delay_ms(1500);

                    g_meas_step++;

                    if (g_meas_step >= g_total_steps) {
                        lcd_clear();
                        lcd_set_cursor(0, 0);

                        if (g_state == STATE_MEASURE_LENGTH) {
                            lcd_print("Lengte:");
                            lcd_set_cursor(0, 1);
                            lcd_print_uint(g_meas_0);
                            lcd_print(" mm         ");
                        }
                        else if (g_state == STATE_MEASURE_AREA) {
                            /* TEST 8: 500*300=150000 ? /100 = 1500 cm² */
                            g_area_result = ((uint32_t)g_meas_0 * (uint32_t)g_meas_1) / 100u;
                            lcd_print("Opp (cm2):");
                            lcd_set_cursor(0, 1);
                            lcd_print_uint(g_area_result);
                            lcd_print(" cm2        ");
                        }
                        else {
                            /* TEST 9: 100*200=20000 ? /10=2000 ? *300=600000 ? /100=6000 cm³ */
                            uint32_t tmp = ((uint32_t)g_meas_0 * (uint32_t)g_meas_1) / 10u;
                            g_vol_result = (tmp * (uint32_t)g_meas_2) / 100u;
                            lcd_print("Vol (cm3):");
                            lcd_set_cursor(0, 1);
                            lcd_print_uint(g_vol_result);
                            lcd_print(" cm3        ");
                        }

                        g_state = STATE_RESULT;
                    } else {
                        lcd_print_measuring(g_meas_step + 1u, g_total_steps);
                    }
                }
                break;

            /* ?? RESULTAAT ?? */
            case STATE_RESULT:
                if (ev == BTN_UP || ev == BTN_DOWN) {
                    g_state = STATE_MENU;
                    lcd_print_menu(g_menu_sel);
                }
                break;

            default:
                g_state = STATE_MENU;
                lcd_print_menu(g_menu_sel);
                break;
        }
    }

    return 0;
}
