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
 * 04may2014
 */

#include "GPIO.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;

#ifdef GPIO_VERBOSE
#define ERROR(x)	perror(x)
#else
#define ERROR(x)
#endif

#define GPIO_ACTIVATE		GPIO_PATH"export"
#define GPIO_DEACTIVATE		GPIO_PATH"unexport"
#define GPIO_CONTROL		GPIO_PATH"gpio"

enum {
	GPIO_CONTROL_DIRECTION,
	GPIO_CONTROL_ACTIVE_STATE,
	GPIO_CONTROL_INPUT_TRIGGER_EDGE,
	GPIO_CONTROL_STATE,
	GPIO_CONTROL_ITEMS
};

typedef struct {
	GPIO *gpio;
	GPIODelegate *delegate;
	GPIONumber number;
	char numberString[30];
	bool isActive;
	int controlFd[GPIO_CONTROL_ITEMS];
	int controlItems;
	pthread_mutex_t mutex;
	int inputWatcherThreadStatus;
	pthread_t inputWatcherThreadId;
	GPIOInputTriggerEdge inputTriggerEdge;
	pthread_mutex_t inputWatcherMutex;
	pthread_cond_t inputWatcherEnableCond;
	double inputPollingRate;
	bool inputPollingRateChanged;
} GPIOInfo;

#define Info(info)		((GPIOInfo *)data)->info
#define IsActive()		(data != NULL)
#define IsNotActive()	(data == NULL)

static const char *directionControlStrings[] = { "\x1", "\x3 out", "\x2 in", NULL };										// direction: output, input
static const char *activeStateControlStrings[] = { "\x1", "\x1 0", "\x1 1", NULL };											// active state: active high, active low
static const char *inputTriggerEdgeControlStrings[] = { "\x3", "\x4 none", "\x6 rising", "\x7 falling", "\x4 both", NULL };	// input trigger edge: none, rising/positive, falling/negative
static const char *stateControlStrings[] = { "\x1", "\x1 0", "\x1 1", NULL };												// state: low, high
static const char **controlStrings[] = { directionControlStrings, activeStateControlStrings, inputTriggerEdgeControlStrings, stateControlStrings };

/**
 * quickly write data by opening file, writing the data, and immediately
 *   closing the file
 */
static bool quickWrite(const char *file, const char *data, int error)
{
	bool success = true;
	size_t dataLength = strlen(data);
	int fd = open(file, O_WRONLY);

    if(fd == -1)
    {
    	ERROR((string(file) + string(" open")).c_str());
    	return false;
    }

    if(write(fd, data, dataLength) != (ssize_t)dataLength)
    {
    	if(errno == error)
   			return true;

    	ERROR((string(file) + string(" write")).c_str());
    	success = false;
    }

    close(fd);
    return success;
}

/**
 * read control file and return the corresponding table value of the read data
 */
static int getControlValue(int controlFd, const char **table, pthread_mutex_t *mutex)
{
    char buffer[100];
    ssize_t size;
    int value;

	pthread_mutex_lock(mutex);

    if(lseek(controlFd, 0, SEEK_SET) != 0)
    {
    	ERROR("lseek");
    	pthread_mutex_unlock(mutex);
    	return -1;
    }

    if((size = read(controlFd, buffer, 99)) < 1)
    {
    	ERROR("read");
    	pthread_mutex_unlock(mutex);
    	return -1;
    }

    pthread_mutex_unlock(mutex);

    for(value = 1; table[value] != NULL; value++)
    {
    	if(memcmp(buffer, table[value] + 2, (size_t)table[value][0]) == 0)
    		return (value - 1);
    }

    return -1;
}

/**
 * write the corresponding table data of value to control file
 */
static bool setControlValue(int controlFd, const char **table, int value, pthread_mutex_t *mutex)
{
	ssize_t size;

	if(value < 0 || value > (int)table[0][0])
		return false;

	pthread_mutex_lock(mutex);

    if(lseek(controlFd, 0, SEEK_SET) != 0)
    {
    	ERROR("lseek");
    	pthread_mutex_unlock(mutex);
    	return false;
    }

    ++value;
    size = (size_t)table[value][0];

    if(write(controlFd, table[value] + 2, size) != size)
    {
    	ERROR("write");
    	pthread_mutex_unlock(mutex);
    	return false;
    }

    pthread_mutex_unlock(mutex);
    return true;
}

/**
 * set the input trigger edge and notify the input watcher thread of the change
 */
static void changeInputTriggerEdge(GPIOInfo *info, GPIOInputTriggerEdge inputTriggerEdge)
{
	pthread_mutex_lock(&info->inputWatcherMutex);
	info->inputTriggerEdge = inputTriggerEdge;
	pthread_cond_signal(&info->inputWatcherEnableCond);
	pthread_mutex_unlock(&info->inputWatcherMutex);
}

