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
 * CC debug pins
 * ---
 * RESET	- reset
 * DC		- debug clock
 * DD		- debug data (bidirectional)
 */

#ifndef CCDBG_DEVICE_H_
#define CCDBG_DEVICE_H_

typedef enum {
	CCDBG_PIN_RESET,
	CCDBG_PIN_DC,
	CCDBG_PIN_DD
} CCDBG_PIN;

/**
 * device-dependent; each and every one must be defined as
 *   a macro or a function
 */

/**
 * initialize the device; called at the beginning
 *
 * returns 0 if successful, non-zero otherwise
 */
#ifndef ccdbgDevice_initialize
extern int ccdbgDevice_initialize(void);
#endif

/**
 * clean-up the device; called at the end
 */
#ifndef ccdbgDevice_destroy
extern void ccdbgDevice_destroy(void);
#endif

/**
 * set the pin's state to either high or low
 *
 * pin - the pin
 * high - 0 for low, non-zero for high
 */
#ifndef ccdbgDevice_setPinState
extern void ccdbgDevice_setPinState(CCDBG_PIN pin, int high);
#endif

/**
 * get the pin's state
 *
 * pin - the pin
 *
 * returns 0 if low, non-zero otherwise
 */
#ifndef ccdbgDevice_getPinState
extern int ccdbgDevice_getPinState(CCDBG_PIN pin);
#endif

/**
 * set the pin's direction to either output or input
 *
 * pin - the pin
 * output - 0 for input, non-zero for output
 */
#ifndef ccdbgDevice_setPinDirection
extern void ccdbgDevice_setPinDirection(CCDBG_PIN pin, int output);
#endif

/**
 * delay between debug clock pin's changing of state
 */
#ifndef ccdbgDevice_delay
extern void ccdbgDevice_delay(void);
#endif

/**
 * reset
 */
#define CCDBG_RESET_OUT()	ccdbgDevice_setPinDirection(CCDBG_PIN_RESET, 1)		/* set RESET line as output */
#define CCDBG_RESET_HIGH()	ccdbgDevice_setPinState(CCDBG_PIN_RESET, 1)			/* set RESET line high */
#define CCDBG_RESET_LOW()	ccdbgDevice_setPinState(CCDBG_PIN_RESET, 0)			/* set RESET line low */

/**
 * clock
 */
#define CCDBG_DC_OUT()		ccdbgDevice_setPinDirection(CCDBG_PIN_DC, 1)		/* set DC line as output */
#define CCDBG_DC_HIGH()		ccdbgDevice_setPinState(CCDBG_PIN_DC, 1)			/* set DC line high */
#define CCDBG_DC_LOW()		ccdbgDevice_setPinState(CCDBG_PIN_DC, 0)			/* set DC line low */

/**
 * data
 */
#define CCDBG_DD_OUT()		ccdbgDevice_setPinDirection(CCDBG_PIN_DD, 1)		/* set DD line as output */
#define CCDBG_DD_HIGH()		ccdbgDevice_setPinState(CCDBG_PIN_DD, 1)			/* set DD line high */
#define CCDBG_DD_LOW()		ccdbgDevice_setPinState(CCDBG_PIN_DD, 0)			/* set DD line low */
#define CCDBG_DD_IN()		ccdbgDevice_setPinDirection(CCDBG_PIN_DD, 0)		/* set DD line as input */
#define CCDBG_DD()			ccdbgDevice_getPinState(CCDBG_PIN_DD)				/* read DD line */

/**
 * delay
 */
#define CCDBG_DELAY()		ccdbgDevice_delay()

#endif /* CCDBG_DEVICE_H_ */
