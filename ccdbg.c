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
 * 25apr2014
 */

/**
 * bit-banging the CC debug protocol
 */

#include "ccdbg.h"

int ccdbg_retries = 1;

static void toggleDC(void)
{
	CCDBG_DC_HIGH();
	CCDBG_DELAY();
	CCDBG_DC_LOW();
	CCDBG_DELAY();
}

static void writeByte(unsigned int byte)
{
	unsigned int mask = 0x80;

	for( ; mask != 0x00; mask >>= 1)
	{
		if((byte & mask) == 0)
			CCDBG_DD_LOW();
		else
			CCDBG_DD_HIGH();

		toggleDC();
	}
}

static unsigned int readByte(void)
{
	unsigned int byte = 0x00;
	int bit;
	int i;

	for(i = 8; i-- > 0; )
	{
		CCDBG_DC_HIGH();
		CCDBG_DELAY();
		CCDBG_DC_LOW();
		bit = CCDBG_DD();
		CCDBG_DELAY();
		byte |= (((bit == 0) ? 0x0 : 0x1) << i);
	}

	return byte;
}

void ccdbg_reset(void)
{
	CCDBG_RESET_OUT();
	CCDBG_DC_OUT();
	CCDBG_RESET_HIGH();
	CCDBG_DC_LOW();
	CCDBG_DELAY();
	CCDBG_RESET_LOW();
	CCDBG_DELAY();
	toggleDC();
	toggleDC();
	CCDBG_RESET_HIGH();
	CCDBG_DELAY();
}

int ccdbg_command(CCDBG_COMMAND command, unsigned int inputDataSize, const unsigned char *inputData, unsigned int *outputDataSize, unsigned short *outputData, int retries)
{
	unsigned int _outputDataSize;
	unsigned short _outputData;
	unsigned int commandByte = command << 3;
	unsigned int i;

	if(inputDataSize > 0 && inputData == 0)
		return -1;

	if(outputDataSize == 0)
		outputDataSize = &_outputDataSize;

	if(outputData == 0)
		outputData = &_outputData;

	*outputDataSize = 1;
	*outputData = 0;

	switch(command)
	{
	case CCDBG_COMMAND_DEBUG_INSTR:
		commandByte |= (inputDataSize & 0x3);
		break;

	case CCDBG_COMMAND_BURST_WRITE:
		commandByte |= ((inputDataSize & 0x7ff) >> 8);
		break;

	case CCDBG_COMMAND_GET_PC:
	case CCDBG_COMMAND_GET_CHIP_ID:
		*outputDataSize = 2;
		break;

	default:
		break;
	}

	/**
	 * write phase
	 */

	CCDBG_DD_OUT();

	writeByte(commandByte);

	if(command == CCDBG_COMMAND_BURST_WRITE)
		writeByte((inputDataSize & 0xff));

	for(i = 0; i < inputDataSize; i++)
		writeByte((unsigned int)inputData[i]);

	/**
	 * read phase
	 */

	CCDBG_DD_IN();

	while(1)
	{
		CCDBG_DELAY();

		if(CCDBG_DD() == 0)
		{
			for(i = 0; i < *outputDataSize; i++)
				((unsigned char *)outputData)[i] = readByte() & 0xff;

			return *outputData;
		}

		if(retries-- == 0)
			break;

		readByte();
		CCDBG_DELAY();
	}

	return -1;
}

/*****************************************************************************/

#define KB(x)			(x * 1024)
#define UNKNOWN_CHIP	(unsigned char)0xff

static struct {
	unsigned char id;
	unsigned int flashPageSize;
} chip[] = {
		{ CCDBG_CHIP_ID_CC2530, KB(2) },
		{ CCDBG_CHIP_ID_CC2531, KB(2) },
		{ CCDBG_CHIP_ID_CC2533, KB(1) },
		{ CCDBG_CHIP_ID_CC2540, KB(2) },
		{ CCDBG_CHIP_ID_CC2541, KB(2) },
		{ UNKNOWN_CHIP }
};

#define MAXIMUM_FLASH_PAGE_SIZE		KB(2)
#define FLASH_PAGE_LOCK_BITS_SIZE	16