/**
 * clean-up
 */
static void cleanUp(void **data)
{
	GPIOInfo *info = *((GPIOInfo **)data);

	if(info == NULL)
		return;

	if(info->inputWatcherThreadStatus == 0)
	{
		pthread_cancel(info->inputWatcherThreadId);
		pthread_join(info->inputWatcherThreadId, NULL);
		info->inputWatcherThreadStatus = -1;
	}

	pthread_cond_destroy(&info->inputWatcherEnableCond);
	pthread_mutex_destroy(&info->inputWatcherMutex);
	pthread_mutex_destroy(&info->mutex);

	while(info->controlItems-- > 0)
		close(info->controlFd[info->controlItems]);

	if(info->isActive)
		quickWrite(GPIO_DEACTIVATE, info->numberString, 0);

	free(info);
	*data = NULL;
}

/**
 * the input watcher thread which polls for a change in the state of the input,
 *   compares this change if it is wanted by the delegate as indicated by the
 *   input trigger edge value, then notifies the delegate of the trigger event
 */
static void * inputWatcher(GPIOInfo *info)
{
	GPIOState state;
	GPIOState newState;
	GPIOInputTriggerEdge inputTriggerEdge;
	struct timespec delay;
	struct timespec *currentDelay;
	struct timespec remainingDelay;
	double delayDuration;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	while(1)
	{
		pthread_mutex_lock(&info->inputWatcherMutex);

		while(info->inputTriggerEdge == GPIO_INPUT_TRIGGER_EDGE_NONE)
			pthread_cond_wait(&info->inputWatcherEnableCond, &info->inputWatcherMutex);

		pthread_mutex_unlock(&info->inputWatcherMutex);

		state = (GPIOState)getControlValue(info->controlFd[GPIO_CONTROL_STATE], controlStrings[GPIO_CONTROL_STATE], &info->mutex);

		while(info->inputTriggerEdge != GPIO_INPUT_TRIGGER_EDGE_NONE)
		{
			pthread_mutex_lock(&info->inputWatcherMutex);

			if(info->inputPollingRateChanged)
			{
				delayDuration = 1.0 / info->inputPollingRate;
				delay.tv_sec = (time_t)delayDuration;
				delay.tv_nsec = (long int)((delayDuration - (long double)delay.tv_sec) * 1000000000);
				info->inputPollingRateChanged = false;
			}

			pthread_mutex_unlock(&info->inputWatcherMutex);

			currentDelay = &delay;

			while(nanosleep(currentDelay, &remainingDelay) == -1 && errno == EINTR)
				currentDelay = &remainingDelay;

			newState = (GPIOState)getControlValue(info->controlFd[GPIO_CONTROL_STATE], controlStrings[GPIO_CONTROL_STATE], &info->mutex);

			if(newState == (GPIOState)-1)
				continue;

			if(newState != state)
			{
				state = newState;
				inputTriggerEdge = (GPIOInputTriggerEdge)(info->inputTriggerEdge & ((newState == GPIO_STATE_LOW) ? GPIO_INPUT_TRIGGER_EDGE_FALLING : GPIO_INPUT_TRIGGER_EDGE_RISING));

				pthread_mutex_lock(&info->inputWatcherMutex);

				if(inputTriggerEdge != GPIO_INPUT_TRIGGER_EDGE_NONE && info->inputTriggerEdge != GPIO_INPUT_TRIGGER_EDGE_NONE)
					info->delegate->triggered(info->gpio, inputTriggerEdge);

				pthread_mutex_unlock(&info->inputWatcherMutex);
			}
		}
	}

	return NULL;
}

GPIODelegate GPIO::dummyDelegate;

