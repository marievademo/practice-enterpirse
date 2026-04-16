/**
 * buttons.c ? Knop driver met 8-staps shift-register debounce
 * Bare-metal, ATmega32U4
 *
 * Knoppen schakelen naar GND.
 * Externe 10k? pull-ups naar +3V3 aanwezig (GEEN interne pull-ups).
 *
 * Pinmapping (conform schema):
 *   BTN_UP   ? PE6  (SW1 "PE6_Button_Meten")  ? navigeren
 *   BTN_DOWN ? PC7                             ? bevestigen
 *
 * buttons_get_event() moet periodiek aangeroepen worden (~10 ms interval).
 */

#include "buttons.h"
#include <avr/io.h>

/* ?? Lokale pin-macro's ?? */
#define BTN_UP_DDR    DDRE
#define BTN_UP_PIN    PINE
#define BTN_UP_BIT    6

#define BTN_DOWN_DDR  DDRC
#define BTN_DOWN_PIN  PINC
#define BTN_DOWN_BIT  7

/* Debounce drempel: 8 consistente samples = ingedrukt */
#define DEBOUNCE_MASK  0xFF

/* ?? Lokale status ?? */
static uint8_t up_history   = 0xFF;  /* 0xFF = altijd hoog = losgelaten */
static uint8_t down_history = 0xFF;
static uint8_t up_prev_state   = 1;
static uint8_t down_prev_state = 1;

/* ?? Publieke functies ?? */

void buttons_init(void)
{
    /* Zet pinnen als ingang, GEEN pull-up (PORTx-bit blijft 0) */
    BTN_UP_DDR   &= ~(1 << BTN_UP_BIT);
    BTN_DOWN_DDR &= ~(1 << BTN_DOWN_BIT);

    /* Zorg dat pull-ups echt uit zijn (externe 10k? pull-ups aanwezig op schema) */
    PORTE &= ~(1 << BTN_UP_BIT);    /* PE6 */
    PORTC &= ~(1 << BTN_DOWN_BIT);  /* PC7 */
}

/**
 * Niet-blokkerend. Roep aan met ~10 ms interval (via timer of polling).
 * Geeft BTN_UP of BTN_DOWN terug bij een volledige debounce-event (stijgende flank:
 * knop was ingedrukt, nu losgelaten). Geeft BTN_NONE als er niets is.
 *
 * Één event per aanroep (prioriteit: UP boven DOWN).
 */
btn_event_t buttons_get_event(void)
{
    uint8_t raw_up   = (BTN_UP_PIN   >> BTN_UP_BIT)   & 1u;
    uint8_t raw_down = (BTN_DOWN_PIN >> BTN_DOWN_BIT)  & 1u;

    /* Schuif nieuwe sample in shift-register */
    up_history   = (uint8_t)((up_history   << 1) | raw_up);
    down_history = (uint8_t)((down_history << 1) | raw_down);

    btn_event_t event = BTN_NONE;

    /* Debounced status: stabiel als alle bits hetzelfde zijn */
    uint8_t up_stable   = (up_history   == DEBOUNCE_MASK) ? 1u : 0u;  /* 1 = losgelaten */
    uint8_t down_stable = (down_history == DEBOUNCE_MASK) ? 1u : 0u;

    /* Detecteer stijgende flank (ingedrukt ? losgelaten) */
    if (up_stable && (up_prev_state == 0)) {
        event = BTN_UP;
    } else if (down_stable && (down_prev_state == 0)) {
        event = BTN_DOWN;
    }

    up_prev_state   = up_stable;
    down_prev_state = down_stable;

    return event;
}