enum {
	REG_CHIPID		= 0x624a,
	REG_CHVER		= 0x6249,
	REG_CHIPINFO0	= 0x6276,
	REG_CHIPINFO1	= 0x6277,
	REG_MEMCTR		= 0x70c7,
	REG_FADDRL		= 0x6271,
	REG_FADDRH		= 0x6272,
	REG_FCTL		= 0x6270,
	REG_DMA1CFGL	= 0x70d2,
	REG_DMA1CFGH	= 0x70d3,
	REG_DMA0CFGL	= 0x70d4,
	REG_DMA0CFGH	= 0x70d5,
	REG_DMAARM		= 0x70d6,
	REG_XDATA		= 0x8000
};

enum {
	FCTL_ERASE		= 0x01,
	FCTL_WRITE		= 0x02,
	FCTL_ABORT		= 0x20,
	FCTL_FULL		= 0x40,
	FCTL_BUSY		= 0x80,
	FCTL_CM			= 0x04
};

#define executeInstruction(size, instruction) \
	ccdbg_command(CCDBG_COMMAND_DEBUG_INSTR, size, instruction, 0, 0, ccdbg_retries)

CCDBG_ID ccdbg_identifyChip(CCDBG_ID id)
{
	int value;
	int i;

	if(id == CCDBG_INVALID_ID)
		return CCDBG_INVALID_ID;

	/**
	 * reset the chip and put it in debug mode
	 */
	ccdbg_reset();

	/**
	 * get the chip's ID and version
	 */
	if(ccdbg_command(CCDBG_COMMAND_GET_CHIP_ID, 0, 0, 0, (unsigned short *)id, ccdbg_retries) < 0)
		return CCDBG_INVALID_ID;

	/**
	 * check if chip is supported
	 */
	for(i = 0; id->id != chip[i].id; i++)
	{
		if(chip[i].id == UNKNOWN_CHIP)
			return CCDBG_INVALID_ID;
	}

	/**
	 * get debug interface lock status
	 */
	if((value = ccdbg_command(CCDBG_COMMAND_READ_STATUS, 0, 0, 0, 0, ccdbg_retries)) < 0)
		return CCDBG_INVALID_ID;

	if((id->isLocked = (value & CCDBG_STATUS_DEBUG_LOCKED)))
	{
		id->flashSize = 0;
		id->writableFlashSize = 0;
		id->flashBankSize = 0;
		id->flashPageSize = 0;
		id->numberOfFlashPages = 0;
		id->sramSize = 0;
		id->ieeeAddressLength = 0;
		return id;
	}

	/**
	 * verify the returned chip ID
	 */
	if(ccdbg_readMemory(id, REG_CHIPID, 0, 0) != (unsigned int)id->id)
		return CCDBG_INVALID_ID;

	/**
	 * verify the returned chip version
	 */
	if(ccdbg_readMemory(id, REG_CHVER, 0, 0) != (unsigned int)id->rev)
		return CCDBG_INVALID_ID;

	/**
	 * get the size of the chip's flash memory
	 */
	if((value = ccdbg_readMemory(id, REG_CHIPINFO0, 0, 0)) < 0)
		return CCDBG_INVALID_ID;

	value >>= 4;
	id->flashSize = (id->id == CCDBG_CHIP_ID_CC2533 && value == 0x3) ? KB(96) : (KB(16) << value);
	id->writableFlashSize = id->flashSize - FLASH_PAGE_LOCK_BITS_SIZE;

	/**
	 * get the bank size and page size of the chip's flash memory
	 */
	id->flashBankSize = KB(32);
	id->flashPageSize = chip[i].flashPageSize;

	/**
	 * compute number of flash pages
	 */
	id->numberOfFlashPages = (id->flashSize + (id->flashPageSize - 1)) / id->flashPageSize;

	/**
	 * get the size of the chip's SRAM
	 */
	if((value = ccdbg_readMemory(id, REG_CHIPINFO1, 0, 0)) < 0)
		return CCDBG_INVALID_ID;

	id->sramSize = KB(((value & 0x7) + 1));

	/**
	 * if applicable, get the chip's IEEE address
	 */
	switch(id->id)
	{
	case CCDBG_CHIP_ID_CC2530:
	case CCDBG_CHIP_ID_CC2531:
	case CCDBG_CHIP_ID_CC2533:
		id->ieeeAddressLength = 8;
		value = 0x780c;
		break;

	case CCDBG_CHIP_ID_CC2540:
	case CCDBG_CHIP_ID_CC2541:
		id->ieeeAddressLength = 6;
		value = 0x780e;
		break;

	default:
		id->ieeeAddressLength = 0;
		break;
	}

	if(id->ieeeAddressLength > 0)
	{
		if(ccdbg_readMemory(id, (unsigned int)value, id->ieeeAddressLength, id->ieeeAddress) < 0)
			return CCDBG_INVALID_ID;
	}

	return id;
}

