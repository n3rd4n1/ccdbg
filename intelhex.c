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
 * 22may2014
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "intelhex.h"

#define PREFIX			"intelhex: "

#ifdef INTELHEX_VERBOSE
#define ERROR(...)		fprintf(stderr, PREFIX "error: " __VA_ARGS__)
#define WARNING(...)	fprintf(stderr, PREFIX "warning: " __VA_ARGS__)
#else
#define ERROR(...)
#define WARNING(...)
#endif

#define MAX_32BIT		0xffffffff
#define MAX_16BIT		0xfffff
#define MAX_8BIT		0xffff

#define MAX_EIP			0xffffffff
#define MAX_CS_IP		0xffff

#define IS_VALID_ADDRESS(address)		(address != INTEL_HEX_INVALID_ADDRESS)

enum {
	INTEL_HEX_RECORD_DATA,
	INTEL_HEX_RECORD_END_OF_FILE,
	INTEL_HEX_RECORD_EXTENDED_SEGMENT_ADDRESS,
	INTEL_HEX_RECORD_START_SEGMENT_ADDRESS,
	INTEL_HEX_RECORD_EXTENDED_LINEAR_ADDRESS,
	INTEL_HEX_RECORD_START_LINEAR_ADDRESS
};

#define DEFAULT_RECORD_LENGTH	16
#define BUFFER_SIZE				1024

/******************************************************************************
 * other helpers
 */

static int openFiles(FILE **inputFile, const char *inputFilename, const char *inputMode, FILE **outputFile, const char *outputFilename, const char *outputMode)
{
	*inputFile = NULL;
	*outputFile = NULL;

	if(inputFilename != NULL && (*inputFile = fopen(inputFilename, inputMode)) == NULL)
	{
		ERROR("failed to open \"%s\" file for reading\n", inputFilename);
		return -1;
	}

	if(outputFilename != NULL && (*outputFile = fopen(outputFilename, outputMode)) == NULL)
	{
		if(*inputFile != NULL)
		{
			fclose(*inputFile);
			*inputFile = NULL;
		}

		ERROR("failed to open \"%s\" file for writing\n", inputFilename);
		return -1;
	}

	return 0;
}

static int checkEip(uint32_t eip)
{
	if(IS_VALID_ADDRESS(eip) && eip > MAX_EIP)
	{
		ERROR("EIP should never be greater than 0x%x\n", MAX_EIP);
		return -1;
	}

	return 0;
}

static int checkCsAndIp(uint32_t cs, uint32_t ip)
{
	int csIsValid = IS_VALID_ADDRESS(cs);
	int ipIsValid = IS_VALID_ADDRESS(ip);

	if((csIsValid ^ ipIsValid))
	{
		ERROR("CS and IP should both be valid addresses at the same time or invalid addresses at the same time\n");
		return -1;
	}

	if(csIsValid && (cs > MAX_CS_IP || ip > MAX_CS_IP))
	{
		ERROR("CS and IP should never be greater than 0x%x\n", MAX_CS_IP);
		return -1;
	}

	return 0;
}

static int copyData(const uint8_t **sourceData, FILE *sourceFile, uint8_t *destinationData, uint32_t size)
{
	if(*sourceData == NULL)
	{
		if(fread(destinationData, 1, size, sourceFile) != size)
		{
			ERROR("failed to read %u bytes from input file\n", size);
			return -1;
		}
	}
	else
	{
		memcpy(destinationData, *sourceData, size);
		*sourceData += size;
	}

	return 0;
}

/******************************************************************************
 * hex info helpers
 */

static void freeHexData(IntelHexData *data)
{
	IntelHexData *temp;

	while((temp = data) != NULL)
	{
		data = data->next;
		free(temp);
	}
}

static inline IntelHexData * copyHexData(const uint8_t *sourceData, FILE *sourceFile, IntelHexData **destinationData, uint64_t size)
{
	IntelHexData *data = NULL;
	IntelHexData *tail = NULL;
	IntelHexData *temp;
	uint32_t chunkSize;

	/**
	 * note: data capacity of IntelHexData structure is always BUFFER_SIZE bytes
	 */

	if(*destinationData != NULL && (chunkSize = BUFFER_SIZE - (*destinationData)->size) > 0)
	{
		if(chunkSize > size)
			chunkSize = size;

		if(copyData(&sourceData, sourceFile, &(*destinationData)->data[(*destinationData)->size], chunkSize) != 0)
			return NULL;

		(*destinationData)->size += chunkSize;
		size -= chunkSize;
	}

	while(size > 0)
	{
		chunkSize = (size > BUFFER_SIZE) ? BUFFER_SIZE : size;

		if((temp = (IntelHexData *)malloc(sizeof(IntelHexData) + BUFFER_SIZE)) == NULL)
		{
			ERROR("failed to allocate memory for IntelHexData structure\n");
			freeHexData(data);
			return NULL;
		}

		temp->next = NULL;
		temp->size = chunkSize;
		temp->data = (uint8_t *)temp + sizeof(IntelHexData);

		if(tail == NULL)
			data = temp;
		else
			tail->next = temp;

		tail = temp;

		if(copyData(&sourceData, sourceFile, tail->data, chunkSize) != 0)
		{
			freeHexData(data);
			return NULL;
		}

		size -= chunkSize;
	}

	if(*destinationData == NULL)
		*destinationData = data;
	else
		(*destinationData)->next = data;

	return (tail == NULL) ? *destinationData : tail;
}

