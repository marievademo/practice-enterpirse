/**
 * lcd.c ? HD44780 16×2 LCD driver via PCF8574T I2C expander
 * Bare-metal, ATmega32U4 @ 8 MHz
 *
 * De PCF8574T stuurt de HD44780 aan in 4-bit modus.
 * I2C adres PCF8574T: 0x27 (alle adresjumpers open, A0=A1=A2=0)
 *
 * PCF8574T pinmapping naar HD44780 (standaard I2C LCD backpack layout):
 *   P0 ? RS
 *   P1 ? RW  (altijd 0, write-only)
 *   P2 ? E
 *   P3 ? Backlight (1 = aan)
 *   P4 ? D4
 *   P5 ? D5
 *   P6 ? D6
 *   P7 ? D7
 *
 * Bus: hardware I2C (SDA=PD1, SCL=PD0) gedeeld met VL53L1X
 */

#include "lcd.h"
#include "i2c.h"
#include <util/delay.h>

/* ?? Lokale constanten ?? */
#define LCD_I2C_ADDR     0x27u   /* PCF8574T, A0=A1=A2=0 */

/* PCF8574T bit-posities */
#define LCD_BIT_RS       (1u << 0)
#define LCD_BIT_RW       (1u << 1)
#define LCD_BIT_E        (1u << 2)
#define LCD_BIT_BL       (1u << 3)   /* backlight */
#define LCD_BIT_D4       (1u << 4)
#define LCD_BIT_D5       (1u << 5)
#define LCD_BIT_D6       (1u << 6)
#define LCD_BIT_D7       (1u << 7)

/* Backlight altijd aan */
#define LCD_BACKLIGHT    LCD_BIT_BL

/* ?? Stuur 1 byte naar PCF8574T ?? */
static void pcf8574_write(uint8_t data)
{
    i2c_start((uint8_t)(LCD_I2C_ADDR << 1)); /* write */
    i2c_write(data);
    i2c_stop();
}

/* ?? Enable pulse ?? */
static void lcd_pulse_enable(uint8_t data)
{
    pcf8574_write(data | LCD_BIT_E);   /* E hoog */
    _delay_us(1);
    pcf8574_write(data & ~LCD_BIT_E);  /* E laag */
    _delay_us(50);
}

/* ?? Schrijf nibble: bits 7..4 van 'byte' naar D7..D4 ?? */
static void lcd_write_nibble(uint8_t byte, uint8_t rs)
{
    uint8_t pcf = LCD_BACKLIGHT;

    if (rs)           pcf |= LCD_BIT_RS;
    if (byte & 0x80)  pcf |= LCD_BIT_D7;
    if (byte & 0x40)  pcf |= LCD_BIT_D6;
    if (byte & 0x20)  pcf |= LCD_BIT_D5;
    if (byte & 0x10)  pcf |= LCD_BIT_D4;

    lcd_pulse_enable(pcf);
}

static void lcd_send(uint8_t data, uint8_t rs)
{
    lcd_write_nibble(data,                rs);  /* hoog nibble eerst */
    lcd_write_nibble((uint8_t)(data << 4), rs); /* laag nibble */
}

#define lcd_cmd(c)   lcd_send((c), 0)
#define lcd_data(c)  lcd_send((c), 1)

/* ?? Publieke functies ?? */

void lcd_init(void)
{
    /* i2c_init() moet al aangeroepen zijn in main() */
    _delay_ms(50);

    /* HD44780 reset sequence in 4-bit modus via PCF8574 */
    uint8_t init_nibble = LCD_BACKLIGHT | LCD_BIT_D5 | LCD_BIT_D4; /* nibble 0x3 */

    pcf8574_write(init_nibble);
    lcd_pulse_enable(init_nibble);
    _delay_ms(5);

    lcd_pulse_enable(init_nibble);
    _delay_us(150);

    lcd_pulse_enable(init_nibble);
    _delay_us(150);

    /* Schakel naar 4-bit modus: nibble 0x2 */
    uint8_t nibble_4bit = LCD_BACKLIGHT | LCD_BIT_D5;
    pcf8574_write(nibble_4bit);
    lcd_pulse_enable(nibble_4bit);
    _delay_us(150);

    /* Function Set: 4-bit, 2 lijnen, 5×8 font */
    lcd_cmd(0x28);
    _delay_us(50);

    /* Display UIT */
    lcd_cmd(0x08);
    _delay_us(50);

    /* Clear */
    lcd_cmd(0x01);
    _delay_ms(2);

    /* Entry Mode: links naar rechts */
    lcd_cmd(0x06);
    _delay_us(50);

    /* Display AAN, cursor UIT, blink UIT */
    lcd_cmd(0x0C);
    _delay_us(50);
}

void lcd_clear(void)
{
    lcd_cmd(0x01);
    _delay_ms(2);
}

void lcd_home(void)
{
    lcd_cmd(0x02);
    _delay_ms(2);
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
    static const uint8_t row_offsets[2] = {0x00, 0x40};
    lcd_cmd((uint8_t)(0x80 | (col + row_offsets[row & 1u])));
    _delay_us(50);
}

void lcd_print_char(char c)
{
    lcd_data((uint8_t)c);
    _delay_us(50);
}

void lcd_print(const char *str)
{
    while (*str) {
        lcd_print_char(*str++);
    }
}

void lcd_print_uint(uint32_t val)
{
    char buf[11];
    uint8_t i = 10;
    buf[10] = '\0';

    if (val == 0) {
        lcd_print_char('0');
        return;
    }
    while (val > 0 && i > 0) {
        buf[--i] = (char)('0' + (val % 10u));
        val /= 10u;
    }
    lcd_print(&buf[i]);
}

void lcd_print_fixed(uint32_t val_mm, uint8_t decimals)
{
    uint32_t divisor = 1u;
    for (uint8_t d = 0; d < decimals; d++) divisor *= 10u;

    uint32_t integer_part = val_mm / divisor;
    uint32_t frac_part    = val_mm % divisor;

    lcd_print_uint(integer_part);
    lcd_print_char('.');

    uint32_t tmp = divisor / 10u;
    while (tmp > 1u && frac_part < tmp) {
        lcd_print_char('0');
        tmp /= 10u;
    }
    if (frac_part > 0u) {
        lcd_print_uint(frac_part);
    }
}
