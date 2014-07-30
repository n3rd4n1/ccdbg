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

#include "ccdbg.h"
#include "GPIO.h"

#define RESET	25	// GPIO4, pin 7
#define DC		23	// GPIO0, pin 3
#define DD		24	// GPIO1, pin 5

static GPIO resetPin(RESET, true);
static GPIO dcPin(DC, true);
static GPIO ddPin(DD, true);
static GPIO *gpio[3] = { &resetPin, &dcPin, &ddPin };

int ccdbgDevice_initialize(void)
{
	return (!resetPin.isActive() || !dcPin.isActive() || !ddPin.isActive()) ? -1 : 0;
}

void ccdbgDevice_destroy(void)
{
	// no operation
}

void ccdbgDevice_setPinState(CCDBG_PIN pin, int high)
{
	gpio[pin]->setState(high ? GPIO_STATE_HIGH : GPIO_STATE_LOW);
}

int ccdbgDevice_getPinState(CCDBG_PIN pin)
{
	return (gpio[pin]->state() == GPIO_STATE_HIGH) ? 1 : 0;
}

void ccdbgDevice_setPinDirection(CCDBG_PIN pin, int output)
{
	gpio[pin]->setDirection(output ? GPIO_DIRECTION_OUTPUT : GPIO_DIRECTION_INPUT);
}

void ccdbgDevice_delay(void)
{
	// no operation
}