int intelHex_initializeHexInfo(IntelHex *hex, uint32_t flags)
{
	if(hex == NULL)
		return -1;

	hex->eip = INTEL_HEX_INVALID_ADDRESS;
	hex->cs = INTEL_HEX_INVALID_ADDRESS;
	hex->ip = INTEL_HEX_INVALID_ADDRESS;
	hex->memory = NULL;

	switch(INTEL_HEX_FLAGS_ADDRESSING(flags))
	{
	case INTEL_HEX_8BIT_ADDRESSING:
		hex->endmostAddress = MAX_8BIT;
		break;

	case INTEL_HEX_16BIT_ADDRESSING:
		hex->endmostAddress = MAX_16BIT;
		break;

	case INTEL_HEX_32BIT_ADDRESSING:
		hex->endmostAddress = MAX_32BIT;
		break;

	default:
		hex->endmostAddress = MAX_32BIT;
		hex->endAddress = MAX_8BIT;
		return 0;
	}

	hex->endAddress = hex->endmostAddress;
	return 0;
}

static void freeHexMemory(IntelHexMemory *memory)
{
	if(memory->head != NULL)
		freeHexData(memory->head);

	free(memory);
}

void intelHex_destroyHexInfo(IntelHex *hex)
{
	IntelHexMemory *memory;

	while((memory = hex->memory) != NULL)
	{
		hex->memory = hex->memory->next;
		freeHexMemory(memory);
	}

	intelHex_initializeHexInfo(hex, 0);
}

int intelHex_saveDataToHexInfo(IntelHex *hex, const uint8_t *data, FILE *file, uint64_t size, uint32_t baseAddress)
{
	IntelHexMemory *previousMemory = NULL;
	IntelHexMemory *currentMemory = NULL;
	IntelHexMemory *memory;
	IntelHexData *head;
	IntelHexData *tail;
	uint32_t endAddress;

	if(hex == NULL)
	{
		ERROR("hex info structure cannot be NULL\n");
		return -1;
	}

	if((data == NULL && file == NULL) || (data != NULL && file != NULL))
	{
		ERROR("only one of data or file should not be NULL\n");
		return -1;
	}

	if(size < 1)
	{
		ERROR("data size should be at least 1 byte\n");
		return -1;
	}

	if(baseAddress > hex->endmostAddress || (size - 1) > (hex->endmostAddress - baseAddress))
	{
		ERROR("hex memory at 0x%.8x with %llu bytes exceeded the maximum address of 0x%.8x\n", baseAddress, size, hex->endmostAddress);
		return -1;
	}

	endAddress = baseAddress + size - 1;

	if(endAddress > hex->endAddress)
	{
		if(endAddress > MAX_16BIT)
			hex->endAddress = MAX_32BIT;
		else if(endAddress > MAX_8BIT)
			hex->endAddress = MAX_16BIT;
	}

	for(currentMemory = hex->memory; currentMemory != NULL; previousMemory = currentMemory, currentMemory = currentMemory->next)
	{
		if(endAddress >= currentMemory->baseAddress && baseAddress < (currentMemory->baseAddress + currentMemory->size))
		{
			ERROR("hex memory at 0x%.8x ~ 0x%.8x overlapped hex memory at 0x%.8x ~ 0x%.8x\n", baseAddress, endAddress, currentMemory->baseAddress, currentMemory->baseAddress + currentMemory->size - 1);
			return -1;
		}

		if(baseAddress < currentMemory->baseAddress)
			break;
	}

	if(previousMemory != NULL && (previousMemory->baseAddress + previousMemory->size) == baseAddress)
	{
		memory = previousMemory;
		head = previousMemory->tail;
	}
	else
	{
		memory = NULL;
		head = NULL;
	}

	if((tail = copyHexData(data, file, &head, size)) == NULL)
		return -1;

	if(memory != NULL)
	{
		previousMemory->size += size;
		previousMemory->tail = tail;
		head = previousMemory->head;
	}

	if(currentMemory != NULL && (endAddress + 1) == currentMemory->baseAddress)
	{
		if(memory == NULL)
		{
			currentMemory->size += size;
			tail->next = currentMemory->head;
			currentMemory->head = head;
		}
		else
		{
			memory->size += size;
			memory->tail->next = currentMemory->head;
			memory->tail = currentMemory->tail;
			currentMemory->head = NULL;
			previousMemory->next = currentMemory->next;
			freeHexMemory(currentMemory);
		}
	}
	else if(memory == NULL)
	{
		if((memory = (IntelHexMemory *)malloc(sizeof(IntelHexMemory))) == NULL)
		{
			ERROR("failed to allocate memory for IntelHexData structure\n");
			return -1;
		}

		memory->baseAddress = baseAddress;
		memory->size = size;
		memory->head = head;
		memory->tail = tail;

		if(previousMemory == NULL)
		{
			memory->next = hex->memory;
			hex->memory = memory;
		}
		else
		{
			memory->next = previousMemory->next;
			previousMemory->next = memory;
		}
	}

	return 0;
}

