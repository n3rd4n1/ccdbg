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
 * 17jul2014
 */

#include "ccdbg.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include "intelhex.h"

#define KB(x)	((float)x / 1024.0)

enum {
	UNKNOWN_COMMAND = -1,
	COMMAND_EXECUTE_DEBUG_COMMAND,
	COMMAND_SHOW_CHIP_INFORMATION,
	COMMAND_EXECUTE_INSTRUCTION,
	COMMAND_READ_MEMORY,
	COMMAND_WRITE_MEMORY,
	COMMAND_READ_FLASH_PAGE,
	COMMAND_WRITE_FLASH_PAGE,
	COMMAND_ERASE_FLASH_PAGE,
	COMMAND_CHECK_FLASH_PAGE,
	COMMAND_LOCK_FLASH_PAGES,
	COMMAND_UNLOCK_FLASH_PAGES,
	COMMAND_READ_FLASH,
	COMMAND_WRITE_FLASH,
	COMMAND_ERASE_FLASH,
	COMMAND_LOCK_DEBUG_INTERFACE,
	COMMAND_ITEMS
};

#define EXECUTE_DEBUG_COMMAND	"-ec"
#define SHOW_CHIP_INFORMATION	"-si"
#define EXECUTE_INSTRUCTION		"-ei"
#define READ_MEMORY				"-rm"
#define WRITE_MEMORY			"-wm"
#define READ_FLASH_PAGE			"-rp"
#define WRITE_FLASH_PAGE		"-wp"
#define ERASE_FLASH_PAGE		"-ep"
#define CHECK_FLASH_PAGE		"-cp"
#define LOCK_FLASH_PAGES		"-lp"
#define UNLOCK_FLASH_PAGES		"-up"
#define READ_FLASH				"-rf"
#define WRITE_FLASH				"-wf"
#define ERASE_FLASH				"-ef"
#define LOCK_DEBUG_INTERFACE	"-ld"

static const char *commandList[] = {
		EXECUTE_DEBUG_COMMAND,
		SHOW_CHIP_INFORMATION,
		EXECUTE_INSTRUCTION,
		READ_MEMORY,
		WRITE_MEMORY,
		READ_FLASH_PAGE,
		WRITE_FLASH_PAGE,
		ERASE_FLASH_PAGE,
		CHECK_FLASH_PAGE,
		LOCK_FLASH_PAGES,
		UNLOCK_FLASH_PAGES,
		READ_FLASH,
		WRITE_FLASH,
		ERASE_FLASH,
		LOCK_DEBUG_INTERFACE
};

enum {
	UNKNOWN_DEBUG_COMMAND = -1,
	DEBUG_COMMAND_ERASE_FLASH,
	DEBUG_COMMAND_WRITE_CONFIGURATION,
	DEBUG_COMMAND_READ_CONFIGURATION,
	DEBUG_COMMAND_GET_PC,
	DEBUG_COMMAND_READ_STATUS,
	DEBUG_COMMAND_SET_BREAKPOINT,
	DEBUG_COMMAND_HALT_CPU,
	DEBUG_COMMAND_RESUME_CPU,
	DEBUG_COMMAND_RUN_INSTRUCTION,
	DEBUG_COMMAND_STEP_CPU,
	DEBUG_COMMAND_GET_BM,
	DEBUG_COMMAND_GET_ID,
	DEBUG_COMMAND_BURST_WRITE,
	DEBUG_COMMAND_ITEMS
};

#define DEBUG_ERASE_FLASH			"ec"
#define DEBUG_WRITE_CONFIGURATION	"wc"
#define DEBUG_READ_CONFIGURATION	"rc"
#define DEBUG_GET_PC				"gp"
#define DEBUG_READ_STATUS			"rs"
#define DEBUG_SET_BREAKPOINT		"sb"
#define DEBUG_HALT_CPU				"ho"
#define DEBUG_RESUME_CPU			"ro"
#define DEBUG_RUN_INSTRUCTION		"ri"
#define DEBUG_STEP_CPU				"si"
#define DEBUG_GET_BM				"gb"
#define DEBUG_GET_ID				"gi"
#define DEBUG_BURST_WRITE			"bw"

