#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

/**
 * buttons.h ? Knop-driver met software debounce
 *
 * Knoppen schakelen naar GND. Externe pull-ups vereist (interne NIET gebruikt).
 *
 * Pinmapping (conform schema):
 *   BTN_UP   ? PE6  (SW1 "PE6_Button_Meten")  ? navigeren
 *   BTN_DOWN ? PC7                             ? bevestigen
 */

typedef enum {
    BTN_NONE  = 0,
    BTN_UP    = 1,
    BTN_DOWN  = 2
} btn_event_t;

void        buttons_init(void);
btn_event_t buttons_get_event(void);   /* niet-blokkerend, geeft event bij loslaten */

#endif /* BUTTONS_H */


