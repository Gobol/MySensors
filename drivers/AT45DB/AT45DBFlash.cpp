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

#include "AT45DBFlash.h"

#if defined(MY_DEBUG_VERBOSE_EEPROM)
#define EEP_DEBUG(x,...) DEBUG_OUTPUT(x, ##__VA_ARGS__)	//!< debug
//#define OTA_EXTRA_FLASH_DEBUG	//!< Dumps flash after each FW block
#else
#define EEP_DEBUG(x,...)	//!< debug NULL
#endif

uint8_t AT45DBFlash::UNIQUEID[8];

/// IMPORTANT: NAND FLASH memory requires erase before write, because
///            it can only transition from 1s to 0s and only the erase command can reset all 0s to 1s
/// See http://en.wikipedia.org/wiki/Flash_memory
/// The smallest range that can be erased is a sector (4K, 32K, 64K); 
/// In Atmel AT45DB chips there is NO a chip erase command !! 
/// Adesto chips has chip erase.

/// Library reserves PAGE 0 for UNIQUEID & other future variables


AT45DBFlash::AT45DBFlash(const AT45Flash expected_chip)
{
	_expected_chip = expected_chip;
	_cur_page_in_buf = -1;
	hwPinMode(MY_OTA_FLASH_SS, OUTPUT);
	hwDigitalWrite(MY_OTA_FLASH_SS, HIGH);
}

/// Select the flash chip
void AT45DBFlash::select()
{
	//save current SPI settings
#ifndef SPI_HAS_TRANSACTION
	noInterrupts();
#endif
#if defined(SPCR) && defined(SPSR)
	_SPCR = SPCR;
	_SPSR = SPSR;
#endif

#ifdef SPI_HAS_TRANSACTION
	SPI.beginTransaction(_settings);
#else
	// set FLASH SPI settings
	SPI.setDataMode(SPI_MODE0);
	SPI.setBitOrder(MSBFIRST);
	SPI.setClockDivider(SPI_CLOCK_DIV2); 
#endif
	hwDigitalWrite(MY_OTA_FLASH_SS, LOW);
}

/// UNselect the flash chip
void AT45DBFlash::unselect()
{
	hwDigitalWrite(MY_OTA_FLASH_SS, HIGH);
	//restore SPI settings to what they were before talking to the FLASH chip
#ifdef SPI_HAS_TRANSACTION
	SPI.endTransaction();
#else
	interrupts();
#endif
#if defined(SPCR) && defined(SPSR)
	SPCR = _SPCR;
	SPSR = _SPSR;
#endif
}

/// setup SPI, read device ID etc...
bool AT45DBFlash::initialize()
{
#if defined(SPCR) && defined(SPSR)
	_SPCR = SPCR;
	_SPSR = SPSR;
#endif
	SPI.begin(); 
	
#ifdef SPI_HAS_TRANSACTION
	_settings = SPISettings(MY_OTA_FLASH_SPI_CLOCK, MSBFIRST, SPI_MODE0);
#endif

	unselect();

	for (int i=0;i<10;i++) {
		// detection loop - expect CHIP READY and DENSITY == expected_chip density
		if (readStatus() != 0x00) {
			uint8_t sr = 0;
			while ((sr & 0x80)!=0x80) {
				sr = readStatus();
			}
			uint8_t den;
			den = (sr & 0b00111100) >> 2;
			EEP_DEBUG(PSTR("OTA:AT45:DENS=%" PRIu8 "\n"), den);
			den = (den-3) >> 1;
			EEP_DEBUG(PSTR("OTA:AT45:PGSZ=%" PRIu8 "\n"), AT45_PageBitSizes[den]);
			if ((uint8_t)(_expected_chip) == den)
			{
				EEP_DEBUG(PSTR("OTA:AT45:OK\n"));
				_density = den;
				return true;
			}
		}
	}
	EEP_DEBUG(PSTR("!OTA:AT45:NOT FOUND!\n"));
	return false;
}

uint16_t AT45DBFlash::addressToByteInPage(uint32_t addr) {
	return (addr % AT45_PageBytes[_density]);
}

uint16_t AT45DBFlash::addressToPage(uint32_t addr) {
	return (addr / AT45_PageBytes[_density]);
}