int intelHex_copyDataFromHexInfo(IntelHex *hex, uint32_t baseAddress, uint8_t *data, FILE *file, uint64_t size)
{
	IntelHexMemory *memory;
	uint32_t endAddress;

	if(hex == NULL)
	{
		ERROR("hex info structure cannot be NULL\n");
		return -1;
	}

	if(size < 1)
	{
		ERROR("data size should be at least 1 byte\n");
		return -1;
	}

	for(memory = hex->memory; memory != NULL; memory = memory->next)
	{
		endAddress = memory->baseAddress + memory->size - 1;

		if(baseAddress >= memory->baseAddress && baseAddress <= endAddress)
		{
			IntelHexData *hexData;
			uint64_t address;
			uint8_t *sourceData;
			uint32_t copySize;

			if((baseAddress + size - 1) > endAddress)
			{
				ERROR("cannot copy all of the requested memory data\n");
				return -1;
			}

			if(data == NULL && file == NULL)
				return 0;

			address = memory->baseAddress;

			for(hexData = memory->head; ; hexData = hexData->next)
			{
				if((address += hexData->size) > baseAddress)
				{
					copySize = address - baseAddress;
					sourceData = &hexData->data[hexData->size - copySize];

					if(copySize > size)
						copySize = size;

					memcpy(data, sourceData, copySize);
					baseAddress += copySize;
					data += copySize;

					if((size -= copySize) < 1)
						return 0;
				}
			}
		}
	}

	ERROR("requested memory data cannot be located\n");
	return -1;
}

static inline int copyHexInfo(const IntelHex *sourceHex, IntelHex *destinationHex, uint32_t flags)
{
	IntelHexMemory *memory;
	IntelHexData *data;
	uint64_t memorySize;
	uint64_t totalSize;

	intelHex_initializeHexInfo(destinationHex, flags);

	if(checkEip(sourceHex->eip) != 0 || checkCsAndIp(sourceHex->cs, sourceHex->ip) != 0)
		return -1;

	destinationHex->eip = sourceHex->eip;
	destinationHex->cs = sourceHex->cs;
	destinationHex->ip = sourceHex->ip;

	for(memory = sourceHex->memory; memory != NULL; memory = memory->next)
	{
		if(memory->head == NULL)
		{
			ERROR("invalid hex memory information: data=%p\n", memory->head);
			return -1;
		}

		memorySize = (memory->size == 0) ? 0x100000000ULL : memory->size;
		totalSize = 0;

		for(data = memory->head; data != NULL; data = data->next)
		{
			if(data->data == NULL || data->size < 1)
			{
				ERROR("invalid hex memory data information: data=%p size=%u\n", data->data, data->size);
				return -1;
			}

			if(intelHex_saveDataToHexInfo(destinationHex, data->data, NULL, data->size, memory->baseAddress + totalSize) != 0)
				return -1;

			totalSize += data->size;

			if(totalSize > memorySize)
			{
				ERROR("total hex memory data size, %llu bytes, exceeded expected size, %llu bytes\n", totalSize, memorySize);
				return -1;
			}
		}

		if(totalSize != memorySize)
		{
			ERROR("total hex memory data size, %llu bytes, does not match expected size, %llu bytes\n", totalSize, memorySize);
			return -1;
		}
	}

	return 0;
}

/******************************************************************************
 * bin file helpers
 */

static int writeValueToBinFile(uint32_t value, FILE *file)
{
	uint8_t byte;
	int i;

	for(i = 0; i < 4; i++)
	{
		byte = (value >> (i * 8)) & 0xff;

		if(fwrite(&byte, 1, 1, file) != 1)
			return -1;
	}

	return 0;
}

static int readValueFromBinFile(uint32_t *value, FILE *file)
{
	uint8_t byte;
	int i;

	*value = 0;

	for(i = 0; i < 4; i++)
	{
		if(fread(&byte, 1, 1, file) != 1)
			return -1;

		*value |= (byte << (i * 8));
	}

	return 0;
}