static struct {
	const char *string;
	CCDBG_COMMAND id;
	unsigned int minimumInputBytes;
	unsigned int maximumInputBytes;
} debugCommandList[] = {
		{ DEBUG_ERASE_FLASH, CCDBG_COMMAND_CHIP_ERASE, 0, 0 },
		{ DEBUG_WRITE_CONFIGURATION, CCDBG_COMMAND_WR_CONFIG, 1, 1 },
		{ DEBUG_READ_CONFIGURATION, CCDBG_COMMAND_RD_CONFIG, 0, 0 },
		{ DEBUG_GET_PC, CCDBG_COMMAND_GET_PC, 0, 0 },
		{ DEBUG_READ_STATUS, CCDBG_COMMAND_READ_STATUS, 0, 0 },
		{ DEBUG_SET_BREAKPOINT, CCDBG_COMMAND_SET_HW_BRKPNT, 3, 3 },
		{ DEBUG_HALT_CPU, CCDBG_COMMAND_HALT, 0, 0 },
		{ DEBUG_RESUME_CPU, CCDBG_COMMAND_RESUME, 0, 0 },
		{ DEBUG_RUN_INSTRUCTION, CCDBG_COMMAND_DEBUG_INSTR, 1, 3 },
		{ DEBUG_STEP_CPU, CCDBG_COMMAND_STEP_INSTR, 0, 0 },
		{ DEBUG_GET_BM, CCDBG_COMMAND_GET_BM, 0, 0 },
		{ DEBUG_GET_ID, CCDBG_COMMAND_GET_CHIP_ID, 0, 0 },
		{ DEBUG_BURST_WRITE, CCDBG_COMMAND_BURST_WRITE, 2, 2049 }
};

static const char *commandHelpList[] = {
				"  "EXECUTE_DEBUG_COMMAND" <command> [input bytes] \n"
						"    commands:\n"
						"      "DEBUG_ERASE_FLASH", erase flash\n"
						"      "DEBUG_WRITE_CONFIGURATION", write debug configuration data\n"
						"      "DEBUG_READ_CONFIGURATION", read debug configuration data\n"
						"      "DEBUG_GET_PC", get value of program counter\n"
						"      "DEBUG_READ_STATUS", read debug status\n"
						"      "DEBUG_SET_BREAKPOINT", set breakpoint\n"
						"      "DEBUG_HALT_CPU", halt CPU operation\n"
						"      "DEBUG_RESUME_CPU", resume CPU operation\n"
						"      "DEBUG_RUN_INSTRUCTION", run debug instruction\n"
						"      "DEBUG_STEP_CPU", step CPU instruction\n"
						"      "DEBUG_GET_BM", get memory bank\n"
						"      "DEBUG_GET_ID", get chip ID\n"
						"      "DEBUG_BURST_WRITE", perform burst write operation\n"
						"    e.g. "EXECUTE_DEBUG_COMMAND" "DEBUG_WRITE_CONFIGURATION" 02\n",
				"  "SHOW_CHIP_INFORMATION"\n",
				"  "EXECUTE_INSTRUCTION" <instruction bytes>\n",
				"  "READ_MEMORY" <address:size> [output]\n"
						"    output:\n"
						"      hex <file>, intel hexadecimal object file format (see intelhex.h)\n"
						"      bin <file>, simple binary file format (see intelhex.h)\n"
						"      raw <file>, data-only binary file\n",
				"  "WRITE_MEMORY" <input> [\"verify\"]\n"
						"    input:\n"
						"      dat <data bytes> <address>\n"
						"      hex <file> [address:size], intel hexadecimal object file format (see intelhex.h)\n"
						"      bin <file> [address:size], simple binary file format (see intelhex.h)\n"
						"      raw <file> <address[:size]> [file offset], data-only binary file\n",
				"  "READ_FLASH_PAGE" <page> [output]\n"
						"    output:\n"
						"      hex <file>, intel hexadecimal object file format (see intelhex.h)\n"
						"      bin <file>, simple binary file format (see intelhex.h)\n"
						"      raw <file>, data-only binary file\n",
				"  "WRITE_FLASH_PAGE" <page> <input> [\"verify\"]\n"
						"    input:\n"
						"      dat <data bytes> <address>\n"
						"      hex <file>, intel hexadecimal object file format (see intelhex.h)\n"
						"      bin <file>, simple binary file format (see intelhex.h)\n"
						"      raw <file> [file offset], data-only binary file\n",
				"  "ERASE_FLASH_PAGE" <page>\n",
				"  "CHECK_FLASH_PAGE" <page>\n",
				"  "LOCK_FLASH_PAGES" <page> [items]\n",
				"  "UNLOCK_FLASH_PAGES" <page> [items]\n",
				"  "READ_FLASH" <address:size> [output]\n"
						"    output:\n"
						"      hex <file>, intel hexadecimal object file format (see intelhex.h)\n"
						"      bin <file>, simple binary file format (see intelhex.h)\n"
						"      raw <file>, data-only binary file\n",
				"  "WRITE_FLASH" <input> [\"verify\"]\n"
						"    input:\n"
						"      dat <data bytes> <address>\n"
						"      hex <file> [address:size], intel hexadecimal object file format (see intelhex.h)\n"
						"      bin <file> [address:size], simple binary file format (see intelhex.h)\n"
						"      raw <file> <address[:size]> [file offset], data-only binary file\n",
				"  "ERASE_FLASH"\n",
				"  "LOCK_DEBUG_INTERFACE"\n"
};