int ccdbg_executeInstruction(CCDBG_ID id, unsigned int size, const unsigned char *instruction)
{
	return executeInstruction(size, instruction);
}

int ccdbg_readMemory(CCDBG_ID id, unsigned int address, unsigned int size, unsigned char *data)
{
	unsigned char instruction1[] = { 0x90, (address >> 8) & 0xff, address & 0xff };
	static unsigned char instruction2 = 0xe0;
	static unsigned char instruction3 = 0xa3;
	unsigned char _data;
	int value;
	unsigned int i;

	switch(size)
	{
	case 0:
		size = 1;
		data = &_data;
		break;

	case 1:
		if(data == 0)
			data = &_data;

		break;

	default:
		if(data == 0)
			return -1;

		break;
	}

	/**
	 * MOV DPTR,#data16
	 *   #data16 is address
	 */
	if(executeInstruction(3, instruction1) < 0)
		return -1;

	for(i = 0; ; )
	{
		/**
		 * MOVX A,@DPTR
		 */
		if((value = executeInstruction(1, &instruction2)) < 0)
			return -1;

		data[i] = value;

		if(++i == size)
			break;

		/**
		 * INC DPTR
		 */
		if(executeInstruction(1, &instruction3) < 0)
			return -1;
	}

	return data[0];
}

int ccdbg_writeMemory(CCDBG_ID id, unsigned int address, unsigned int size, const unsigned char *data, int verify)
{
	unsigned char instruction1[] = { 0x90, (address >> 8) & 0xff, address & 0xff };
	unsigned char instruction2[] = { 0x74, 0x00 };
	static unsigned char instruction3 = 0xf0;
	static unsigned char instruction4 = 0xa3;
	unsigned int i;

	/**
	 * MOV DPTR,#data16
	 *   #data16 is address
	 */
	if(executeInstruction(3, instruction1) < 0)
		return -1;

	for(i = 0; ; )
	{
		/**
		 * MOV A,#data
		 *   #data is value
		 */
		instruction2[1] = data[i];

		if(executeInstruction(2, instruction2) < 0)
			return -1;

		/**
		 * MOVX @DPTR,A
		 */
		if(executeInstruction(1, &instruction3) < 0)
			return -1;

		if(++i == size)
			break;

		/**
		 * INC DPTR
		 */
		if(executeInstruction(1, &instruction4) < 0)
			return -1;
	}

	/**
	 * verify
	 */
	if(verify)
	{
		for(i = 0; i < size; i++)
		{
			if(ccdbg_readMemory(id, address, 0, 0) != data[i])
				return -1;

			++address;
		}
	}

	return 0;
}

static unsigned int readFlash(CCDBG_ID id, unsigned int address, unsigned int size, unsigned char *data)
{
	unsigned int bytes = 0;
	unsigned char bank;
	unsigned int bankSize;

	while(bytes < size)
	{
		/**
		 * set the flash bank to access in MEMCTR
		 */
		bank = (unsigned char)(address / id->flashBankSize);

		if(ccdbg_writeMemory(id, REG_MEMCTR, 1, &bank, 1) < 0)
			break;

		/**
		 * compute the size of memory to read from the flash bank
		 */
		bankSize = ((bank + 1) * id->flashBankSize) - address;

		if((bytes + bankSize) > size)
			bankSize = size - bytes;

		/**
		 * read the data from the flash bank
		 */
		if(ccdbg_readMemory(id, REG_XDATA + (address % id->flashBankSize), bankSize, &data[bytes]) < 0)
			break;

		/**
		 * move on to the next flash bank
		 */
		bytes += bankSize;
		address += bankSize;
	}

	return (bytes == size) ? bytes : ~bytes;
}