static inline int writeHexInfoToBinFile(IntelHex *hex, FILE *file)
{
	IntelHexMemory *memory;
	IntelHexData *data;

	if(writeValueToBinFile(hex->eip, file) != 0)
	{
		ERROR("failed to write EIP info to bin file\n");
		return -1;
	}

	if(writeValueToBinFile(hex->cs, file) != 0)
	{
		ERROR("failed to write CS info to bin file\n");
		return -1;
	}

	if(writeValueToBinFile(hex->ip, file) != 0)
	{
		ERROR("failed to write IP info to bin file\n");
		return -1;
	}

	for(memory = hex->memory; memory != NULL; memory = memory->next)
	{
		if(writeValueToBinFile(memory->baseAddress, file) != 0)
		{
			ERROR("failed to write data base address info to bin file\n");
			return -1;
		}

		if(writeValueToBinFile(memory->size, file) != 0)
		{
			ERROR("failed to write data size info to bin file\n");
			return -1;
		}

		for(data = memory->head; data != NULL; data = data->next)
		{
			if(fwrite(data->data, 1, data->size, file) != data->size)
			{
				ERROR("failed to write %u bytes of data to bin file\n", data->size);
				return -1;
			}
		}
	}

	return 0;
}

static int readHexInfoFromBinFile(FILE *file, IntelHex *hex, uint32_t flags)
{
	uint32_t baseAddress;
	uint32_t size;

	intelHex_initializeHexInfo(hex, flags);

	if(readValueFromBinFile(&hex->eip, file) != 0)
	{
		ERROR("failed to read EIP info from bin file\n");
		return -1;
	}

	if(readValueFromBinFile(&hex->cs, file) != 0)
	{
		ERROR("failed to read CS info from bin file\n");
		return -1;
	}

	if(readValueFromBinFile(&hex->ip, file) != 0)
	{
		ERROR("failed to read IP info from bin file\n");
		return -1;
	}

	if(checkEip(hex->eip) != 0 || checkCsAndIp(hex->cs, hex->ip) != 0)
		return -1;

	while(fgetc(file) != EOF)
	{
		if(fseek(file, -1, SEEK_CUR) != 0)
		{
			ERROR("failed to set the position indicator of bin file\n");
			return -1;
		}

		if(readValueFromBinFile(&baseAddress, file) != 0)
		{
			ERROR("failed to read data base address info from bin file\n");
			return -1;
		}

		if(readValueFromBinFile(&size, file) != 0)
		{
			ERROR("failed to read data size info from bin file\n");
			return -1;
		}

		if(intelHex_saveDataToHexInfo(hex, NULL, file, (size == 0) ? 0x100000000ULL : (uint64_t)size, baseAddress) != 0)
			return -1;
	}

	if(ferror(file))
	{
		ERROR("EOF not found in bin file\n");
		return -1;
	}

	return 0;
}

/******************************************************************************
 * hex file helpers
 */

static int readValueFromHexFile(FILE *file, int size, uint32_t *value)
{
	/**
	 * note: value in file is big-endian
	 */

	int hex;

	*value = 0;

	while(size-- > 0)
	{
		hex = fgetc(file);

		if(!isxdigit(hex))
			return -1;

		if(isdigit(hex))
			hex -= '0';
		else
			hex = tolower(hex) - 'a' + 10;

		*value |= (hex << (4 * size));
	}

	return 0;
}

static int writeDataToHexFile(FILE *file, int size, const uint8_t *data, int *sum)
{
	int temp = 0;
	int i;

	if(sum == NULL)
		sum = &temp;

	for(i = 0; i < size; i++)
	{
		*sum += data[i];

		if(fprintf(file, "%.2x", data[i]) != 2)
			return -1;
	}

	return 0;
}

static int writeValueToHexFile(FILE *file, int size, uint32_t value, int *sum)
{
	uint8_t data[4];
	int i;

	for(i = 0; size-- > 0; i++)
		data[i] = (uint8_t)((value >> (size * 8)) & 0xff);

	return writeDataToHexFile(file, i, data, sum);
}

