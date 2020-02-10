#include "Watchdog.h"

#include <avr/wdt.h>            // library for default watchdog functions
#include <avr/interrupt.h>      // library for interrupts handling
#include <avr/sleep.h>          // library for sleep
#include <avr/power.h>          // library for power control

static uint8_t Watchdog::_wdto = WDTO_8S;
volatile unsigned long long _sleepOrWaitCount = 0;
volatile unsigned long long _waitTotal = 1;

uint8_t Watchdog::DeterminePeriod(unsigned int periodInSeconds) {
	uint8_t wdto = WDTO_1S;

	// Note the order of these if statements from highest to lowest  is 
	// important so that control flow cascades down to the right value based
	// on its position in the range of discrete timeouts.
	if ((periodInSeconds >= 8) || (periodInSeconds == 0)) {
		wdto = WDTO_8S;
	}
	else if (periodInSeconds >= 4) {
		wdto = WDTO_4S;
	}
	else if (periodInSeconds >= 2) {
		wdto = WDTO_2S;
	}

	return wdto;
}

// function to configure the watchdog: let it sleep n seconds before firing
// when firing, configure it for resuming program execution
static void Watchdog::Configure(bool sleepOrWait = false)
{
	uint8_t wdps = (_wdto & 0x08 ? _BV(WDP3) : 0)
		| (_wdto & 0x04 ? _BV(WDP2) : 0)
		| (_wdto & 0x02 ? _BV(WDP1) : 0)
		| (_wdto & 0x01 ? _BV(WDP0) : 0)
		| _BV(WDE)
		| (sleepOrWait ? _BV(WDIE) : 0);

	noInterrupts();

	// The MCU Status Register (MCUSR) is used to tell the cause of the last
	// reset, such as brown-out reset, watchdog reset, etc.
	// NOTE: for security reasons, there is a timed sequence for clearing the
	// WDE and changing the time-out configuration. If you don't use this
	// sequence properly, you'll get unexpected results.

	// Clear the reset flag on the MCUSR, the WDRF bit (bit 3).
	MCUSR &= ~_BV(WDRF);

	// Configure the Watchdog timer Control Register (WDTCSR)
	// The WDTCSR is used for configuring the time-out, mode of operation, etc

	// In order to change WDE or the pre-scaler, we need to set WDCE (This will
	// allow updates for 4 clock cycles).

	// Set the WDCE bit (bit 4) and the WDE bit (bit 3) of the WDTCSR. The WDCE
	// bit must be set in order to change WDE or the watchdog pre-scalers.
	// Setting the WDCE bit will allow updates to the pre-scalers and WDE for 4
	// clock cycles then it will be reset by hardware.
	WDTCSR = _BV(WDCE) | _BV(WDE);

	/**
	 *	Setting the watchdog pre-scaler value with VCC = 5.0V and 16mHZ
	 *	WDP3 WDP2 WDP1 WDP0 | Number of WDT | Typical Time-out at Oscillator Cycles
	 *	0    0    0    0    |   2K cycles   | 16 ms
	 *	0    0    0    1    |   4K cycles   | 32 ms
	 *	0    0    1    0    |   8K cycles   | 64 ms
	 *	0    0    1    1    |  16K cycles   | 0.125 s
	 *	0    1    0    0    |  32K cycles   | 0.25 s
	 *	0    1    0    1    |  64K cycles   | 0.5 s
	 *	0    1    1    0    |  128K cycles  | 1.0 s
	 *	0    1    1    1    |  256K cycles  | 2.0 s
	 *	1    0    0    0    |  512K cycles  | 4.0 s
	 *	1    0    0    1    | 1024K cycles  | 8.0 s
	*/
	WDTCSR = wdps;

	interrupts();

	wdt_reset();
}

// Constructor
Watchdog::Watchdog(unsigned int periodInSeconds = 0)
{
	wdt_disable();

	_wdto = DeterminePeriod(periodInSeconds);
}

// Put the Arduino to deep sleep. Only an interrupt can wake it up.
void Watchdog::Sleep(unsigned long long periodInSeconds)
{
	unsigned long long ncycles = periodInSeconds;

	if (_wdto == WDTO_8S) {
		ncycles = periodInSeconds / 8;
	}
	else if (_wdto == WDTO_4S) {
		ncycles = periodInSeconds / 4;
	}
	else if (_wdto == WDTO_2S) {
		ncycles = periodInSeconds / 2;
	}

	Configure(true);

	// Turn off the ADC while asleep.
	power_all_disable();

	// There are five different sleep modes in order of power saving:
	// SLEEP_MODE_IDLE - the lowest power saving mode
	// SLEEP_MODE_ADC
	// SLEEP_MODE_PWR_SAVE
	// SLEEP_MODE_STANDBY
	// SLEEP_MODE_PWR_DOWN - the highest power saving mode
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);

	_sleepOrWaitCount = 0;

	while (_sleepOrWaitCount < ncycles)
	{
		// while some cycles left, sleep!
		// Enable sleep and enter sleep mode.
		sleep_mode();

		// CPU is now asleep and program execution completely halts!
		// Once awake, execution will resume at this point if the
		// watchdog is configured for resume rather than restart

		// When awake, disable sleep mode
		sleep_disable();

		Configure(true);
	}

	// put everything on again
	power_all_enable();

	Configure();
}

// Put the Arduino to deep sleep. Only an interrupt can wake it up.
void Watchdog::Wait(unsigned long long periodInSeconds)
{
	int ncycles = periodInSeconds;

	if (_wdto == WDTO_8S) {
		ncycles = periodInSeconds / 8;
	}
	else if (_wdto == WDTO_4S) {
		ncycles = periodInSeconds / 4;
	}
	else if (_wdto == WDTO_2S) {
		ncycles = periodInSeconds / 2;
	}

	_sleepOrWaitCount = 0;
	_waitTotal = ncycles;

	Configure(true);
}

void Watchdog::Reset(unsigned int periodInSeconds = 0)
{
	// Test if watchdog reset enabled
	if ((WDTCSR & _BV(WDE)) || (MCUSR & _BV(WDRF)))
	{
		wdt_reset();
	}

	if (periodInSeconds != 0)
	{
		_wdto = DeterminePeriod(periodInSeconds);
		_waitTotal = 0;

		Configure();
	}

	if (_waitTotal != 0)
	{
		_waitTotal = 0;
		Configure();
	}
}

// Define watchdog timer interrupt.
ISR(WDT_vect) {
	// increase the counter, we waited another n seconds
	_sleepOrWaitCount++;

	if (_sleepOrWaitCount < _waitTotal)
	{
		Watchdog::Configure(true);
	}
	else
	{
		if (_sleepOrWaitCount == _waitTotal && _waitTotal != 0)
		{
			Watchdog::Configure();
		}
	}
}