static int writeFlashPage(CCDBG_ID id, unsigned int page, const unsigned char *data, int eraseFirst)
{
	unsigned char size[2] = { (id->flashPageSize >> 8) & 0xff, id->flashPageSize & 0xff };

	unsigned char descriptorData[] = {
			// source descriptor
			0x62, 0x60,			// source: DBGDATA (0x6260)
			0x00, 0x10,			// destination: SRAM (0x0010)
			size[0], size[1],	// length: flash page size
			31,					// trigger: DBG_BW
			0x11,				// source increment: 0, destination increment: 1, priority: assured

			// destination descriptor
			0x00, 0x10,			// source: SRAM (0x0010)
			0x62, 0x73,			// destination: FWDATA (0x6273)
			size[0], size[1],	// length: flash page size
			18,					// trigger: FLASH
			0x42				// source increment: 1, destination increment: 0, priority: high
	};

	static unsigned char descriptorAddress[] = {
			0x08, 0x00,	// destination descriptor address: 0x0008
			0x00, 0x00	// source descriptor address: 0x0000
	};

	static unsigned char dmaarmValue1 = 0x01;				// arm DMA0
	static unsigned char dmaarmValue2 = 0x02;				// arm DMA1
	static unsigned char fctlValue = FCTL_WRITE | FCTL_CM;	// start flash write
	unsigned char faddrValue[2];							// flash address
	int value;

	if(eraseFirst)
	{
		if(ccdbg_eraseFlashPage(id, page) != 0)
			return -1;
	}
	else
	{
		if(id == CCDBG_INVALID_ID || id->isLocked)
			return -1;

		if(page >= id->numberOfFlashPages)
			return -1;
	}

	/**
	 * enable DMA transfers via the debug configuration register
	 */
	if((value = ccdbg_command(CCDBG_COMMAND_RD_CONFIG, 0, 0, 0, 0, ccdbg_retries)) < 0)
		return -1;

	value &= ~CCDBG_CONFIG_DMA_PAUSED;

	if((value = ccdbg_command(CCDBG_COMMAND_WR_CONFIG, 1, (unsigned char *)&value, 0, 0, ccdbg_retries)) < 0)
		return -1;

	if((value & (CCDBG_STATUS_CHIP_ERASE_BUSY | CCDBG_STATUS_PCON_IDLE | CCDBG_STATUS_PM_ACTIVE | CCDBG_STATUS_DEBUG_LOCKED)))
		return -1;

	/**
	 * write DMA descriptor data to SRAM
	 */
	if(ccdbg_writeMemory(id, 0x0000, 16, descriptorData, 1) < 0)
		return -1;

	/**
	 * write DMA descriptor addresses to DMA0CFG and DMA1CFG
	 */
	if(ccdbg_writeMemory(id, REG_DMA1CFGL, 4, descriptorAddress, 1) < 0)
		return -1;

	/**
	 * write destination address (flash) to FADDR
	 */
	value = (page * id->flashPageSize) >> 2;
	faddrValue[0] = value & 0xff;
	faddrValue[1] = (value >> 8) & 0xff;

	if(ccdbg_writeMemory(id, REG_FADDRL, 2, faddrValue, 1) < 0)
		return -1;

	/**
	 * arm DMA0 for DBGDATA to SRAM data transfer
	 */
	if(ccdbg_writeMemory(id, REG_DMAARM, 1, &dmaarmValue1, 1) < 0)
		return -1;

	/**
	 * write flash data to SRAM via DBGDATA
	 */
	if(ccdbg_command(CCDBG_COMMAND_BURST_WRITE, id->flashPageSize, data, 0, 0, ccdbg_retries) < 0)
		return -1;

	/**
	 * arm DMA1 for SRAM to flash data transfer
	 */
	if(ccdbg_writeMemory(id, REG_DMAARM, 1, &dmaarmValue2, 1) < 0)
		return -1;

	/**
	 * start the DMA transfer to flash memory
	 */
	if(ccdbg_writeMemory(id, REG_FCTL, 1, &fctlValue, 0) < 0)
		return -1;

	/**
	 * wait for the DMA to finish
	 */
	do
	{
		if((value = ccdbg_readMemory(id, REG_FCTL, 0, 0)) < 0)
			return -1;
	}
	while((value & FCTL_BUSY));

	/**
	 * check if operation is successful
	 */
	if((value & (FCTL_ERASE | FCTL_WRITE | FCTL_ABORT | FCTL_FULL)))
		return -1;

	return 0;
}

