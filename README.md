# ArduinoWatchdog
Watchdog functions for Arduino, using wdt to sleep of wait.
When de ardiuno is asleep the watchdog is also active, in case of a failure the system gets rebooted.
When the software has a long during action, the watchdog can be suspended for a limited time using wait. This way the code can execute a longer action, and still gets rebooted in case of an failure.

