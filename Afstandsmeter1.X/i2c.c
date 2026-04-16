/**
 * i2c.c ? Bare-metal TWI driver voor ATmega32U4 @ 8 MHz
 * I2C klokfrequentie: 100 kHz
 *
 * TWBR formule: TWBR = ((F_CPU / F_SCL) - 16) / (2 * prescaler)
 * @ 8 MHz, 100 kHz, prescaler=1: TWBR = ((8000000/100000) - 16) / 2 = 32
 */

#include "i2c.h"
#include <avr/io.h>   /*In bare-metal C manipuleer je hardware door eentjes en nulletjes naar specifieke fysieke geheugenadressen in de chip te sturen.*/
#include <util/twi.h>   /*Als je I²C gebruikt, geeft de hardware na elke stap een statuscode terug. Deze code kun je uitlezen uit het statusregister.*/

/* ?? Lokale macros (niet zichtbaar buiten dit bestand) ?? */
#define I2C_TWBR_VALUE   32u   /* zie berekening hierboven */
#define I2C_PRESCALER    0u    /* bits TWPS1:TWPS0 = 00 ? prescaler = 1 */

/* Wacht tot TWI klaar is */
#define TWI_WAIT()  while (!(TWCR & (1 << TWINT)))

/* ?? Publieke functies ?? */

void i2c_init(void)
{
    TWSR = I2C_PRESCALER;          /* prescaler = 1 */
    TWBR = I2C_TWBR_VALUE;
    TWCR = (1 << TWEN);           /* activeer TWI module */
}

uint8_t i2c_start(uint8_t addr_rw)
{
    /* Stuur START conditie */
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    TWI_WAIT();

    if ((TW_STATUS != TW_START) && (TW_STATUS != TW_REP_START)) {
        return 1; /* fout */
    }

    /* Stuur adres + R/W bit */
    TWDR = addr_rw;
    TWCR = (1 << TWINT) | (1 << TWEN);
    TWI_WAIT();

    if ((TW_STATUS != TW_MT_SLA_ACK) && (TW_STATUS != TW_MR_SLA_ACK)) {
        return 1; /* geen ACK van slave */
    }
    return 0; /* OK */
}

void i2c_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    /* Geen wacht nodig ? TWSTO wordt automatisch gewist */
}

uint8_t i2c_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    TWI_WAIT();

    if (TW_STATUS != TW_MT_DATA_ACK) {
        return 1; /* geen ACK */
    }
    return 0;
}

uint8_t i2c_read_ack(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    TWI_WAIT();
    return TWDR;
}

uint8_t i2c_read_nack(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN);
    TWI_WAIT();
    return TWDR;
}

/* ?? Hulpfuncties voor 16-bit registeradressering (VL53L1X) ?? */

uint8_t i2c_write_reg16(uint8_t dev_addr, uint16_t reg, uint8_t val)
{
    if (i2c_start((uint8_t)(dev_addr << 1) | TW_WRITE)) goto err;
    if (i2c_write((uint8_t)(reg >> 8)))                  goto err;
    if (i2c_write((uint8_t)(reg & 0xFF)))                goto err;
    if (i2c_write(val))                                   goto err;
    i2c_stop();
    return 0;
err:
    i2c_stop();
    return 1;
}

uint8_t i2c_read_reg16(uint8_t dev_addr, uint16_t reg, uint8_t *val)
{
    if (i2c_start((uint8_t)(dev_addr << 1) | TW_WRITE)) goto err;
    if (i2c_write((uint8_t)(reg >> 8)))                  goto err;
    if (i2c_write((uint8_t)(reg & 0xFF)))                goto err;
    if (i2c_start((uint8_t)(dev_addr << 1) | TW_READ))  goto err;
    *val = i2c_read_nack();
    i2c_stop();
    return 0;
err:
    i2c_stop();
    return 1;
}

uint8_t i2c_read_reg16_16(uint8_t dev_addr, uint16_t reg, uint16_t *val)
{
    uint8_t hi, lo;
    if (i2c_start((uint8_t)(dev_addr << 1) | TW_WRITE)) goto err;
    if (i2c_write((uint8_t)(reg >> 8)))                  goto err;
    if (i2c_write((uint8_t)(reg & 0xFF)))                goto err;
    if (i2c_start((uint8_t)(dev_addr << 1) | TW_READ))  goto err;
    hi = i2c_read_ack();
    lo = i2c_read_nack();
    *val = ((uint16_t)hi << 8) | lo;
    i2c_stop();
    return 0;
err:
    i2c_stop();
    return 1;
}
