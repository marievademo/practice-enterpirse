#ifndef TIMER_H_
#define TIMER_H_

#include <avr/io.h>
#include <avr/interrupt.h>

/**
 * @brief Initialiseert Timer0 voor een 1ms systeem-tick op 8MHz.
 */
void timer0_init(void);

/**
 * @brief Geeft het aantal verstreken milliseconden terug sinds de start.
 * @return uint32_t milliseconden.
 */
uint32_t get_ticks(void);

/**
 * @brief Een niet-blokkerende delay check functie (handig voor later).
 */
uint8_t has_elapsed(uint32_t start_time, uint32_t duration);

#endif /* TIMER_H_ */