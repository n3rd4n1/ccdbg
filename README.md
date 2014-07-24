ccdbg
=====

A bit-bang implementation of the protocol used by CC Debugger.

If you, like me, don't have access to a CC Debugger but have access to a board
with at least 3 GPIOs like a Raspberry Pi, `ccdbg` will suffice for your
debugging and flashing needs.

ccdbg.c, ccdbg.h
----------------

The device-independent module and API.

ccdbg-device.h
--------------

The device-dependent interface specifying which functions or macros should be
implemented. Those that need implementation are prefixed with `ccdbgDevice_*`.

ccdbg-rpi.cpp, GPIO.cpp, GPIO.h
-------------------------------

The device-dependent module specifically for Raspberry Pi running Raspbian.
GPIO4, GPIO0, and GPIO1 pins are used as reset, debug clock, and debug data
pins, respectively.

Please refer to [this link](http://pi.gadgetoid.com/pinout) for the pinout.

ccdbg-main.c, intelhex.c, intelhex.h, Makefile
----------------------------------------------

The stand-alone `ccdbg` utility. To build and run the utility in Raspberry Pi,
simply execute `make` and `sudo ./ccdbg`, respectively.

