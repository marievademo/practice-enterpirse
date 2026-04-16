#ifndef LCD_H
#define LCD_H

#include <stdint.h>

/**
 * lcd.h ? HD44780 16×2 LCD driver (4-bit modus)
 *
 * Pinmapping (definieer in lcd.c):
 *   RS ? PD4
 *   E  ? PD5
 *   D4 ? PD6
 *   D5 ? PD7
 *   D6 ? PB4
 *   D7 ? PB5
 *
 * Pas de macro's in lcd.c aan indien je andere pinnen gebruikt.
 */

void lcd_init(void);
void lcd_clear(void);
void lcd_home(void);
void lcd_set_cursor(uint8_t col, uint8_t row);   /* col 0-15, row 0-1 */
void lcd_print(const char *str);
void lcd_print_char(char c);
void lcd_print_uint(uint32_t val);
void lcd_print_fixed(uint32_t val_mm, uint8_t decimals); /* bijv. 1234 mm ? "1.234 m" */

#endif /* LCD_H */