static int writeHexRecordToHexFile(FILE *file, uint32_t type, uint32_t length, uint32_t offset, const void *data)
{
	/**
	 * |:|cc|oooo|tt|dd..dd|ss|
	 */

	int checksum = 0;

	if(fputc(':', file) != ':')
	{
		ERROR("failed to write record mark to hex file\n");
		return -1;
	}

	if(writeValueToHexFile(file, 1, length, &checksum) != 0)
	{
		ERROR("failed to write record byte count info to hex file\n");
		return -1;
	}

	if(writeValueToHexFile(file, 2, offset, &checksum) != 0)
	{
		ERROR("failed to write record address offset info to hex file\n");
		return -1;
	}

	if(writeValueToHexFile(file, 1, type, &checksum) != 0)
	{
		ERROR("failed to write record type info to hex file\n");
		return -1;
	}

	if(type == INTEL_HEX_RECORD_DATA)
	{
		if(writeDataToHexFile(file, length, (uint8_t *)data, &checksum) != 0)
		{
			ERROR("failed to write record data to hex file\n");
			return -1;
		}
	}
	else if(type != INTEL_HEX_RECORD_END_OF_FILE)
	{
		if(writeValueToHexFile(file, length, *((uint32_t *)data), &checksum) != 0)
		{
			ERROR("failed to write record data to hex file\n");
			return -1;
		}
	}

	if(writeValueToHexFile(file, 1, 0x100 - (checksum & 0xff), NULL) != 0)
	{
		ERROR("failed to write record checksum info to hex file\n");
		return -1;
	}

	if(fputc('\n', file) != '\n')
	{
		ERROR("failed to write record delimiter to hex file\n");
		return -1;
	}

	return 0;
}

static inline int writeHexInfoToHexFile(IntelHex *hex, FILE *file, uint32_t recordLength)
{
	IntelHexMemory *memory;
	IntelHexData *data;
	uint32_t offset;
	uint32_t length;
	uint64_t baseAddress;
	uint64_t endAddress;
	uint64_t memorySize;
	uint32_t size;
	uint32_t address;
	uint32_t type;
	uint8_t buffer[255];
	uint8_t *sourceData;
	uint32_t sourceDataSize;
	uint32_t copySize;
	uint32_t remainingSize;
	uint32_t i;

	if(recordLength == 0)
		recordLength = DEFAULT_RECORD_LENGTH;

	type = (hex->endAddress == MAX_32BIT) ? INTEL_HEX_RECORD_EXTENDED_LINEAR_ADDRESS : INTEL_HEX_RECORD_EXTENDED_SEGMENT_ADDRESS;

	if(IS_VALID_ADDRESS(hex->eip))
	{
		if(writeHexRecordToHexFile(file, INTEL_HEX_RECORD_START_LINEAR_ADDRESS, 4, 0, &hex->eip) != 0)
		{
			ERROR("failed to write start linear address record to hex file\n");
			return -1;
		}
	}

	if(IS_VALID_ADDRESS(hex->cs)) // and IP
	{
		address = (hex->cs << 16) | hex->ip;

		if(writeHexRecordToHexFile(file, INTEL_HEX_RECORD_START_SEGMENT_ADDRESS, 4, 0, &address) != 0)
		{
			ERROR("failed to write start segment address record to hex file\n");
			return -1;
		}
	}

	for(memory = hex->memory; memory != NULL; memory = memory->next)
	{
		baseAddress = memory->baseAddress;
		memorySize = (memory->size == 0) ? 0x100000000ULL : memory->size;
		endAddress = baseAddress + memorySize;
		data = memory->head;
		sourceData = NULL;
		sourceDataSize = 0;

		while(baseAddress < endAddress)
		{
			if(hex->endAddress == MAX_8BIT)
			{
				size = memorySize;
				offset = baseAddress;
			}
			else
			{
				if(hex->endAddress == MAX_32BIT)
				{
					offset = baseAddress & 0xffff;
					address = baseAddress >> 16;
				}
				else
				{
					offset = baseAddress & 0xf;
					address = baseAddress >> 4;
				}

				if(writeHexRecordToHexFile(file, type, 2, 0, &address) != 0)
				{
					ERROR("failed to write extended %s address record to hex file\n", (type == INTEL_HEX_RECORD_EXTENDED_LINEAR_ADDRESS) ? "linear" : "segment");
					return -1;
				}

				if((size = 0x10000 - offset) > memorySize)
					size = memorySize;
			}

			for(i = 0; i < size; i += length)
			{
				length = size - i;

				if(length > recordLength)
					length = recordLength;

				for(remainingSize = length; remainingSize > 0; )
				{
					if(sourceData == NULL)
					{
						sourceData = data->data;
						sourceDataSize = data->size;
						data = data->next;
					}

					copySize = (sourceDataSize > remainingSize) ? remainingSize : sourceDataSize;
					memcpy(&buffer[length - remainingSize], sourceData, copySize);

					if((sourceDataSize -= copySize) < 1)
						sourceData = NULL;
					else
						sourceData += copySize;

					remainingSize -= copySize;
				}

				if(writeHexRecordToHexFile(file, INTEL_HEX_RECORD_DATA, length, offset + i, buffer) != 0)
				{
					ERROR("failed to write data record to hex file\n");
					return -1;
				}
			}

			baseAddress += size;
			memorySize -= size;
		}
	}

	if(writeHexRecordToHexFile(file, INTEL_HEX_RECORD_END_OF_FILE, 0, 0, NULL) != 0)
	{
		ERROR("failed to write end-of-file record to hex file\n");
		return -1;
	}

	return 0;
}

