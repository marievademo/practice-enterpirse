#ifndef I2C_H
#define I2C_H

#include <stdint.h>

/**
 * i2c.h ? Hardware I2C (TWI) driver voor ATmega32U4
 * Bare-metal, geen libraries.
 */

void    i2c_init(void);
uint8_t i2c_start(uint8_t addr_rw);   /* addr_rw = (7-bit addr << 1) | R/W */
void    i2c_stop(void);
uint8_t i2c_write(uint8_t data);
uint8_t i2c_read_ack(void);
uint8_t i2c_read_nack(void);

/* Hulpfuncties voor burst-toegang */
uint8_t i2c_write_reg16(uint8_t dev_addr, uint16_t reg, uint8_t val);
uint8_t i2c_read_reg16(uint8_t dev_addr, uint16_t reg, uint8_t *val);
uint8_t i2c_read_reg16_16(uint8_t dev_addr, uint16_t reg, uint16_t *val);

#endif /* I2C_H */