GPIO::GPIO(GPIONumber number, bool force, GPIODirection direction, GPIOState state, GPIOActiveState activeState, GPIOInputTriggerEdge inputTriggerEdge, GPIODelegate &delegate)
{
	if((data = malloc(sizeof(GPIOInfo))) == NULL)
	{
		ERROR("data malloc");
		return;
	}

	do
	{
		static const char *control[] = { "/direction", "/active_low", "/edge", "/value" };
		string controlPath;

		Info(gpio) = this;
		Info(delegate) = &delegate;
		Info(number) = number;
	    sprintf(Info(numberString), "%u", number);
	    Info(isActive) = false;
	    memset(Info(controlFd), -1, sizeof(Info(controlFd)));
	    Info(controlItems) = 0;
	    pthread_mutex_init(&Info(mutex), NULL);
	    Info(inputTriggerEdge) = GPIO_INPUT_TRIGGER_EDGE_NONE;
		Info(inputPollingRate) = 100;
		Info(inputPollingRateChanged) = true;
	    pthread_mutex_init(&Info(inputWatcherMutex), NULL);
	    pthread_cond_init(&Info(inputWatcherEnableCond), NULL);
	    Info(inputWatcherThreadStatus) = pthread_create(&Info(inputWatcherThreadId), NULL, (void * (*)(void *))inputWatcher, data);

	    if(Info(inputWatcherThreadStatus) == -1)
	    {
	    	ERROR("pthread_create");
	    	break;
	    }

		if(!quickWrite(GPIO_ACTIVATE, Info(numberString), force ? EBUSY : 0))
			break;

	    Info(isActive) = true;
	    controlPath = string(GPIO_CONTROL) + string(Info(numberString));

	    for( ; Info(controlItems) < GPIO_CONTROL_ITEMS; Info(controlItems)++)
	    {
	    	Info(controlFd)[Info(controlItems)] = open((controlPath + string(control[Info(controlItems)])).c_str(), O_RDWR);

	    	if(Info(controlFd)[Info(controlItems)] == -1)
	    	{
	    		ERROR((controlPath + string(control[Info(controlItems)]) + string(" open")).c_str());
	    		break;
	    	}
	    }

	    if(Info(controlItems) != GPIO_CONTROL_ITEMS)
	    	break;

	    setDirection(direction);
	    setActiveState(activeState);
	    setInputTriggerEdge(inputTriggerEdge);

	    if(direction == GPIO_DIRECTION_OUTPUT)
	    	setState(state);

	    return;
	}
	while(0);

	cleanUp(&data);
}

GPIO::~GPIO()
{
	cleanUp(&data);
}

bool GPIO::isActive()
{
	return IsActive();
}

GPIONumber GPIO::number()
{
	return (IsActive() ? Info(number) : (GPIONumber)-1);
}

GPIODirection GPIO::direction()
{
	if(IsNotActive())
		return (GPIODirection)-1;

	return (GPIODirection)getControlValue(Info(controlFd)[GPIO_CONTROL_DIRECTION], controlStrings[GPIO_CONTROL_DIRECTION], &Info(mutex));
}

bool GPIO::setDirection(GPIODirection direction)
{
	if(IsNotActive())
		return false;

	changeInputTriggerEdge((GPIOInfo *)data, GPIO_INPUT_TRIGGER_EDGE_NONE);
	return setControlValue(Info(controlFd)[GPIO_CONTROL_DIRECTION], controlStrings[GPIO_CONTROL_DIRECTION], (int)direction, &Info(mutex));
}

GPIOActiveState GPIO::activeState()
{
	if(IsNotActive())
		return (GPIOActiveState)-1;

	return (GPIOActiveState)getControlValue(Info(controlFd)[GPIO_CONTROL_ACTIVE_STATE], controlStrings[GPIO_CONTROL_ACTIVE_STATE], &Info(mutex));
}

bool GPIO::setActiveState(GPIOActiveState activeState)
{
	if(IsNotActive())
		return false;

	return setControlValue(Info(controlFd)[GPIO_CONTROL_ACTIVE_STATE], controlStrings[GPIO_CONTROL_ACTIVE_STATE], (int)activeState, &Info(mutex));
}

GPIOInputTriggerEdge GPIO::inputTriggerEdge()
{
	if(IsNotActive())
		return (GPIOInputTriggerEdge)-1;

	return Info(inputTriggerEdge);
}

bool GPIO::setInputTriggerEdge(GPIOInputTriggerEdge inputTriggerEdge)
{
	if(IsNotActive())
		return false;

	if(direction() != GPIO_DIRECTION_INPUT)
		inputTriggerEdge = GPIO_INPUT_TRIGGER_EDGE_NONE;

	changeInputTriggerEdge((GPIOInfo *)data, inputTriggerEdge);
	return true;
}

GPIOState GPIO::state()
{
	if(IsNotActive())
		return (GPIOState)-1;

	return (GPIOState)getControlValue(Info(controlFd)[GPIO_CONTROL_STATE], controlStrings[GPIO_CONTROL_STATE], &Info(mutex));
}

bool GPIO::setState(GPIOState state)
{
	if(IsNotActive())
		return false;

	return setControlValue(Info(controlFd)[GPIO_CONTROL_STATE], controlStrings[GPIO_CONTROL_STATE], (int)state, &Info(mutex));
}

double GPIO::inputPollingRate()
{
	if(IsNotActive())
		return -1.0;

	return Info(inputPollingRate);
}

bool GPIO::setInputPollingRate(double inputPollingRate)
{
	if(IsNotActive() || inputPollingRate <= 0)
		return false;

	pthread_mutex_lock(&Info(inputWatcherMutex));
	Info(inputPollingRate) = inputPollingRate;
	Info(inputPollingRateChanged) = true;
	pthread_mutex_unlock(&Info(inputWatcherMutex));
	return true;
}