static unsigned int writeFlash(CCDBG_ID id, unsigned int address, unsigned int size, const unsigned char *data, int verify, int unlock)
{
	int erasePage = 1;
	unsigned int bytes = 0;
	unsigned int dataBytes;
	unsigned int page;
	unsigned int pageAddress;
	unsigned char writeBuffer[MAXIMUM_FLASH_PAGE_SIZE];
	unsigned char readBuffer[MAXIMUM_FLASH_PAGE_SIZE];
	unsigned char *writeData;
	unsigned char value;
	int changed;
	unsigned int i, j, k;

	dataBytes = id->flashPageSize - (address % id->flashPageSize);
	page = address / id->flashPageSize;

	if(size >= id->writableFlashSize)
	{
		if(ccdbg_eraseFlash(id) != 0)
			return ~0;

		erasePage = 0;
	}
	else if(unlock && ccdbg_unlockFlashPages(id, page, ((address + size + id->flashPageSize - 1) / id->flashPageSize) - page) != 0)
		return ~0;

	while(bytes < size)
	{
		pageAddress = page * id->flashPageSize;

		if((bytes + dataBytes) > size)
			dataBytes = size - bytes;

		if(dataBytes != id->flashPageSize)
		{
			if(readFlash(id, pageAddress, id->flashPageSize, writeBuffer) != id->flashPageSize)
				break;

			changed = 0;

			for(i = address % id->flashPageSize, j = bytes, k = 0; k < dataBytes; i++, j++, k++)
			{
				if(writeBuffer[i] != data[j])
				{
					writeBuffer[i] = data[j];
					changed = 1;
				}
			}

			writeData = changed ? writeBuffer : 0;
		}
		else
			writeData = (unsigned char *)&data[bytes];

		if((value = ccdbg_readMemory(id, REG_FCTL, 0, 0)) < 0 || (value & (FCTL_ERASE | FCTL_WRITE | FCTL_FULL | FCTL_BUSY)))
			break;

		if(writeData != 0)
		{
			if(writeFlashPage(id, page, writeData, erasePage) != 0)
				break;

			if(verify)
			{
				for(i = 2; i > 0; i--)
				{
					if(readFlash(id, pageAddress, id->flashPageSize, readBuffer) != id->flashPageSize)
						break;

					for(j = 0; j < id->flashPageSize && readBuffer[j] == writeData[j]; j++);

					if(j == id->flashPageSize)
						break;
				}

				if(i < 1)
					break;
			}
		}

		address += dataBytes;
		bytes += dataBytes;
		dataBytes = id->flashPageSize;
		++page;
	}

	return (bytes == size) ? bytes : ~bytes;
}

int ccdbg_isFlashPageLocked(CCDBG_ID id, unsigned int page)
{
	unsigned char lockBits;

	if(id == CCDBG_INVALID_ID || id->isLocked)
		return -1;

	if(page >= id->numberOfFlashPages)
		return -1;

	if(readFlash(id, id->writableFlashSize + (page / 8), 1, &lockBits) != 1)
		return -1;

	return !(lockBits & (0x1 << (page % 8)));
}

static int lockUnlockFlashPages(CCDBG_ID id, int lock, unsigned int startPage, unsigned int numberOfPages)
{
	int changed = 0;
	unsigned char lockBits[FLASH_PAGE_LOCK_BITS_SIZE];
	int i;
	unsigned char mask;

	if(id == CCDBG_INVALID_ID || id->isLocked || numberOfPages < 1)
		return -1;

	if(startPage >= id->numberOfFlashPages)
		return -1;

	if(readFlash(id, id->writableFlashSize, FLASH_PAGE_LOCK_BITS_SIZE, lockBits) != FLASH_PAGE_LOCK_BITS_SIZE)
		return -1;

	if((startPage + numberOfPages) > id->numberOfFlashPages)
		numberOfPages = id->numberOfFlashPages - startPage;

	for(i = startPage / 8, mask = 0x1 << (startPage % 8); numberOfPages > 0; numberOfPages--)
	{
		if(lock)
		{
			changed |= (lockBits[i] & mask);
			lockBits[i] &= ~mask;
		}
		else
		{
			changed |= !(lockBits[i] & mask);
			lockBits[i] |= mask;
		}

		if((mask <<= 1) == 0)
		{
			++i;
			mask = 0x1;
		}
	}

	if(changed && writeFlash(id, id->writableFlashSize, FLASH_PAGE_LOCK_BITS_SIZE, lockBits, 1, 0) != FLASH_PAGE_LOCK_BITS_SIZE)
		return -1;

	return 0;
}

