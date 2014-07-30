/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Billy Millare
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * 05may2014
 */

/**
 * bit-banging the CC debug protocol
 */

#ifndef CCDBG_H_
#define CCDBG_H_

#include "ccdbg-device.h"

enum {
	CCDBG_CHIP_ID_CC2530	= 0xa5,
	CCDBG_CHIP_ID_CC2531	= 0xb5,
	CCDBG_CHIP_ID_CC2533	= 0x95,
	CCDBG_CHIP_ID_CC2540	= 0x8d,
	CCDBG_CHIP_ID_CC2541	= 0x41
};

typedef struct {
	unsigned char id;
	unsigned char rev;
	unsigned int flashSize;
	unsigned int writableFlashSize;
	unsigned int flashBankSize;
	unsigned int flashPageSize;
	unsigned int numberOfFlashPages;
	unsigned int sramSize;
	int isLocked;
	unsigned int ieeeAddressLength;
	unsigned char ieeeAddress[8];
} CCDBG_INFO, *CCDBG_ID;

#define CCDBG_INVALID_ID	(CCDBG_ID)0

typedef enum {
	CCDBG_COMMAND_CHIP_ERASE		= 0x02,
	CCDBG_COMMAND_WR_CONFIG			= 0x03,
	CCDBG_COMMAND_RD_CONFIG			= 0x04,
	CCDBG_COMMAND_GET_PC			= 0x05,
	CCDBG_COMMAND_READ_STATUS		= 0x06,
	CCDBG_COMMAND_SET_HW_BRKPNT		= 0x07,
	CCDBG_COMMAND_HALT				= 0x08,
	CCDBG_COMMAND_RESUME			= 0x09,
	CCDBG_COMMAND_DEBUG_INSTR		= 0x0a,
	CCDBG_COMMAND_STEP_INSTR		= 0x0b,
	CCDBG_COMMAND_GET_BM			= 0x0c,
	CCDBG_COMMAND_GET_CHIP_ID		= 0x0d,
	CCDBG_COMMAND_BURST_WRITE		= 0x10
} CCDBG_COMMAND;

typedef enum {
	CCDBG_STATUS_STACK_OVERFLOW		= 0x01,
	CCDBG_STATUS_OSCILLATOR_STABLE	= 0x02,
	CCDBG_STATUS_DEBUG_LOCKED		= 0x04,
	CCDBG_STATUS_HALT_STATUS		= 0x08,
	CCDBG_STATUS_PM_ACTIVE			= 0x10,
	CCDBG_STATUS_CPU_HALTED			= 0x20,
	CCDBG_STATUS_PCON_IDLE			= 0x40,
	CCDBG_STATUS_CHIP_ERASE_BUSY	= 0x80
} CCDBG_STATUS;

typedef enum {
	CCDBG_CONFIG_TIMER_SUSPENDED	= 0x02,
	CCDBG_CONFIG_DMA_PAUSED			= 0x04,
	CCDBG_CONFIG_TIMERS_DISABLED	= 0x08,
	CCDBG_CONFIG_SOFT_POWER_MODE	= 0x20
} CCDBG_CONFIG;

/**
 * default number of retries in reading the chip's response to a command
 */
extern int ccdbg_retries;

/**
 * put the chip in debug mode
 */
void ccdbg_reset(void);

/**
 * issue a debug command
 *
 * command - debug command
 * inputDataSize - size of additional command data
 * inputData - additional command data
 * outputDataSize - size of data received from the chip
 * outputData - data received from the chip
 * retry - number of retries in receiving response from the chip
 *
 * returns the chip's response which is the same as the value of
 *   outputData if successful, negative value if no response is
 *   received from the chip
 */
int ccdbg_command(CCDBG_COMMAND command, unsigned int inputDataSize, const unsigned char *inputData, unsigned int *outputDataSize, unsigned short *outputData, int retries);

/**
 * identify and get the chip's info
 *
 * id - chip's uninitialized identification token
 *
 * returns the chip's identification containing information about
 *   the chip which is the same as id if successful, CCDBG_INVALID_ID
 *   if chip cannot be identified
 */
CCDBG_ID ccdbg_identifyChip(CCDBG_ID id);