static int readHexInfoFromHexFile(FILE *file, IntelHex *hex, uint32_t flags)
{
	int notFirstRecord = 0;
	int isLinear = 1;
	uint32_t baseAddress = 0x00000000;
	uint32_t recordType = INTEL_HEX_RECORD_DATA;
	uint8_t buffer[255];
	uint32_t byteCount;
	uint32_t offset;
	uint32_t value;
	uint32_t checksum;
	uint32_t address;
	uint64_t size;
	uint8_t *data;
	int hasNewline;
	int i;

	intelHex_initializeHexInfo(hex, flags);

	while(1)
	{
		for(hasNewline = 0; isspace(i = fgetc(file)); hasNewline |= (i == '\r' || i == '\n'));

		if(recordType == INTEL_HEX_RECORD_END_OF_FILE)
		{
			if(i != EOF || ferror(file))
			{
				ERROR("EOF not found in hex file\n");
				return -1;
			}

			return 0;
		}

		if(notFirstRecord && !hasNewline)
		{
			ERROR("record delimiter not found in hex file\n");
			return -1;
		}

		if(i != ':')
		{
			ERROR("record mark not found in hex file\n");
			return -1;
		}

		if(readValueFromHexFile(file, 2, &byteCount) != 0)
		{
			ERROR("failed to read record byte count info from hex file\n");
			return -1;
		}

		if(readValueFromHexFile(file, 4, &offset) != 0)
		{
			ERROR("failed to read record address offset info from hex file\n");
			return -1;
		}

		if(readValueFromHexFile(file, 2, &recordType) != 0)
		{
			ERROR("failed to read record type info from hex file\n");
			return -1;
		}

		checksum = byteCount + (offset & 0xff) + (offset >> 8) + recordType;

		for(i = 0; (uint32_t)i < byteCount; i++)
		{
			if(readValueFromHexFile(file, 2, &value) != 0)
			{
				ERROR("failed to read record data byte from hex file\n");
				return -1;
			}

			buffer[i] = (uint8_t)value;
			checksum += value;
		}

		if(readValueFromHexFile(file, 2, &value) != 0)
		{
			ERROR("failed to read record checksum info from hex file\n");
			return -1;
		}

		if(((checksum + value) & 0xff) != 0)
		{
			ERROR("wrong record checksum\n");
			return -1;
		}

		if(recordType == INTEL_HEX_RECORD_DATA)
		{
			data = buffer;

			while(byteCount > 0)
			{
				if(isLinear)
				{
					address = (baseAddress + offset) & MAX_32BIT;
					size = (uint64_t)(MAX_32BIT - address) + 1ULL;

					if(size > byteCount)
						size = byteCount;
				}
				else
				{
					offset &= 0xffff;
					address = baseAddress + offset;
					size = ((offset + byteCount) > 0x10000) ? (0x10000 - offset) : byteCount;
				}

				if(intelHex_saveDataToHexInfo(hex, data, NULL, size, address) != 0)
					return -1;

				data += size;
				byteCount -= size;
				offset += size;
			}
		}
		else if(recordType == INTEL_HEX_RECORD_END_OF_FILE)
		{
			if(byteCount != 0 || offset != 0x0000)
			{
				ERROR("wrong record info for type 0x%x: byteCount=%u addressOffset=0x%.4x\n", recordType, byteCount, offset);
				return -1;
			}
		}
		else if(recordType == INTEL_HEX_RECORD_EXTENDED_SEGMENT_ADDRESS)
		{
			if(byteCount != 2 || offset != 0x0000)
			{
				ERROR("wrong record info for type 0x%x: byteCount=%u addressOffset=0x%.4x\n", recordType, byteCount, offset);
				return -1;
			}

			baseAddress = (buffer[0] << 8 | buffer[1]) << 4;
			isLinear = 0;
		}
		else if(recordType == INTEL_HEX_RECORD_EXTENDED_LINEAR_ADDRESS)
		{
			if(byteCount != 2 || offset != 0x0000)
			{
				ERROR("wrong record info for type 0x%x: byteCount=%u addressOffset=0x%.4x\n", recordType, byteCount, offset);
				return -1;
			}

			baseAddress = (buffer[0] << 8 | buffer[1]) << 16;
			isLinear = 1;
		}
		else if(recordType == INTEL_HEX_RECORD_START_LINEAR_ADDRESS)
		{
			if(byteCount != 4 || offset != 0x0000)
			{
				ERROR("wrong record info for type 0x%x: byteCount=%u addressOffset=0x%.4x\n", recordType, byteCount, offset);
				return -1;
			}

			if(IS_VALID_ADDRESS(hex->eip))
			{
				ERROR("duplicate record for start linear address (EIP)\n");
				return -1;
			}

			hex->eip = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
		}
		else if(recordType == INTEL_HEX_RECORD_START_SEGMENT_ADDRESS)
		{
			if(byteCount != 4 || offset != 0x0000)
			{
				ERROR("wrong record info for type 0x%x: byteCount=%u addressOffset=0x%.4x\n", recordType, byteCount, offset);
				return -1;
			}

			if(IS_VALID_ADDRESS(hex->cs)) // and IP
			{
				ERROR("duplicate record for start segment address (CS and IP)\n");
				return -1;
			}

			hex->cs = (buffer[0] << 8) | buffer[1];
			hex->ip = (buffer[2] << 8) | buffer[3];
		}
		else if((flags & INTEL_HEX_IGNORE_UNKNOWN_RECORD))
			WARNING("unknown record of type 0x%x\n", recordType);
		else
		{
			ERROR("unknown record of type 0x%x\n", recordType);
			return -1;
		}

		notFirstRecord = 1;
	}

	return -1;
}