void AT45DBFlash::waitUntilBusy(void) {
	select();
	SPI.transfer(0xD7);
	while ((SPI.transfer(0x00) & 0x80)!=0x80) { }
	unselect();
}

void AT45DBFlash::send3bytesAddr(int page, int start_byte) {
	uint32_t adr = page;
	adr = (adr << AT45_PageBitSizes[_density]) + start_byte;
	SPI.transfer((uint8_t)(adr >> 16));
	SPI.transfer((uint8_t)(adr >> 8));
	SPI.transfer((uint8_t)(adr));
}

void AT45DBFlash::clearBuffer(uint8_t buf) {
	command(0x84+ 3*buf);
	send3bytesAddr(0,0);
	for (int i=0; i<264; i++) {
		SPI.transfer(0xff);		// transfering NOT-ed bytes !!!
	}
	unselect();
}

void AT45DBFlash::getPageToBuf(uint8_t buf, int page) {
	clearBuffer(buf);
	command(0x53+ 2*buf);
	send3bytesAddr(page, 0);
	unselect();
	waitUntilBusy();
	//Serial.printf("-gp2b: B:0x%02x P:0x%04x \n\r", buf, page);
}

void AT45DBFlash::writeByteToBuffer(uint8_t buf, uint16_t start_byte, uint8_t data) {
	command(0x84+ 3*buf);
	send3bytesAddr(0,start_byte);
	SPI.transfer(~data);				// transfering NOT-ed bytes !!!
	unselect();
	//Serial.printf("-wb2b: B:0x%02x @:0x%04x <= 0x%02x \n\r", buf, start_byte, data);
}

uint8_t* AT45DBFlash::readUniqueId()
{
	// EMULATED @ last page of chip - first 8 bytes
	for (uint8_t i=0; i<8; i++) {
		UNIQUEID[i] = readByte((AT45_PagesTotal[_density]-1)*AT45_PageBytes[_density] + i);
	}
	return UNIQUEID;
}

/// read 1 byte from flash memory
uint8_t AT45DBFlash::readByte(uint32_t addr)
{
	EEP_DEBUG(PSTR("OTA:AT45:RDBT=%" PRIu32 "\n"), addr);
	command(0xE8);
	send3bytesAddr(addressToPage(addr), addressToByteInPage(addr));	
	// now 4 dummy transfers to start READ
	for (int i=0;i<4;i++) {
		SPI.transfer(0x00);
	}
	uint8_t result = SPI.transfer(0x00);
	unselect();
	return result;
}

/// read unlimited # of bytes
void AT45DBFlash::readBytes(uint32_t addr, void* buf, uint16_t len)
{
	EEP_DEBUG(PSTR("OTA:AT45:RDBS=%" PRIu32 " L=%" PRIu16 "\n"), addr, len);
	command(0xE8);
	send3bytesAddr(addressToPage(addr), addressToByteInPage(addr));
	// now 4 dummy transfers to start READ
	for (int i=0;i<4;i++) {
		SPI.transfer(0x00);
	}
	for (uint16_t i = 0; i < len; ++i) {
		((uint8_t*) buf)[i] = ~SPI.transfer(0x00);
	}
	unselect();
}

/// Send a command to the flash chip, pass TRUE for isWrite when its a write command
void AT45DBFlash::command(uint8_t cmd, bool isWrite)
{
#if defined(__AVR_ATmega32U4__) // Arduino Leonardo, MoteinoLeo
	DDRB |= B00000001;            // Make sure the SS pin (PB0 - used by RFM12B on MoteinoLeo R1) is set as output HIGH!
	PORTB |= B00000001;
#endif
	// wait for chip ready;
	while (busy());
	select();
	SPI.transfer(cmd);
}

/// check if the chip is busy erasing/writing
bool AT45DBFlash::busy()
{
	return ((readStatus() & 0x80)!=0x80);
}

/// return the STATUS register
uint8_t AT45DBFlash::readStatus()
{
	select();
	SPI.transfer(0xD7);
	uint8_t r = SPI.transfer(0x00);
	unselect();
	return r;
}

void AT45DBFlash::programBufToPage(uint8_t buf,int page) {
	command(0x83+ 2*buf);
	send3bytesAddr(page, 0);
	unselect();
	//Serial.printf("-pb2p: B:0x%02x ==> P:0x%04x \n\r", buf, page);
}