/**
 * execute a CPU instruction
 *
 * id - chip's identification
 * size - size of instruction
 * instruction - instruction bytes
 *
 * returns the resulting accumulator register value after the instruction
 *   has been executed if successful, a value less than zero for error
 */
int ccdbg_executeInstruction(CCDBG_ID id, unsigned int size, const unsigned char *instruction);

/**
 * read from the chip's memory
 *
 * id - chip's identification
 * address - memory base address
 * size - memory data size
 * data - destination buffer of memory data
 *
 * returns the value of the first byte of data if successful,
 *   a value less than zero for error
 */
int ccdbg_readMemory(CCDBG_ID id, unsigned int address, unsigned int size, unsigned char *data);

/**
 * write to the chip's memory
 *
 * id - chip's identification
 * address - memory base address
 * size - memory data size
 * data - source buffer of memory data
 * verify - verify written data or not
 *
 * returns 0 if successful, non-zero otherwise
 */
int ccdbg_writeMemory(CCDBG_ID id, unsigned int address, unsigned int size, const unsigned char *data, int verify);

/**
 * check if flash page is locked for writing
 *
 * id - chip's identification
 * page - flash page
 *
 * returns 0 if not locked, greater than zero if locked,
 *   and less than zero for error
 */
int ccdbg_isFlashPageLocked(CCDBG_ID id, unsigned int page);

/**
 * lock contiguous flash pages
 *
 * id - chip's identification
 * startPage - starting page
 * numberOfPages - number of pages starting from startPage
 *
 * returns 0 if successful, non-zero otherwise
 */
int ccdbg_lockFlashPages(CCDBG_ID id, unsigned int startPage, unsigned int numberOfPages);

/**
 * unlock contiguous flash pages
 *
 * id - chip's identification
 * startPage - starting page
 * numberOfPages - number of pages starting from startPage
 *
 * returns 0 if successful, non-zero otherwise
 */
int ccdbg_unlockFlashPages(CCDBG_ID id, unsigned int startPage, unsigned int numberOfPages);

/**
 * read from a chip's flash page
 *
 * id - chip's identification
 * page - flash page
 * data - destination buffer of flash data
 *
 * returns 0 if successful, non-zero otherwise
 */
int ccdbg_readFlashPage(CCDBG_ID id, unsigned int page, unsigned char *data);

/**
 * write to a chip's flash page
 *
 * id - chip's identification
 * page - flash page
 * data - source buffer of flash data
 * verify - verify written data or not
 *
 * returns 0 if successful, non-zero otherwise
 */
int ccdbg_writeFlashPage(CCDBG_ID id, unsigned int page, const unsigned char *data, int verify);

/**
 * erase a chip's flash page
 *
 * id - chip's identification
 * page - flash page
 *
 * returns 0 if successful, non-zero otherwise
 */
int ccdbg_eraseFlashPage(CCDBG_ID id, unsigned int page);

/**
 * read from the chip's flash
 *
 * id - chip's identification
 * address - flash base address
 * size - flash data size
 * data - destination buffer of flash data
 *
 * returns the number of bytes read if successful,
 *   a negative value if unsuccessful with its ones' complement
 *   representing the number of bytes successfully read
 */
int ccdbg_readFlash(CCDBG_ID id, unsigned int address, unsigned int size, unsigned char *data);

/**
 * write to the chip's flash
 *
 * id - chip's identification
 * address - flash base address
 * size - flash data size
 * data - source buffer of flash data
 * verify - verify written data or not
 *
 * returns the number of bytes written if successful,
 *   a negative value if unsuccessful with its ones' complement
 *   representing the number of bytes successfully written
 */
int ccdbg_writeFlash(CCDBG_ID id, unsigned int address, unsigned int size, const unsigned char *data, int verify);

/**
 * erase the chip's flash
 *
 * id - chip's identification
 *
 * returns 0 if successful, non-zero otherwise
 */
int ccdbg_eraseFlash(CCDBG_ID id);

/**
 * lock the chip's debug interface
 *
 * id - chip's identification
 *
 * returns 0 if successful, non-zero otherwise
 */
int ccdbg_lock(CCDBG_ID id);

#endif /* CCDBG_H_ */