static void printBytes(unsigned int address, unsigned int size, const unsigned char *data)
{
	unsigned int currentAddress = address & ~0xf;
	unsigned int i;

	printf("%u bytes of data at 0x%.8x\n\n"
			"         00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n"
			"--------------------------------------------------------\n",
			size, address);

	for(i = 0; i < size; )
	{
		printf("%.8x ", currentAddress);

		do
		{
			if(currentAddress < address || i >= size)
				printf(".. ");
			else
				printf("%.2x ", data[i++]);
		}
		while((++currentAddress % 16) != 0);

		printf("\n");
	}

	printf("--------------------------------------------------------\n");
}

static int hexToDec(char hex)
{
	if(isdigit(hex))
		return (hex - '0');

	if(isxdigit(hex))
		return (tolower(hex) - 'a' + 10);

	return -1;
}

static int stringToNumber(const char *string, unsigned int *number, const char *delimiter)
{
	int isHex = (string[0] == '0' && tolower(string[1]) == 'x');
	uint64_t num = 0;
	int index = 0;
	int i;

	if(isHex)
		for(i = 2; isxdigit(string[i]); i++);
	else
		for(i = 0; isdigit(string[i]); i++);

	while(*delimiter != '\0' && *delimiter != string[i])
		++delimiter;

	if(*delimiter != string[i])
		return -1;

	if(*delimiter != '\0')
		index = i + 1;

	if(string[0] == '0' && tolower(string[1]) == 'x')
	{
		int shift = 0;
		int dec;

		if(i > 10)
			return -1;

		while(i-- > 2)
		{
			dec = hexToDec(string[i]);

			if(dec < 0)
				return -1;

			num |= ((uint64_t)dec << shift);

			if(num > (uint64_t)((unsigned int)-1))
				return -1;

			shift += 4;
		}
	}
	else
	{
		uint64_t multiplier = 1;

		while(i-- > 0)
		{
			if(isdigit(string[i]))
				num += ((uint64_t)(string[i] - '0') * multiplier);
			else
				return -1;

			if(num > (uint64_t)((unsigned int)-1))
				return -1;

			multiplier *= 10;
		}
	}

	*number = (unsigned int)num;
	return index;
}

static int hexStringToBytes(unsigned char **buffer, const char *hexString)
{
	unsigned int length = strlen(hexString);
	unsigned int size;
	int shift;
	int dec;
	unsigned int i;

	if((length & 0x1) != 0)
		return -1;

	size = length / 2;

	if((*buffer = (unsigned char *)calloc(size, 1)) == NULL)
		return -1;

	for(i = 0, shift = 4; i < length; i++)
	{
		if((dec = hexToDec(hexString[i])) < 0)
			return -1;

		(*buffer)[i / 2] |= dec << shift;
		shift ^= 4;
	}

	return size;
}

