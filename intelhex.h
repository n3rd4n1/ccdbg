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
 * 18jun2014
 */

/**
 * intel hexadecimal object file format (big-endian)
 *
 * "Hexadecimal Object File Format Specification", intel, revision A, January 6, 1988
 */

/**
 * binary file format (little-endian)
 *
 * offset         size (bytes)    description
 * --------------------------------------------------------------------
 * 0              4               EIP address
 * 4              4               CS address
 * 8              4               IP address
 * 12             4               base address of first memory chunk
 * 16             4               size of first memory chunk
 * 20             size0           data of first memory chunk
 * 20 + size0     4               base address of second memory chunk
 * ...
 * o              sizeN           data of last memory chunk
 * o + sizeN      0               EOF
 * --------------------------------------------------------------------
 *
 * note: size of memory chunk ranges from "1" byte to "0x100000000" bytes,
 *       with the latter represented as "0" due to overflow
 */

#ifndef INTELHEX_H_
#define INTELHEX_H_

#include <stdint.h>

/**
 * NOTE:
 *   to print error and warning messages, define INTELHEX_VERBOSE
 */

/**
 * invalid address value used to check EIP, CS, and IP values
 */
#define INTEL_HEX_INVALID_ADDRESS		((uint32_t)-1)

/**
 * hex data
 */
typedef struct IntelHexData {
	struct IntelHexData *next;
	uint32_t size;
	uint8_t *data;
} IntelHexData;

/**
 * hex memory
 */
typedef struct IntelHexMemory {
	struct IntelHexMemory *next;
	uint32_t baseAddress;
	uint32_t size;
	IntelHexData *head;
	IntelHexData *tail;
} IntelHexMemory;

/**
 * hex info
 */
typedef struct {
	uint32_t eip;
	uint32_t cs;
	uint32_t ip;
	IntelHexMemory *memory;
	uint32_t endAddress;
	uint32_t endmostAddress;
} IntelHex;

/**
 * format
 */
enum {
	INTEL_HEX_FORMAT_HEX,
	INTEL_HEX_FORMAT_BIN
};

/**
 * flags
 *
 * note: lower byte of flags is the record length
 */
enum {
	INTEL_HEX_IGNORE_UNKNOWN_RECORD		= 0x80000000,
	INTEL_HEX_32BIT_ADDRESSING			= 0x00800000,
	INTEL_HEX_16BIT_ADDRESSING			= 0x00400000,
	INTEL_HEX_8BIT_ADDRESSING			= 0x00200000
};

#define INTEL_HEX_FLAGS_RECORD_LENGTH(flags)				((uint32_t)flags & 0x000000ff)
#define INTEL_HEX_FLAGS_SET_RECORD_LENGTH(flags, length)	flags = (((uint32_t)flags & ~0x000000ff) | ((uint32_t)length & 0x000000ff))
#define INTEL_HEX_FLAGS_ADDRESSING(flags)					((uint32_t)flags & 0x00ff0000)
#define INTEL_HEX_FLAGS_SET_ADDRESSING(flags, addressing)	flags = (((uint32_t)flags & ~0x00ff0000) | ((uint32_t)addressing & 0x00ff0000))

/**
 * convert intel hexadecimal object file or binary file to intel hexadecimal object file or binary file
 *
 * inputFormat - file format of input file
 * inputFilename - name of input file
 * inputHex - IntelHex input as an alternative to input file
 * outputFormat - file format of output file
 * outputFilename - name of output file
 * outputHex - IntelHex output
 * flags - conversion parameters
 *
 * 0 if successful, non-zero otherwise
 *
 * note: don't forget to destroy outputHex
 */
int intelHex_convert(int inputFormat, const char *inputFilename, const IntelHex *inputHex, int outputFormat, const char *outputFilename, IntelHex *outputHex, uint32_t flags);

/**
 * convert intel hexadecimal object file to binary file
 *
 * inputFilename - name of hex input file
 * inputHex - IntelHex input as an alternative to input file
 * outputFilename - name of bin output file
 * outputHex - IntelHex output
 * flags - conversion parameters
 *
 * 0 if successful, non-zero otherwise
 *
 * note: don't forget to destroy outputHex
 */
int intelHex_hexToBin(const char *inputFilename, const IntelHex *inputHex, const char *outputFilename, IntelHex *outputHex, uint32_t flags);

/**
 * convert binary file to intel hexadecimal object file
 *
 * inputFilename - name of bin input file
 * inputHex - IntelHex input as an alternative to input file
 * outputFilename - name of hex output file
 * outputHex - IntelHex output
 * flags - conversion parameters
 *
 * 0 if successful, non-zero otherwise
 *
 * note: don't forget to destroy outputHex
 */
int intelHex_binToHex(const char *inputFilename, const IntelHex *inputHex, const char *outputFilename, IntelHex *outputHex, uint32_t flags);

/**
 * initialize the hex info structure
 *
 * hex - IntelHex to initialize
 * flags - conversion parameters
 *
 * 0 if successful, non-zero otherwise
 */
int intelHex_initializeHexInfo(IntelHex *hex, uint32_t flags);

/**
 * save buffer data or file data into hex info structure
 *
 * hex - IntelHex to save data into
 * data - buffer data; must not be NULL if file is NULL, NULL otherwise
 * file - file data; must not be NULL if data is NULL, NULL otherwise
 * size - size of data
 * baseAddress - memory base address of data being saved
 *
 * 0 if successful, non-zero otherwise
 */
int intelHex_saveDataToHexInfo(IntelHex *hex, const uint8_t *data, FILE *file, uint64_t size, uint32_t baseAddress);

/**
 * copy data from hex info structure
 *
 * hex - IntelHex to copy data from
 * baseAddress - memory base address of data being copied
 * data - destination buffer
 * file - destination file
 * size - size of data
 *
 * 0 if successful, non-zero otherwise
 */
int intelHex_copyDataFromHexInfo(IntelHex *hex, uint32_t baseAddress, uint8_t *data, FILE *file, uint64_t size);

/**
 * destroy the contents of hex info structure
 *
 * hex - IntelHex to destroy
 */
void intelHex_destroyHexInfo(IntelHex *hex);

#endif /* INTELHEX_H_ */
