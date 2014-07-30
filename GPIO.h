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
 * a convenience class to interface with Linux's GPIO Sysfs (/sys/class/gpio)
 */

#ifndef GPIO_H_
#define GPIO_H_

/**
 * NOTE:
 *   to print error messages, define GPIO_VERBOSE
 */

/**
 * GPIO sysfs path
 */
#define GPIO_PATH	"/sys/class/gpio/"

/**
 * GPIO number
 */
typedef unsigned int GPIONumber;

/**
 * GPIO direction
 */
typedef enum {
	GPIO_DIRECTION_OUTPUT	= 0,
	GPIO_DIRECTION_INPUT	= 1
} GPIODirection;

/**
 * GPIO active state
 */
typedef enum {
	GPIO_ACTIVE_STATE_HIGH	= 0,
	GPIO_ACTIVE_STATE_LOW	= 1
} GPIOActiveState;

/**
 * GPIO input trigger edge
 */
typedef enum {
	GPIO_INPUT_TRIGGER_EDGE_NONE	= 0,
	GPIO_INPUT_TRIGGER_EDGE_RISING	= 1,
	GPIO_INPUT_TRIGGER_EDGE_FALLING	= 2,
	GPIO_INPUT_TRIGGER_EDGE_BOTH	= 3
} GPIOInputTriggerEdge;

/**
 * GPIO state
 */
typedef enum {
	GPIO_STATE_LOW	= 0,
	GPIO_STATE_HIGH	= 1
} GPIOState;

/**
 * GPIO class
 */
class GPIO;

/**
 * GPIO delegate protocol
 */
class GPIODelegate
{
public:
	virtual ~GPIODelegate() { }
	virtual void triggered(GPIO *gpio, GPIOInputTriggerEdge inputTriggerEdge) { }
};

/**
 * GPIO class
 */
class GPIO
{
public:
	/**
	 * constructor
	 *   activates the GPIO and initializes it to a known state; the GPIO
	 *   should always be checked using isActive() after this to determine
	 *   if it was indeed activated
	 *
	 * number - the GPIO number and identifier
	 * force - whether to force the activation or fail if the GPIO is currently active and being used
	 * direction - initial direction
	 * state - initial state
	 * activeState - initial active state
	 * inputTriggerEdge - initial input trigger edge
	 * delegate - the delegate conforming to the GPIODelegate protocol
	 */
	GPIO(GPIONumber number, bool force = false, GPIODirection direction = GPIO_DIRECTION_OUTPUT, GPIOState state = GPIO_STATE_LOW, GPIOActiveState activeState = GPIO_ACTIVE_STATE_HIGH, GPIOInputTriggerEdge inputTriggerEdge = GPIO_INPUT_TRIGGER_EDGE_NONE, GPIODelegate &delegate = dummyDelegate);

	/**
	 * destructor
	 *   deactivates the GPIO if it was activated earlier, and does clean-up
	 */
	~GPIO();

	/**
	 * get the status of GPIO if it was activated or not
	 *
	 * returns true if it was activated, false if not and thus unusable
	 */
	bool isActive();

	/**
	 * get the GPIO's number which is also its unique identifier
	 *
	 * returns the GPIO's number
	 */
	GPIONumber number();

	/**
	 * get the GPIO's current direction
	 *
	 * returns the GPIO's current direction, or -1 for error
	 */
	GPIODirection direction();

	/**
	 * set the GPIO's direction
	 *
	 * direction - the desired direction
	 *
	 * returns true if successful, false otherwise
	 */
	bool setDirection(GPIODirection direction);

	/**
	 * get the GPIO's current active state
	 *
	 * returns the GPIO's current active state, or -1 for error
	 */
	GPIOActiveState activeState();

	/**
	 * set the GPIO's active state
	 *
	 * activeState - the desired active state
	 *
	 * returns true if successful, false otherwise
	 */
	bool setActiveState(GPIOActiveState activeState);

	/**
	 * get the GPIO's current trigger edge
	 *
	 * returns the GPIO's current trigger edge, or -1 for error
	 */
	GPIOInputTriggerEdge inputTriggerEdge();

	/**
	 * set the GPIO's input trigger edge; has no effect on GPIOs configured as output
	 *
	 * inputTriggerEdge - the desired input trigger edge
	 *
	 * returns true if successful, false otherwise
	 */
	bool setInputTriggerEdge(GPIOInputTriggerEdge inputTriggerEdge);

	/**
	 * get the GPIO's current state
	 *
	 * returns the GPIO's current state, or -1 for error
	 */
	GPIOState state();

	/**
	 * set the GPIO's output state; has no effect on GPIOs configured as input
	 *
	 * state - the desired state
	 *
	 * returns true if successful, false otherwise
	 */
	bool setState(GPIOState state);

	/**
	 * get the GPIO's input polling rate
	 *
	 * returns the GPIO's input polling rate
	 */
	double inputPollingRate();

	/**
	 * set the GPIO's input polling rate in hertz (cycles per second)
	 *
	 * inputPollingRate - the desired input polling rate
	 *
	 * returns true if successful, false otherwise
	 */
	bool setInputPollingRate(double inputPollingRate);

	/**
	 * dummy delegate
	 */
	static GPIODelegate dummyDelegate;

private:
	void *data;
};

#endif /* GPIO_H_ */