static int parseReadArgs(int argc, char **argv, unsigned int *address, unsigned int *size, unsigned int *page, int *fileFormat, IntelHex *intelHex, FILE **file, unsigned char **buffer)
{
	long fileSize;
	int i;

	if(argc != 3 && argc != 5)
		return -1;

	if(page == NULL)
	{
		if((i = stringToNumber(argv[2], address, ":")) < 1)
			return -1;

		if(stringToNumber(&argv[2][i], size, "") != 0 || *size < 1)
			return -1;
	}
	else
	{
		if(stringToNumber(argv[2], page, "") != 0)
			return -1;

		*address = *page * *size;
	}

	if(argc == 5)
	{
		if(strcmp(argv[3], "raw") == 0)
		{
			if((*file = fopen(argv[4], "ab")) == NULL)
				return -1;
		}
		else
		{
			if(strcmp(argv[3], "hex") == 0)
				*fileFormat = INTEL_HEX_FORMAT_HEX;
			else if(strcmp(argv[3], "bin") == 0)
				*fileFormat = INTEL_HEX_FORMAT_BIN;
			else
				return -1;

			if((*file = fopen(argv[4], "rb")) != NULL)
			{
				if(fseek(*file, 0, SEEK_END) != 0)
					return -1;

				if((fileSize = ftell(*file)) < 0)
					return -1;

				fclose(*file);
				*file = NULL;

				if(fileSize > 0)
				{
					if(intelHex_convert(*fileFormat, argv[4], NULL, 0, NULL, intelHex, INTEL_HEX_IGNORE_UNKNOWN_RECORD) != 0)
						return -1;
				}
			}
		}
	}

	if((*buffer = (unsigned char *)malloc(*size)) == NULL)
		return -1;

	return 0;
}

static void saveReadData(int argc, char **argv, unsigned int address, unsigned int size, int fileFormat, IntelHex *intelHex, FILE *file, const unsigned char *buffer)
{
	int okay;

	if(argc == 3)
		printBytes(address, size, buffer);
	else
	{
		if(file == NULL)
		{
			if((okay = (intelHex_saveDataToHexInfo(intelHex, buffer, NULL, size, address) == 0)))
				okay = (intelHex_convert(0, NULL, intelHex, fileFormat, argv[4], NULL, INTEL_HEX_IGNORE_UNKNOWN_RECORD) == 0);
		}
		else
			okay = (fwrite(buffer, 1, size, file) == size);

		printf("%s %u bytes of data at 0x%.8x to \"%s\"", okay ? "saved" : "FAILED to save", size, address, argv[4]);
	}
}

