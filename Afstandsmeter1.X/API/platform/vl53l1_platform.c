/**
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "vl53l1_platform.h"
#include <string.h>
#include <time.h>
#include <math.h>
#include <i2c.c>

int8_t VL53L1_WriteMulti( uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count) {
	uint8_t status = 255;
	
     /* 
	 * WAT DEZE FUNCTIE DOET: 
	 * Deze functie stuurt een reeks data (een array) naar de sensor over I2C.
	 * ST geeft een 8-bit 'dev' adres mee. Om te schrijven moet de laagste bit een 0 zijn (dev & 0xFE).
	 * Het registeradres 'index' is 16-bit, dus knippen we dit in tweeën (Hoogste byte eerst).
	 */
	if (i2c_start(dev & 0xFE) != 0) {
		i2c_stop();
		return -1; // Fout bij het starten
	}
	
	// Stuur de twee bytes van het registeradres
	if (i2c_write((uint8_t)(index >> 8)) != 0) { i2c_stop(); return -1; }
	if (i2c_write((uint8_t)(index & 0xFF)) != 0) { i2c_stop(); return -1; }
	
	// Stuur alle data bytes vanuit de pdata array
	for (uint32_t i = 0; i < count; i++) {
		if (i2c_write(pdata[i]) != 0) { i2c_stop(); return -1; }
	}
	
	i2c_stop();
	status = 0; // 0 betekent succes
	
	return status;
}

int8_t VL53L1_ReadMulti(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count){
	uint8_t status = 255;
	
	/* 
	 * WAT DEZE FUNCTIE DOET: 
	 * Leest een aantal opeenvolgende bytes uit de sensor in de pdata array.
	 * Eerst vertellen we de sensor (in de schrijf-fase) WELK register we willen lezen.
	 * Daarna herstarten we I2C in lees-modus en halen we de data op.
	 */
	
	// Schrijffase
	if (i2c_start(dev & 0xFE) != 0) { i2c_stop(); return -1; }
	if (i2c_write((uint8_t)(index >> 8)) != 0) { i2c_stop(); return -1; }
	if (i2c_write((uint8_t)(index & 0xFF)) != 0) { i2c_stop(); return -1; }
	
	// Leesfase (bit 0 wordt een 1 om lezen aan te geven)
	if (i2c_start(dev | 0x01) != 0) { i2c_stop(); return -1; }
	
	// Bytes binnenhalen
	for (uint32_t i = 0; i < count; i++) {
		if (i == (count - 1)) {
			// Bij het allerlaatste byte moet altijd een NACK teruggestuurd worden
			pdata[i] = i2c_read_nack();
		} else {
			// Alle bytes ervóór krijgen een ACK
			pdata[i] = i2c_read_ack();
		}
	}
	
	i2c_stop();
	status = 0;
	
	return status;
}

int8_t VL53L1_WrByte(uint16_t dev, uint16_t index, uint8_t data) {
	uint8_t status = 255;
	
	/* 
	 * WAT DEZE FUNCTIE DOET: 
	 * Schrijft exact 1 byte naar een 16-bit register. 
	 * Omdat jij al een geniale hulpfunctie hiervoor hebt (i2c_write_reg16), 
	 * roepen we die simpelweg aan! 
	 * Let op: jouw functie verwacht een 7-bit adres, dus doen we 'dev >> 1'.
	 */
	if (i2c_write_reg16((uint8_t)(dev >> 1), index, data) == 0) {
		status = 0;
	} else {
		status = -1;
	}
	
	return status;
}

int8_t VL53L1_WrWord(uint16_t dev, uint16_t index, uint16_t data) {
	uint8_t status = 255;
	
	/* 
	 * WAT DEZE FUNCTIE DOET: 
	 * Schrijft 16-bit (2 bytes) data over de lijn. I2C kan maar 1 byte tegelijk sturen.
	 * De VL53L1X sensor is "Big-Endian" en verwacht de grootste byte eerst.
	 * We knippen 'data' op en gebruiken WriteMulti om ze te versturen.
	 */
	uint8_t buf[2];
	buf[0] = (uint8_t)(data >> 8);   // MSB
	buf[1] = (uint8_t)(data & 0xFF); // LSB
	
	status = VL53L1_WriteMulti(dev, index, buf, 2);
	
	return status;
}

int8_t VL53L1_WrDWord(uint16_t dev, uint16_t index, uint32_t data) {
	uint8_t status = 255;
	
	/* 
	 * WAT DEZE FUNCTIE DOET: 
	 * Zelfde als hierboven, maar dan knippen we een 32-bit (4 bytes) variabele op.
	 */
	uint8_t buf[4];
	buf[0] = (uint8_t)(data >> 24);
	buf[1] = (uint8_t)(data >> 16);
	buf[2] = (uint8_t)(data >> 8);
	buf[3] = (uint8_t)(data & 0xFF);
	
	status = VL53L1_WriteMulti(dev, index, buf, 4);
	
	return status;
}

int8_t VL53L1_RdByte(uint16_t dev, uint16_t index, uint8_t *data) {
	uint8_t status = 255;
	
	/* 
	 * WAT DEZE FUNCTIE DOET: 
	 * Leest exact 1 byte uit via I2C. Wederom hergebruiken we jouw eigen 
	 * slimme 'i2c_read_reg16' functie, en passen we het 8-bit naar 7-bit adres toe.
	 */
	if (i2c_read_reg16((uint8_t)(dev >> 1), index, data) == 0) {
		status = 0;
	} else {
		status = -1;
	}
	
	return status;
}

int8_t VL53L1_RdWord(uint16_t dev, uint16_t index, uint16_t *data) {
	uint8_t status = 255;
	
	/* 
	 * WAT DEZE FUNCTIE DOET: 
	 * Leest een 16-bit register. Dankzij jouw 'i2c_read_reg16_16' is dit 1 regel code.
	 */
	if (i2c_read_reg16_16((uint8_t)(dev >> 1), index, data) == 0) {
		status = 0;
	} else {
		status = -1;
	}
	
	return status;
}

int8_t VL53L1_RdDWord(uint16_t dev, uint16_t index, uint32_t *data) {
	uint8_t status = 255;
	
	/* 
	 * WAT DEZE FUNCTIE DOET: 
	 * Leest 4 bytes uit via ReadMulti. De ontvangen 4 losse bytes worden daarna 
	 * weer met Bit-Shift logica aan elkaar geplakt tot één grote 32-bit variabele.
	 */
	uint8_t buf[4];
	status = VL53L1_ReadMulti(dev, index, buf, 4);
	
	if (status == 0) {
		*data = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
	}
	
	return status;
}

int8_t VL53L1_WaitMs(uint16_t dev, int32_t wait_ms){
	uint8_t status = 255;
	
	/* 
	 * WAT DEZE FUNCTIE DOET: 
	 * Pauzeert het programma. De delay functie '_delay_ms()' uit de <util/delay.h> library
	 * werkt het beste met harde getallen (bijv: _delay_ms(100)). 
	 * Omdat we hier een variabele 'wait_ms' krijgen, roepen we de delay van 1ms aan 
	 * binnen een while-loop tot de tijd om is.
	 */
	while (wait_ms > 0) {
		_delay_ms(1);
		wait_ms--;
	}
	
	status = 0;
	
	return status;
}
