// Copyright (c) 2022 by Bogusz Jagoda 
// AT45DB Flash memory library, for MySensors
// DEPENDS ON: Arduino SPI library
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it
// and/or modify it under the terms of the GNU General
// Public License as published by the Free Software
// Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE. See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General
// Public License along with this program.
// If not, see <http://www.gnu.org/licenses/>.
//
// Licence can be viewed at
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code

///
/// @file AT45DBFlash.h
///
/// @brief AT45DBFlash provides access to a SPI Flash IC for OTA update or storing data
///
/// IMPORTANT: NAND FLASH memory requires erase before write, because
///            it can only transition from 1s to 0s and only the erase command can reset all 0s to 1s
/// See http://en.wikipedia.org/wiki/Flash_memory
/// The smallest range that can be erased is a sector (4K, 32K, 64K); there is also a chip erase command
///
/// Atmel/Adesto DataFlash mode (264/528/1056 byte page) SPI flash commands <BR>
/// Assuming the WP pin is pulled up (to disable hardware write protection).<BR>

#ifndef _AT45DBFLASH_H_
#define _AT45DBFLASH_H_

#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <wiring.h>
#include "pins_arduino.h"
#endif

enum AT45Flash {
	AT45DB011 = 0,
	AT45DB021,
	AT45DB041,
	AT45DB081,
	AT45DB161,
	AT45DB321,
	AT45DB641
};


// CHIP     DENSITY CODE      PAGE SIZE      MEM TOTAL      ORGANIZATION
//  011		  0011 (3)			256/264 (+8)	1Mb          512 x 264
//  021       0101 (5)			256/264	(+8)	2Mb         1024 x 264
//  041       0111 (7)			256/264	(+8)	4Mb         2048 x 264
//  081       1001 (9)			256/264	(+8)	8Mb         4096 x 264
//  161       1011 (11)			512/528 (+16)	16Mb		4096 x 528
//  321		  1101 (13)			512/528			32Mb        8192 x 528
//  641       1111 (15)		   1024/1056 (+32)	64Mb        8192 x 1056

const int AT45_PageBitSizes[7] = {9,9,9,9,10,10,11};
const int AT45_PagesTotal[7]   = {512,1024,2048,4096,4096,8192,8192};
const int AT45_PageBytes[7]	   = {264, 264, 264, 264, 528, 528,1056};
	
	
#include <SPI.h>

/** AT45DBFlash class */
class AT45DBFlash
{
	public:
	static uint8_t UNIQUEID[8]; //!< Storage for unique identifier
	explicit AT45DBFlash(AT45Flash expected_chip); //!< Constructor
	bool initialize(); //!< setup SPI, read device ID etc...	
	void command(uint8_t cmd, bool isWrite=false); //!< Send a command to the flash chip, pass TRUE for isWrite when its a write command
	uint8_t readStatus(); //!< return the STATUS register
	uint8_t readByte(uint32_t addr); //!< read 1 byte from flash memory
	void readBytes(uint32_t addr, void* buf, uint16_t len); //!< read unlimited # of bytes
	void writeByte(uint32_t addr, uint8_t byt); //!< Write 1 byte to flash memory
	void writeBytes(uint32_t addr, const void* buf,uint16_t len); //!< write multiple bytes to flash memory (up to 64K), if define SPIFLASH_SST25TYPE is set AAI Word Programming will be used
	bool busy(); //!< check if the chip is busy erasing/writing
	void chipErase(); //!< erase entire flash memory array
	void blockErase4K(uint32_t address); //!< erase a 4Kbyte block
	void blockErase32K(uint32_t address); //!< erase a 32Kbyte block
	void blockErase64K(uint32_t addr); //!< erase a 64Kbyte block
	uint8_t* readUniqueId(); //!< Get the 64 bit unique identifier, stores it in @ref UNIQUEID[8]
        
	void sleep(); //!< Put device to sleep
	void wakeup(); //!< Wake device
	void end(); //!< end
	
	protected:
	void select(); //!< select
	void unselect(); //!< unselect
	uint16_t addressToByteInPage(uint32_t addr);		//!< linear addr conversion to byte number in page		(addr % page_size)
	uint16_t addressToPage(uint32_t addr);				//!< linear addr conversion to page number				(addr / page_size)
	void waitUntilBusy();								//!< wait until chip busy	
	void send3bytesAddr(int page, int start_byte);		//!< internal - send 3 bytes of address to chip	
	void startContRead(uint32_t start_addr);			//!< start continuous memory read
	void getPageToBuf(uint8_t buf, int page);			//!< fetch main memory page into #buf buffer
	void clearBuffer(uint8_t buf);						//!< clear internal buffer 
	void writeByteToBuffer(uint8_t buf, uint16_t start_byte, uint8_t data);		//!< Write single byte (data) into internal buffer #buf
	void programBufToPage(uint8_t buf,int page);		//!< program buffer #buf into #page		(WRITE TO EEPROM)
	void startContWrite(uint32_t start_addr);			//!< prepare continuous write sequence
	void writeContByte(uint32_t addr, uint8_t byte);	//!< write single byte in write continuous sequence
	void stopContWrite();								//!< stop continuous write sequence
	void erasePage(int page);							//!< erase single page
	void eraseBlock(int block);							//!< erase whole block (8 pages)
	void eraseChip();									//!< erase whole chip
	
	AT45Flash _expected_chip; //!< Expected chip in initialization stage - one of AT45Flash enum 
	uint8_t _SPCR; //!< SPCR
	uint8_t _SPSR; //!< SPSR
	uint8_t _stat_reg;	//!< Status register
	uint8_t _density;	//!< Chip density
	int32_t _cur_page_in_buf;		//!< current memory page in chip's internal buffer,  -1 if no actual page in EEPROM internal buffer
	#ifdef SPI_HAS_TRANSACTION
	SPISettings _settings;
	#endif
};

#endif