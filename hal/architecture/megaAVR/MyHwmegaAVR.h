/*
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2019 Sensnology AB
 * Full contributor list: https://github.com/mysensors/MySensors/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef MyHwmegaAVR_h
#define MyHwmegaAVR_h

#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/io.h>
//#include <avr/boot.h>
#include <util/atomic.h>
#include <SPI.h>

// Fast IO driver   - not needed (megaTinyCore implements this on CORE_HAS_FASTIO capable MPUs)
//#include "drivers/DigitalWriteFast/digitalWriteFast.h"

// SOFTSPI
#ifdef MY_SOFTSPI
#	include "hal/architecture/AVR/drivers/DigitalIO/DigitalIO.h"
#endif

#ifdef __cplusplus
#include <Arduino.h>
#endif

#define CRYPTO_LITTLE_ENDIAN

#ifndef MY_SERIALDEVICE
#define MY_SERIALDEVICE Serial
#endif

#ifndef MY_DEBUGDEVICE
#define MY_DEBUGDEVICE MY_SERIALDEVICE
#endif

// AVR temperature calibration reference: http://ww1.microchip.com/downloads/en/AppNotes/Atmel-8108-Calibration-of-the-AVRs-Internal-Temperature-Reference_ApplicationNote_AVR122.pdf
#ifndef MY_AVR_TEMPERATURE_OFFSET
#define MY_AVR_TEMPERATURE_OFFSET (324.31f)
#endif

#ifndef MY_AVR_TEMPERATURE_GAIN
#define MY_AVR_TEMPERATURE_GAIN (1.22f)
#endif

#ifndef wdt_enable
#	define wdt_enable() _PROTECTED_WRITE(WDT.CTRLA,WDT_PERIOD_8KCLK_gc)
#	define wdt_reset()	__asm__ __volatile__ ("wdr"::)
#	define wdt_disable() _PROTECTED_WRITE(WDT.CTRLA,0)
#endif

// Define these as macros to save valuable space
#define hwDigitalWrite(__pin, __value) digitalWriteFast(__pin, __value)
#define hwDigitalRead(__pin) digitalReadFast(__pin)
#if (CORE_HAS_FASTPINMODE == 1)
	#define hwPinMode(__pin, __value) pinModeFast(__pin, __value)
#else
	#define hwPinMode(__pin, __value) pinMode(__pin, __value)
#endif

bool hwInit(void);

#define hwWatchdogReset() wdt_reset()
//#define hwReboot() wdt_enable(WDTO_15MS); while (1)
#define hwReboot() _PROTECTED_WRITE(RSTCTRL.SWRR,1);
#define hwMillis() millis()
#define hwReadConfig(__pos) eeprom_read_byte((const uint8_t *)__pos)
#define hwWriteConfig(__pos, __val) eeprom_update_byte((uint8_t *)__pos, (uint8_t)__val)
#define hwReadConfigBlock(__buf, __pos, __length) eeprom_read_block((void *)__buf, (const void *)__pos, (uint32_t)__length)
#define hwWriteConfigBlock(__buf, __pos, __length) eeprom_update_block((const void *)__buf, (void *)__pos, (uint32_t)__length)

inline void hwRandomNumberInit(void);
uint32_t hwInternalSleep(uint32_t ms);

#if defined(MY_SOFTSPI)
SoftSPI<MY_SOFT_SPI_MISO_PIN, MY_SOFT_SPI_MOSI_PIN, MY_SOFT_SPI_SCK_PIN, 0> hwSPI; //!< hwSPI
#else
#define hwSPI SPI //!< hwSPI
#endif

#ifndef DOXYGEN
#define MY_CRITICAL_SECTION     ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#endif  /* DOXYGEN */

const uint8_t digital_pin_to_port_int_vect[] = {
	  // Left side, top to bottom
	  PORTA_PORT_vect_num, // 0  PA4
	  PORTA_PORT_vect_num, // 1  PA5
	  PORTA_PORT_vect_num, // 2  PA6
	  PORTA_PORT_vect_num, // 3  PA7
	  PORTB_PORT_vect_num, // 4  PB7
	  PORTB_PORT_vect_num, // 5  PB6
	  PORTB_PORT_vect_num, // 6  PB5
	  PORTB_PORT_vect_num, // 7  PB4
	  PORTB_PORT_vect_num, // 8  PB3
	  PORTB_PORT_vect_num, // 9  PB2
	  PORTB_PORT_vect_num, // 10 PB1
	  PORTB_PORT_vect_num, // 11 PB0
	  PORTC_PORT_vect_num, // 12 PC0
	  PORTC_PORT_vect_num, // 13 PC1
	  PORTC_PORT_vect_num, // 14 PC2
	  PORTC_PORT_vect_num, // 15 PC3
	  PORTA_PORT_vect_num, // 16 PA4
	  PORTA_PORT_vect_num, // 17 PA5
	  PORTA_PORT_vect_num, // 18 PA1
	  PORTA_PORT_vect_num, // 19 PA2
	  PORTA_PORT_vect_num, // 20 PA3
	  PORTA_PORT_vect_num  // 21 PA0
};

#define digitalPinToPortIntVect(x)		(digital_pin_to_port_int_vect[x])

#endif