void AT45DBFlash::startContWrite(uint32_t start_addr) {
	_cur_page_in_buf = addressToPage(start_addr);
	getPageToBuf(0, _cur_page_in_buf);
	//Serial.printf("-stcW: @:0x%06x ==> \n\r", start_addr);
}

void AT45DBFlash::writeContByte(uint32_t addr, uint8_t byte) {
	uint16_t _act_addr_page = addressToPage(addr);
	if (_cur_page_in_buf!=_act_addr_page) {
		programBufToPage(0, _cur_page_in_buf);
		_cur_page_in_buf = _act_addr_page;
		getPageToBuf(0, _cur_page_in_buf);		// CHECK IT: Buffer is not updated with new page...
	}
	writeByteToBuffer(0, addressToByteInPage(addr), byte);
}

void AT45DBFlash::stopContWrite() {
	programBufToPage(0, _cur_page_in_buf);
	//Serial.printf("-stopcW:\n\r");
}


/// Write 1 byte to flash memory
/// library takes care about erasing / pagination... full emulation of linear storage

void AT45DBFlash::writeByte(uint32_t addr, uint8_t byt)
{
	EEP_DEBUG(PSTR("OTA:AT45:WRBT=%" PRIu32 " B=%" PRIu8 "\n"), addr, byt);
	startContWrite(addr);
	writeContByte(addr,byt);
	stopContWrite();
}

/// write multiple bytes to flash memory (up to 64K)
/// library takes care about erasing / pagination... full emulation of linear storage

void AT45DBFlash::writeBytes(uint32_t addr, const void* buf, uint16_t len)
{
	EEP_DEBUG(PSTR("OTA:AT45:WRBTS=%" PRIu32 " L=%" PRIu16 "\n"), addr, len);
	startContWrite(addr);
	
	for (uint16_t i=0; i<len; i++) {
		writeContByte(addr+i, ((uint8_t*) buf)[i]);
	}

	stopContWrite();
}

void AT45DBFlash::erasePage(int page) {
	command(0x81);
	send3bytesAddr(page,0);
	unselect();
	waitUntilBusy();
}

/*
	Block consist of 8 pages
*/
void AT45DBFlash::eraseBlock(int block) {
	EEP_DEBUG(PSTR("OTA:AT45:ERBLK=%" PRIu16 "\n"), block);
	command(0x50);
	send3bytesAddr(block << 3,0);
	unselect();
	waitUntilBusy();
}

void AT45DBFlash::eraseChip() {
	EEP_DEBUG(PSTR("OTA:AT45:ERCHIP"));
	int bl = 0;
	for (bl=0 ; bl<(AT45_PagesTotal[_density] >> 3) ; bl++)
	{
		eraseBlock(bl);
	}
}

/// erase entire flash memory array
/// may take several seconds depending on size, but is non blocking
/// so you may wait for this to complete using busy() or continue doing
/// other things and later check if the chip is done with busy()
/// note that any command will first wait for chip to become available using busy()
/// so no need to do that twice
void AT45DBFlash::chipErase()
{
	eraseChip();
}

/// erase a 4Kbyte block
void AT45DBFlash::blockErase4K(uint32_t addr)
{
	// 4K = 4096/256 = 16 pages	= 2 blocks
	int page = addressToPage(addr);
	int num_pages = (4096/AT45_PageBytes[_density]) + 1;
	for (int i=0; i<num_pages;i+=8)
		eraseBlock(page+i);
}

/// erase a 32Kbyte block
void AT45DBFlash::blockErase32K(uint32_t addr)
{
		// 32K = 32768/256 = 128 pages	= 16 blocks
		int page = addressToPage(addr);
		int num_pages = (32768/AT45_PageBytes[_density]) + 1;
		for (int i=0; i<num_pages;i+=8)
			eraseBlock(page+i);
}
/// erase a 64Kbyte block
void AT45DBFlash::blockErase64K(uint32_t addr)
{
	int page = addressToPage(addr);
	int num_pages = (655356/AT45_PageBytes[_density]) + 1;
	for (int i=0; i<num_pages;i+=8)
		eraseBlock(page+i);
}

void AT45DBFlash::sleep()
{
	// unsupported unless Adesto chip
}

void AT45DBFlash::wakeup()
{
	// unsupported unless Adesto chip
}

/// cleanup
void AT45DBFlash::end()
{
	SPI.end();
}