/******************************************************************************
 * conversion
 */

int intelHex_convert(int inputFormat, const char *inputFilename, const IntelHex *inputHex, int outputFormat, const char *outputFilename, IntelHex *outputHex, uint32_t flags)
{
	int status = -1;
	FILE *inputFile;
	FILE *outputFile;
	IntelHex hex;

	if((inputFilename == NULL && inputHex == NULL) || (inputFilename != NULL && inputHex != NULL))
	{
		ERROR("at most one of inputFilename and inputHex must be specified\n");
		return -1;
	}

	if(outputFilename == NULL && outputHex == NULL)
	{
		ERROR("at least one of outputFilename and outputHex must be specified\n");
		return -1;
	}

	if(openFiles(&inputFile, inputFilename, (inputFormat == INTEL_HEX_FORMAT_HEX) ? "r" : "rb", &outputFile, outputFilename, (outputFormat == INTEL_HEX_FORMAT_HEX) ? "w" : "wb") != 0)
		return -1;

	if(outputHex == NULL)
		outputHex = &hex;

	if(inputFile == NULL)
		status = copyHexInfo(inputHex, outputHex, flags);
	else
	{
		if(inputFormat == INTEL_HEX_FORMAT_HEX)
			status = readHexInfoFromHexFile(inputFile, outputHex, flags);
		else
			status = readHexInfoFromBinFile(inputFile, outputHex, flags);

		fclose(inputFile);
	}

	if(outputFile != NULL)
	{
		if(status == 0)
		{
			if(outputFormat == INTEL_HEX_FORMAT_HEX)
				status = writeHexInfoToHexFile(outputHex, outputFile, INTEL_HEX_FLAGS_RECORD_LENGTH(flags));
			else
				status = writeHexInfoToBinFile(outputHex, outputFile);
		}

		fclose(outputFile);
	}

	if(status != 0 || outputHex == &hex)
		intelHex_destroyHexInfo(outputHex);

	return status;
}

inline int intelHex_hexToBin(const char *inputFilename, const IntelHex *inputHex, const char *outputFilename, IntelHex *outputHex, uint32_t flags)
{
	return intelHex_convert(INTEL_HEX_FORMAT_HEX, inputFilename, inputHex, INTEL_HEX_FORMAT_BIN, outputFilename, outputHex, flags);
}

inline int intelHex_binToHex(const char *inputFilename, const IntelHex *inputHex, const char *outputFilename, IntelHex *outputHex, uint32_t flags)
{
	return intelHex_convert(INTEL_HEX_FORMAT_BIN, inputFilename, inputHex, INTEL_HEX_FORMAT_HEX, outputFilename, outputHex, flags);
}

#ifdef INTELHEX_STANDALONE

static void usage(const char *name)
{
	ERROR("wrong parameter\n");

	printf(PREFIX "usage:\n"
			"  \n"
			"  %s <input file format: \"-hex\" or \"-bin\"> <input file> <output file format: \"-hex\" or \"-bin\"> <output file> [optional parameters]\n"
			"  \n"
			"  [optional parameters]\n"
			"    -rl<[0 to 255]>, to specify the maximum data record length; 0 to 255 bytes\n"
			"    -ur, to allow unknown record\n"
			"    -ad<[8,16,32]>, to force the addressing\n"
			"  \n",
			name);
}

static int getDecimalValue(const char *data, int maxDigits)
{
	int value = 0;
	int multiplier = 1;
	int i = strlen(data);

	if(i < 1 || i > maxDigits || (i > 1 && *data == '0'))
		return -1;

	while(i-- > 0)
	{
		if(!isdigit(data[i]))
			return -1;

		value += ((data[i] - '0') * multiplier);
		multiplier *= 10;
	}

	return value;
}