static int parseWriteArgs(int argc, char **argv, unsigned int *address, unsigned int *size, unsigned int *page, int *verify, IntelHex *intelHex, FILE **file, unsigned char **buffer, IntelHexMemory **intelHexMemory)
{
	long fileSize;
	int fileFormat;
	unsigned int offset;
	int arg;
	int i;

	if((*verify = (strcmp(argv[argc - 1], "verify") == 0)))
		--argc;

	if(page == NULL)
	{
		if(argc < 4)
			return -1;

		arg = 2;
	}
	else
	{
		if(argc < 5)
			return -1;

		if(stringToNumber(argv[2], page, "") != 0)
			return -1;

		*address = *page * *size;
		arg = 3;
	}

	if(strcmp(argv[arg], "dat") == 0)
	{
		if(argc != 5)
			return -1;

		if((i = hexStringToBytes(buffer, argv[arg + 1])) < 1)
			return -1;

		if(page == NULL)
		{
			if(stringToNumber(argv[arg + 2], address, "") != 0)
				return -1;

			*size = (unsigned int)i;
		}
		else if((unsigned int)i != *size)
			return -1;
	}
	else if(strcmp(argv[arg], "raw") == 0)
	{
		if(argc < 5 || argc > 6)
			return -1;

		if((*file = fopen(argv[arg + 1], "rb")) == NULL)
			return -1;

		if(fseek(*file, 0, SEEK_END) != 0)
			return -1;

		if((fileSize = ftell(*file)) < 1)
			return -1;

		if(argc == 6)
		{
			if(stringToNumber(argv[arg + 2 + (page == NULL)], &offset, "") != 0)
				return -1;

			if((long)offset >= fileSize)
				return -1;
		}
		else
			offset = 0;

		if(page == NULL)
		{
			if((i = stringToNumber(argv[arg + 2], address, ":")) < 0)
				return -1;

			if(i > 0)
			{
				if(stringToNumber(&argv[arg + 2][i], size, "") != 0 || *size < 1)
					return -1;
			}
			else
				*size = fileSize - offset;
		}

		if(*size > (fileSize - offset))
			return -1;

		if(fseek(*file, offset, SEEK_SET) != 0)
			return -1;

		if((*buffer = (unsigned char *)malloc(*size)) == NULL)
			return -1;

		if(fread(*buffer, 1, *size, *file) != *size)
			return -1;
	}
	else
	{
		if(strcmp(argv[arg], "hex") == 0)
			fileFormat = INTEL_HEX_FORMAT_HEX;
		else if(strcmp(argv[arg], "bin") == 0)
			fileFormat = INTEL_HEX_FORMAT_BIN;
		else
			return -1;

		if(page == NULL)
		{
			if(argc > 4)
			{
				if(argc != 5)
					return -1;

				if((i = stringToNumber(argv[arg + 2], address, ":")) < 1)
					return -1;

				if(stringToNumber(&argv[arg + 2][i], size, "") != 0 || *size < 1)
					return -1;
			}
		}
		else if(argc != 5)
			return -1;

		if(intelHex_convert(fileFormat, argv[arg + 1], NULL, 0, NULL, intelHex, INTEL_HEX_IGNORE_UNKNOWN_RECORD) != 0)
			return -1;

		if(argc != 5)
		{
			*address = intelHex->memory->baseAddress;
			*size = intelHex->memory->size;
			*intelHexMemory = intelHex->memory->next;
		}

		if((*buffer = (unsigned char *)malloc(*size)) == NULL)
			return -1;

		if(intelHex_copyDataFromHexInfo(intelHex, *address, *buffer, NULL, *size) != 0)
			return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int okay = 0;
	unsigned char *buffer = NULL;
	FILE *file = NULL;
	IntelHexMemory *intelHexMemory = NULL;
	IntelHex intelHex;
	CCDBG_INFO info;
	unsigned int address;
	unsigned int size;
	unsigned int page;
	unsigned int count;
	int fileFormat;
	int verify;
	int command;
	int debugCommand;
	int result;

	if(argc < 2 || *argv[1] != '-' || strlen(argv[1]) != 3)
		command = UNKNOWN_COMMAND;
	else
		for(command = COMMAND_ITEMS; --command > UNKNOWN_COMMAND && memcmp(argv[1], commandList[command], 3) != 0; );

	if(command == UNKNOWN_COMMAND)
	{
		printf("\n"
				"  %s <command> [args]\n"
				"    "EXECUTE_DEBUG_COMMAND", execute debug command\n"
				"    "SHOW_CHIP_INFORMATION", show chip information\n"
				"    "EXECUTE_INSTRUCTION", execute instruction\n"
				"    "READ_MEMORY", read memory\n"
				"    "WRITE_MEMORY", write memory\n"
				"    "READ_FLASH_PAGE", read flash page\n"
				"    "WRITE_FLASH_PAGE", write flash page\n"
				"    "ERASE_FLASH_PAGE", erase flash page\n"
				"    "CHECK_FLASH_PAGE", check if flash page is locked\n"
				"    "LOCK_FLASH_PAGES", lock flash pages\n"
				"    "UNLOCK_FLASH_PAGES", unlock flash pages\n"
				"    "READ_FLASH", read flash\n"
				"    "WRITE_FLASH", write flash\n"
				"    "ERASE_FLASH", erase flash\n"
				"    "LOCK_DEBUG_INTERFACE", lock debug interface\n"
				"\n",
				argv[0]);

		return -1;
	}

	intelHex_initializeHexInfo(&intelHex, 0);

	printf("\n");

	do
	{
		if(ccdbgDevice_initialize() != 0)
		{
			printf("FAILED to initialize the ccdbg device\n");
			break;
		}

		if(ccdbg_identifyChip(&info) == CCDBG_INVALID_ID)
		{
			printf("FAILED to identify the chip\n");
			break;
		}

		if(info.isLocked)
		{
			if(command != COMMAND_SHOW_CHIP_INFORMATION && command != COMMAND_ERASE_FLASH)
			{
				printf("FAILED: chip is LOCKED -- only \""SHOW_CHIP_INFORMATION"\" and \""ERASE_FLASH"\" commands are available\n");
				break;
			}
		}

		switch(command)
		{
		case COMMAND_EXECUTE_DEBUG_COMMAND:

			if(argc < 3 || argc > 4 || strlen(argv[2]) != 2)
				break;

			for(debugCommand = DEBUG_COMMAND_ITEMS; --debugCommand > UNKNOWN_DEBUG_COMMAND && memcmp(argv[2], debugCommandList[debugCommand].string, 2) != 0; );

			if(debugCommand == UNKNOWN_DEBUG_COMMAND)
				break;

			if(debugCommandList[debugCommand].minimumInputBytes > 0)
			{
				if(argc != 4)
					break;

				if((int)(size = hexStringToBytes(&buffer, argv[3])) < 1)
					break;

				if(size < debugCommandList[debugCommand].minimumInputBytes || size > debugCommandList[debugCommand].maximumInputBytes)
					break;
			}
			else
			{
				if(argc != 3)
					break;

				size = 0;
			}

			printf("executing debug command...\n");

			result = ccdbg_command(debugCommandList[debugCommand].id, size, buffer, &size, NULL, 1);
			okay = (result >= 0);

			printf("\n>> ");

			if(okay)
			{
				printf("debug command output is %.2x", ((unsigned char *)&result)[0]);

				if(size > 1)
					printf(" %.2x", ((unsigned char *)&result)[1]);

				printf("\n");
			}
			else
				printf("FAILED\n");

			goto done;

		case COMMAND_SHOW_CHIP_INFORMATION:

			if(argc != 2)
				break;

			printf("  chip info:\n"
					"    id: 0x%.2x\n"
					"    revision: 0x%.2x\n"
					"    flash size: %u bytes (%.1fKB)\n"
					"    writable flash size: %u bytes (%.1fKB)\n"
					"    flash bank size: %u bytes (%.1fKB)\n"
					"    flash page size: %u bytes (%.1fKB)\n"
					"    number of flash pages: %u\n"
					"    SRAM size: %u bytes (%.1fKB)\n"
					"    locked: %s\n"
					"    IEEE address: ",
					info.id,
					info.rev,
					info.flashSize, KB(info.flashSize),
					info.writableFlashSize, KB(info.writableFlashSize),
					info.flashBankSize, KB(info.flashBankSize),
					info.flashPageSize, KB(info.flashPageSize),
					info.numberOfFlashPages,
					info.sramSize, KB(info.sramSize),
					info.isLocked ? "yes" : "no");

			if(info.ieeeAddressLength == 0)
				printf("n/a\n");
			else
			{
				int i;

				for(i = (int)info.ieeeAddressLength; i-- > 0; )
					printf("%.2x", info.ieeeAddress[i]);

				printf("\n");
			}

			okay = 1;
			goto done;

		case COMMAND_EXECUTE_INSTRUCTION:

			if(argc != 3)
				break;

			if((int)(size = hexStringToBytes(&buffer, argv[2])) < 1)
				break;

			printf("executing instruction...\n"
					"  code: %s\n", argv[2]);

			result = ccdbg_executeInstruction(&info, size, buffer);
			okay = (result >= 0);

			printf("\n>> ");

			if(okay)
				printf("resulting accumulator register value is 0x%.2x\n", result);
			else
				printf("FAILED\n");

			goto done;

		case COMMAND_ERASE_FLASH_PAGE:

			if(argc != 3)
				break;

			if(stringToNumber(argv[2], &page, "") != 0)
				break;

			printf("erasing flash page %d...\n", page);

			result = ccdbg_eraseFlashPage(&info, page);
			okay = (result == 0);

			printf("\n>> %s\n", okay ? "OK" : "FAILED");

			goto done;

		case COMMAND_CHECK_FLASH_PAGE:

			if(argc != 3)
				break;

			if(stringToNumber(argv[2], &page, "") != 0)
				break;

			printf("checking if flash page is locked...\n"
					"  page: %d\n",
					page);

			result = ccdbg_isFlashPageLocked(&info, page);
			okay = (result >= 0);

			printf("\n>> ");

			if(okay)
				printf("flash page is %s\n", (result == 0) ? "NOT LOCKED" : "LOCKED");
			else
				printf("FAILED\n");

			goto done;

		case COMMAND_LOCK_FLASH_PAGES:
		case COMMAND_UNLOCK_FLASH_PAGES:

			if(argc < 3 || argc > 4)
				break;

			if(stringToNumber(argv[2], &page, "") != 0)
				break;

			if(argc == 4)
			{
				if(stringToNumber(argv[3], &count, "") != 0 || count < 1)
					break;
			}
			else
				count = 1;

			printf("%slocking flash pages...\n"
					"  start page: %u\n"
					"  items: %u\n",
					(command == COMMAND_LOCK_FLASH_PAGES) ? "" : "un", page, count);

			if(command == COMMAND_LOCK_FLASH_PAGES)
				result = ccdbg_lockFlashPages(&info, page, count);
			else
				result = ccdbg_unlockFlashPages(&info, page, count);

			okay = (result == 0);

			printf("\n>> %s\n", okay ? "OK" : "FAILED");

			goto done;

		case COMMAND_ERASE_FLASH:

			if(argc != 2)
				break;

			printf("erasing flash...\n");

			result = ccdbg_eraseFlash(&info);
			okay = (result == 0);

			printf("\n>> %s\n", okay ? "OK" : "FAILED");

			goto done;

		case COMMAND_LOCK_DEBUG_INTERFACE:

			if(argc != 2)
				break;

			printf("locking debug interface...\n");

			result = ccdbg_lock(&info);
			okay = (result == 0);

			printf("\n>> %s\n", okay ? "OK" : "FAILED");

			goto done;

		case COMMAND_READ_MEMORY:
		case COMMAND_READ_FLASH:

			if(parseReadArgs(argc, argv, &address, &size, NULL, &fileFormat, &intelHex, &file, &buffer) != 0)
				break;

			printf("reading %s...\n"
					"  address: 0x%.8x\n"
					"  size: %u\n",
					(command == COMMAND_READ_MEMORY) ? "memory" : "flash", address, size);

			if(command == COMMAND_READ_MEMORY)
				result = ccdbg_readMemory(&info, address, size, buffer);
			else
				result = ccdbg_readFlash(&info, address, size, buffer);

			okay = (result > 0);

			printf("\n>> ");

			if(okay)
				saveReadData(argc, argv, address, result, fileFormat, &intelHex, file, buffer);
			else
				printf("FAILED\n");

			goto done;

		case COMMAND_READ_FLASH_PAGE:

			size = info.flashPageSize;

			if(parseReadArgs(argc, argv, &address, &size, &page, &fileFormat, &intelHex, &file, &buffer) != 0)
				break;

			printf("reading flash page...\n"
					"  page: %d\n"
					"  address: 0x%.8x\n"
					"  size: %u\n",
					page, address, size);

			result = ccdbg_readFlashPage(&info, page, buffer);
			okay = (result == 0);

			printf("\n>> ");

			if(okay)
				saveReadData(argc, argv, address, size, fileFormat, &intelHex, file, buffer);
			else
				printf("FAILED\n");

			goto done;

		case COMMAND_WRITE_MEMORY:
		case COMMAND_WRITE_FLASH:

			if(parseWriteArgs(argc, argv, &address, &size, NULL, &verify, &intelHex, &file, &buffer, &intelHexMemory) != 0)
				break;

			while(1)
			{
				printf("writing %s...\n"
						"  address: 0x%.8x\n"
						"  size: %u\n"
						"  verify: %d\n",
						(command == COMMAND_WRITE_MEMORY) ? "memory" : "flash",
						address, size, verify);

				if(command == COMMAND_WRITE_MEMORY)
					result = ccdbg_writeMemory(&info, address, size, buffer, verify);
				else
					result = ccdbg_writeFlash(&info, address, size, buffer, verify);

				okay = (result > 0);

				printf("\n>> ");

				if(okay)
					printf("%d bytes written\n", result);
				else
				{
					printf("FAILED\n");
					break;
				}

				if(intelHexMemory == NULL)
					break;

				printf("\n");

				okay = 0;
				address = intelHexMemory->baseAddress;
				intelHexMemory = intelHexMemory->next;

				if(size > intelHexMemory->size)
				{
					free(buffer);

					if((buffer = (unsigned char *)malloc(size)) == NULL)
						break;
				}

				size = intelHexMemory->size;

				if(intelHex_copyDataFromHexInfo(&intelHex, address, buffer, NULL, size) != 0)
					break;
			}

			goto done;

		case COMMAND_WRITE_FLASH_PAGE:

			size = info.flashPageSize;

			if(parseWriteArgs(argc, argv, &address, &size, &page, &verify, &intelHex, &file, &buffer, NULL) != 0)
				break;

			printf("writing flash page...\n"
					"  page: %d\n"
					"  address: 0x%.8x\n"
					"  size: %u\n"
					"  verify: %d\n",
					page, address, size, verify);

			result = ccdbg_writeFlashPage(&info, page, buffer, verify);
			okay = (result == 0);

			printf("\n>> %s\n", okay ? "OK" : "FAILED");

			goto done;

		default:
			break;
		}

		printf("%s", commandHelpList[command]);
	}
	while(0);

	done:

	printf("\n");

	if(buffer != NULL)
		free(buffer);

	if(file != NULL)
		fclose(file);

	intelHex_destroyHexInfo(&intelHex);
	ccdbgDevice_destroy();

	return (okay ? 0 : -1);
}
