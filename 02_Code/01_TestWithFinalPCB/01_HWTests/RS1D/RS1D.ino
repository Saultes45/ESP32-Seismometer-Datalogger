
/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : N/A
* Creation date : 2021-07-29 
* Version       : 0.1 (finished on ...)
* Modifications :
* Known bugs    :
*
*
* Possible Improvements
*
*
* Notes
*
*
* Ressources (Boards + Libraries Manager)
*
* HW serial for ESP32: https://learn.adafruit.com/adafruit-ultimate-gps-featherwing/basic-rx-tx-test
*
* TODO
*
* Terminology
* ------------
* STM = Seismometer = Geophone = RS1D
*
*/


// -------------------------- Includes --------------------------
//#include "parameters.h"
#include "Global.h"

// -------------------------- Set up --------------------------

void setup()
{
	
	#ifdef SERIAL_VERBOSE
		Serial.begin(CONSOLE_BAUD_RATE);
    while (!Serial) // wait for serial port to connect. Needed for native USB port only
		Serial.println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"); // Indicates a wakeup to the console
	#endif

	changeCPUFrequency(); // Limit CPU frequency to limit power consumption

	// Declare pins
	// -------------
	pinSetUp();

	// Variable reset
	// --------------
	resetSeismometerData();

turnRS1DON();

	// Preaparing the watchdog
	//------------------------
	prepareWDT();

	// Enable all ISRs
	//----------------
	#ifdef SERIAL_VERBOSE
		Serial.println("Watchdog ISR ready - this is the END of the setup");
	#endif
	timerAlarmEnable(timer);     // Watchdog

	// Check battery level
	// --------------------
	#ifdef SERIAL_VERBOSE
		Serial.println("Let's check the battery level, since we are boot");
	#endif
	checkBatteryLevel();


} // END OF SET UP


//delay(wdtTimeout * S_TO_MS_FACTOR); // DEBUG: trigger watchdog at every loop


// -------------------------- Loop --------------------------
void loop()
{



} // END OF LOOP





// END OF FILE