int main(int argc, char **argv)
{
	uint32_t flags = 0;
	int inputFormat;
	int outputFormat;
	IntelHex hex;
	IntelHexMemory *memory;
	int value;
	int i;

	if(argc < 5)
	{
		usage(argv[0]);
		return -1;
	}

	if((inputFormat = (strcmp(argv[1], "-hex") == 0) ? INTEL_HEX_FORMAT_HEX : INTEL_HEX_FORMAT_BIN) != INTEL_HEX_FORMAT_HEX && strcmp(argv[1], "-bin") != 0)
	{
		usage(argv[0]);
		return -1;
	}

	if((outputFormat = (strcmp(argv[3], "-hex") == 0) ? INTEL_HEX_FORMAT_HEX : INTEL_HEX_FORMAT_BIN) != INTEL_HEX_FORMAT_HEX && strcmp(argv[3], "-bin") != 0)
	{
		usage(argv[0]);
		return -1;
	}

	for(i = 5; i < argc; i++)
	{
		if(strlen(argv[i]) < 4)
		{
			if(strcmp(argv[i], "-ur") == 0)
				flags |= INTEL_HEX_IGNORE_UNKNOWN_RECORD;
			else
			{
				usage(argv[0]);
				return -1;
			}
		}
		else
		{
			if(strncmp(argv[i], "-rl", 3) == 0)
			{
				value = getDecimalValue(&argv[i][3], 3);

				if(value < 0 || value > 255)
				{
					usage(argv[0]);
					return -1;
				}

				INTEL_HEX_FLAGS_SET_RECORD_LENGTH(flags, value);
			}
			else if(strncmp(argv[i], "-ad", 3) == 0)
			{
				value = getDecimalValue(&argv[i][3], 2);

				switch(value)
				{
				case 8:
					flags |= INTEL_HEX_8BIT_ADDRESSING;
					break;

				case 16:
					flags |= INTEL_HEX_16BIT_ADDRESSING;
					break;

				case 32:
					flags |= INTEL_HEX_32BIT_ADDRESSING;
					break;

				default:
					usage(argv[0]);
					return -1;
				}
			}
			else
			{
				usage(argv[0]);
				return -1;
			}
		}
	}

	printf("converting %s file, \"%s\", to %s file, \"%s\", with parameters:\n"
			"  ignore unknown records: %s\n"
			"  addressing: %s\n"
			"  data record length: %d bytes %s\n"
			"  \n",
				(inputFormat == INTEL_HEX_FORMAT_HEX) ? "hex" : "bin",
				argv[2],
				(outputFormat == INTEL_HEX_FORMAT_HEX) ? "hex" : "bin",
				argv[4],
				((flags & INTEL_HEX_IGNORE_UNKNOWN_RECORD)) ? "YES" : "NO",
				(INTEL_HEX_FLAGS_ADDRESSING(flags) == INTEL_HEX_8BIT_ADDRESSING) ? "8-bit" :
						((INTEL_HEX_FLAGS_ADDRESSING(flags) == INTEL_HEX_16BIT_ADDRESSING) ? "16-bit" :
								((INTEL_HEX_FLAGS_ADDRESSING(flags) == INTEL_HEX_32BIT_ADDRESSING) ? "32-bit" : "auto")),
				(INTEL_HEX_FLAGS_RECORD_LENGTH(flags) == 0) ? DEFAULT_RECORD_LENGTH : INTEL_HEX_FLAGS_RECORD_LENGTH(flags),
						(INTEL_HEX_FLAGS_RECORD_LENGTH(flags) == 0) ? "(default)" : "");

	if(intelHex_convert(inputFormat, argv[2], NULL, outputFormat, argv[4], &hex, flags) != 0)
	{
		printf("conversion failed!\n\n");
		return -1;
	}

	printf("conversion successful!\n\n");

	printf("summary:\n");
	printf("  EIP: 0x%.8x %s\n", hex.eip, IS_VALID_ADDRESS(hex.eip) ? "" : "(unspecified)");
	printf("  CS: 0x%.8x %s\n", hex.cs, IS_VALID_ADDRESS(hex.cs) ? "" : "(unspecified)");
	printf("  IP: 0x%.8x %s\n", hex.ip, IS_VALID_ADDRESS(hex.ip) ? "" : "(unspecified)");

	for(i = 0, memory = hex.memory; memory != NULL; i++, memory = memory->next)
		printf("  mem%d: 0x%.8x ~ 0x%.8x, %u bytes\n", i, memory->baseAddress, memory->baseAddress + memory->size - 1, memory->size);

	printf("\n");

	intelHex_destroyHexInfo(&hex);
	return 0;
}

#endif /* INTELHEX_STANDALONE */
