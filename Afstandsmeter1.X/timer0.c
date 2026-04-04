#include "timer0.h"

// Static zorgt ervoor dat deze variabele alleen in dit bestand zichtbaar is
static volatile uint32_t system_ticks = 0;

void timer0_init(void) {
    // Zet Timer0 in CTC mode (Clear Timer on Compare Match)
    TCCR0A = (1 << WGM01);
    
    // Vergelijkingswaarde voor 1ms bij 8MHz met prescaler 64
    // (8.000.000 / 64) / 1000 = 125. Aangezien we vanaf 0 tellen: 124.
    OCR0A = 124;
    
    // Zet de interrupt aan voor compare match A
    TIMSK0 = (1 << OCIE0A);
    
    // Start de timer met prescaler 64
    TCCR0B = (1 << CS01) | (1 << CS00);
    
    // Let op: sei() kun je hier doen, maar vaak is het beter om dit 
    // éénmalig in je main.c te doen nadat ALLE inits klaar zijn.
}

// ISR voor Timer0 Compare Match A
ISR(TIMER0_COMPA_vect) {
    system_ticks++;
}

uint32_t get_ticks(void) {
    uint32_t ticks;
    
    // Sla de huidige status van het interrupt register op
    uint8_t sreg = SREG;
    
    // Zet interrupts tijdelijk uit (atomair uitlezen)
    cli(); 
    ticks = system_ticks;
    
    // Herstel de oude status van interrupts (zodat ze niet per ongeluk uit blijven)
    SREG = sreg;
    
    return ticks;
}

// Handige extra functie om te checken of er al X tijd voorbij is
uint8_t has_elapsed(uint32_t start_time, uint32_t duration) {
    if ((get_ticks() - start_time) >= duration) {
        return 1;
    }
    return 0;
}