int ccdbg_lockFlashPages(CCDBG_ID id, unsigned int startPage, unsigned int numberOfPages)
{
	return lockUnlockFlashPages(id, 1, startPage, numberOfPages);
}

int ccdbg_unlockFlashPages(CCDBG_ID id, unsigned int startPage, unsigned int numberOfPages)
{
	return lockUnlockFlashPages(id, 0, startPage, numberOfPages);
}

int ccdbg_readFlashPage(CCDBG_ID id, unsigned int page, unsigned char *data)
{
	if(id == CCDBG_INVALID_ID || id->isLocked)
		return -1;

	if(page >= id->numberOfFlashPages)
		return -1;

	if(readFlash(id, page * id->flashPageSize, id->flashPageSize, data) != id->flashPageSize)
		return -1;

	return 0;
}

int ccdbg_writeFlashPage(CCDBG_ID id, unsigned int page, const unsigned char *data, int verify)
{
	if(id == CCDBG_INVALID_ID || id->isLocked)
		return -1;

	if(page >= id->numberOfFlashPages)
		return -1;

	if(writeFlash(id, page * id->flashPageSize, id->flashPageSize, data, verify, 1) != id->flashPageSize)
		return -1;

	return 0;
}

int ccdbg_eraseFlashPage(CCDBG_ID id, unsigned int page)
{
	static unsigned char fctlValue = FCTL_ERASE | FCTL_CM;
	unsigned char value = (unsigned char)page;

	if(id == CCDBG_INVALID_ID || id->isLocked)
		return -1;

	if(page >= id->numberOfFlashPages)
		return -1;

	if(id->id != CCDBG_CHIP_ID_CC2533)
		value <<= 1;

	if(ccdbg_writeMemory(id, REG_FADDRH, 1, &value, 1) < 0)
		return -1;

	if(ccdbg_writeMemory(id, REG_FCTL, 1, &fctlValue, 0) < 0)
		return -1;

	do
	{
		if(ccdbg_readMemory(id, REG_FCTL, 1, &value) < 0)
			return -1;
	}
	while((value & FCTL_BUSY));

	if((value & (FCTL_ERASE | FCTL_WRITE | FCTL_ABORT | FCTL_FULL)))
		return -1;

	return 0;
}

int ccdbg_readFlash(CCDBG_ID id, unsigned int address, unsigned int size, unsigned char *data)
{
	if(id == CCDBG_INVALID_ID || id->isLocked)
		return -1;

	if(address > id->writableFlashSize)
		return -1;

	if(size < 1)
		return 0;

	if((address + size) >= id->writableFlashSize)
		size = id->writableFlashSize - address;

	return (int)readFlash(id, address, size, data);
}

int ccdbg_writeFlash(CCDBG_ID id, unsigned int address, unsigned int size, const unsigned char *data, int verify)
{
	if(id == CCDBG_INVALID_ID || id->isLocked)
		return -1;

	if(address > id->writableFlashSize)
		return -1;

	if(size < 1)
		return 0;

	if((address + size) >= id->writableFlashSize)
		size = id->writableFlashSize - address;

	return (int)writeFlash(id, address, size, data, verify, 1);
}

int ccdbg_eraseFlash(CCDBG_ID id)
{
	unsigned short status;

	if(id == CCDBG_INVALID_ID)
		return -1;

	if(ccdbg_command(CCDBG_COMMAND_CHIP_ERASE, 0, 0, 0, &status, ccdbg_retries) < 0)
		return -1;

	while((status & CCDBG_STATUS_CHIP_ERASE_BUSY))
	{
		if(ccdbg_command(CCDBG_COMMAND_READ_STATUS, 0, 0, 0, &status, ccdbg_retries) < 0)
			return -1;
	}

	ccdbg_identifyChip(id);
	return (id->isLocked ? -1 : 0);
}

int ccdbg_lock(CCDBG_ID id)
{
	unsigned char lockBits;
	unsigned int address;

	if(id == CCDBG_INVALID_ID)
		return -1;

	if(id->isLocked)
		return 0;

	address = id->flashSize - 1;

	if(readFlash(id, address, 1, &lockBits) != 1)
		return -1;

	lockBits &= 0x7f;
	writeFlash(id, address, 1, &lockBits, 1, 1);
	ccdbg_identifyChip(id);
	return (id->isLocked ? 0 : -1);
}

