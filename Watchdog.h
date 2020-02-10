#ifndef Watchdog_h
#define Watchdog_h

#include "Arduino.h"

class Watchdog
{
private:
	static uint8_t _wdto;

	uint8_t DeterminePeriod(unsigned int periodInSeconds);

public:
	static void Watchdog::Configure(bool sleepOrWait = false);

	Watchdog(unsigned int periodInSeconds = 0);

	void Sleep(unsigned long long periodInSeconds);
	void Wait(unsigned long long periodInSeconds);
	void Reset(unsigned int periodInSeconds = 0);
};

#endif
