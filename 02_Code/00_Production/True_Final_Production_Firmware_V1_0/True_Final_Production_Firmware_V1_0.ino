
/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : N/A
* Creation date : 2021-06-05 (Queen's b-day)
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
		Serial.println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"); // Indicates a wakeup to the console
	#endif

	changeCPUFrequency(); // Limit CPU frequency to limit power consumption

	// Declare pins
	// -------------
	pinSetUp();

	// Variable reset
	// --------------
	resetSeismometerData();


	// Set up RTC + SD
	// ----------------

	//  turnLogON(); // Start the feather datalogger
	//  delay(1000); // Wait for the LOG to be ready
	//  testRTC();
	//  testSDCard();
	//  turnLogOFF(); // Turn the feather datalogger OFF, we will go to OVW and we don't need it

	// Boolean state set up
	//----------------------
	currentState              = STATE_OVERWATCH;
	GPSNeeded                 = false;
	goodMessageReceived_flag  = false; // Reset the flag

	// Start the RS1D
	//----------------
	turnRS1DON();

	// Start the HW serial for the Geophone/STM
	// ----------------------------------------
	GeophoneSerial.begin(GEOPHONE_BAUD_RATE);
	GeophoneSerial.setTimeout(GEOPHONE_TIMEOUT_MS); // Set the timeout in [ms] (for findUntil)

	//waitForRS1DWarmUp();

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
	// The file has already been closed in the function logToSDCard

} // END OF SET UP


//delay(wdtTimeout * S_TO_MS_FACTOR); // DEBUG: trigger watchdog at every loop
//delay(wdtTimeout * S_TO_MS_FACTOR); // DEBUG: trigger watchdog at every loop

// -------------------------- Loop --------------------------
void loop()
{

	readRS1DBuffer();



	// Get GPS time if necessary (i.e. if we are in LOG) in order to empty the GPS buffer as often as possible
	//----------------------------
	#ifdef USE_GPS

		GPSTimeAvailable = false; // Reset the flag anyway

		if (GPSNeeded)
		{
			getGPSTime();
		}
	#endif




	if (goodMessageReceived_flag)
	{
		goodMessageReceived_flag = false; // Reset the flag

		switch (currentState) // Check which mode we are in
		{
		case STATE_OVERWATCH: // Choose between: Continue, Sleep, Log
			{
				// Statistics
				//-----------
				cnt_Overwatch++;

				nbr_bumpDetectedTotal = nbr_bumpDetectedTotal + nbr_bumpDetectedLast;

				// Sequencing logic
				//-----------------

				if (nbr_bumpDetectedLast > 0)
				{
					nbr_messagesWithBumps++;
					nbr_bumpDetectedLast = 0; // RESET (redundant but security)
				}

				if (cnt_Overwatch >= NBR_OVERWATCH_BEFORE_ACTION)
				{
					cnt_Overwatch = 0; // RESET (redundant but security) <DEBUG>
					fileName = "";    // Reset the filename // RESET (redundant but security) <DEBUG>
					
					#ifdef SERIAL_VERBOSE
					Serial.println("End of the watch");
					Serial.printf("We have reached the desired number of overwatch cycles: %d over %d \r\n", cnt_Overwatch, NBR_OVERWATCH_BEFORE_ACTION);
					Serial.println("Time to make a descision: LOG or SLEEP?");

					Serial.printf("We have detected %d bumps (or %d messages) - %d desired\r\n", nbr_bumpDetectedTotal, nbr_messagesWithBumps, NBR_BUMPS_DETECTED_BEFORE_LOG);
					#endif

					if (nbr_bumpDetectedTotal >= NBR_BUMPS_DETECTED_BEFORE_LOG)
					{
						#ifdef SERIAL_VERBOSE
						Serial.println("We reached our number-of-bumps goal, let's LOG!");
						#endif
						nbr_bumpDetectedTotal = 0;// RESET (redundant but security)
						nextState = STATE_LOG;

						// Start the feather datalogger
						turnLogON();
						delay(1000); // Wait 1s for the feather to be ready // <DEBUG>
						
						#ifdef USE_GPS
						// Start the GPS (it will take 1.9s to get the 1st GPS message (no fix))
						turnGPSON();
						GPSNeeded = true; // for the next loops, to know that a GPS buffer reading is indeed required
						// Set up the GPS so it is ready to be used for LOG (timestamps)
						testGPS();
						waitForGPSFix(); // This is blocking
						

						//noFixGPS = false; //<DEBUG> <REMOVE ME>
						
						// if there was no GPS fix, we need to start and enable the RTC
						if(noFixGPS)
						{
							GPSNeeded = false;
							GPSTimeAvailable = false;
							testRTC();
							//turnGPSOFF();
						}
						#else
						turnGPSON();// You have to turn it on otherwise the RTC I2C doesn't work
						delay(5000);
						testRTC();
						turnGPSOFF();
						#endif

						testSDCard();
						// Create a new file (we do that here because we need to wait RTC/GPS for the timestamp)
						createNewFile();
					}
					else
					{
						#ifdef SERIAL_VERBOSE
						Serial.println("We did NOT reach our number of bumps goal, let's SLEEP");
						#endif

						// Switch the RS1D off immediately to save power
						// Note: it will switched off anyway late when, the esp is put to deep sleep
						turnRS1DOFF();

						nextState = STATE_SLEEP;
					}
				}
				else
				{
					#ifdef SERIAL_VERBOSE
					Serial.printf("Keep looping on OVERWATCH for %d more iterations\r\n", NBR_OVERWATCH_BEFORE_ACTION - cnt_Overwatch);
					#endif
					nextState = STATE_OVERWATCH;
				}


				break;
			}/* [] END OF case STATE_OVERWATCH: */

		case STATE_LOG:
			{
				// Statistics
				//-----------
				nbr_bumpDetectedTotal = nbr_bumpDetectedTotal + nbr_bumpDetectedLast;

				if (nbr_bumpDetectedLast > 0)
				{
					nbr_messagesWithBumps++;
				}

				// Log to SD
				//----------
				logToSDCard();

				// Sequencing logic
				//-----------------

				cnt_Log ++; // Increment the counter telling how many LOG cycles have been performed

				if (cnt_Log >= NBR_LOG_BEFORE_ACTION)
				{

					cnt_Log = 0; // RESET (redundant but security) <DEBUG>
					fileName = "";    // Reset the filename // RESET (redundant but security) <DEBUG>
					dataFile.close();// RESET (redundant but security) <DEBUG>

					#ifdef SERIAL_VERBOSE
					Serial.println("Let's check the battery level, since we are at the end of the LOG");
					#endif
					checkBatteryLevel();
					// The file has already been closed in the function logToSDCard
					
					#ifdef SERIAL_VERBOSE
					Serial.println("End of the LOG");
					Serial.printf("We have reached the desired number of LOG cycles: %d over %d \r\n", cnt_Log, NBR_LOG_BEFORE_ACTION);
					Serial.println("Time to make a descision: OVW or continue LOG?");
					Serial.printf("We have detected %d bumps (or %d messages) during this log cycle of %d messages\r\n", nbr_bumpDetectedTotal, nbr_messagesWithBumps, NBR_LOG_BEFORE_ACTION);
					#endif

					
					

					if (nbr_bumpDetectedTotal >= NBR_BUMPS_DETECTED_BEFORE_LOG)
					{
						
						#ifdef SERIAL_VERBOSE
						Serial.println("We reached our number-of-bumps goal, let's CONTINUE LOG");
						#endif
						nextState = STATE_LOG;

						// We need to create a new file
						createNewFile();

						#ifdef SERIAL_VERBOSE
						Serial.print("Resetting some variables...");
						#endif
						
						nbr_bumpDetectedTotal   = 0;
						nbr_bumpDetectedLast    = 0;
						nbr_messagesWithBumps   = 0;
						cnt_Overwatch           = 0;
						cnt_Log                 = 0;
						
						#ifdef SERIAL_VERBOSE
						Serial.println("done");
						#endif
					}
					else
					{

						#ifdef SERIAL_VERBOSE
						Serial.println("We did NOT reach our number-of-bumps goal, let's OVERWATCH");
						#endif

						nextState = STATE_OVERWATCH;

						turnLogOFF(); // Stop the feather datalogger
						turnGPSOFF(); // Stop the GPS
						GPSNeeded = false;
						// Leave the RS1D ON since we are going to overwatch
					}
				}
				else
				{
					#ifdef SERIAL_VERBOSE
					Serial.printf("Keep looping on LOG for %d more iterations\r\n", NBR_LOG_BEFORE_ACTION - cnt_Log);
					#endif
					nextState = STATE_LOG;

				}

				break;
			}/* [] END OF case STATE_LOG: */

		default:
			{
				#ifdef SERIAL_VERBOSE
				Serial.println("Unknown step type, something went very wrong!");
				Serial.printf("Asked step: %d \r\n", currentState);
				#endif

				//Trigger the watchdog to force a reboot
				delay(wdtTimeout * S_TO_MS_FACTOR);

				break;
			}/* [] END OF "unknown" and "other" case */
		} // END of SWITCH

		// Sleep management
		//------------------
		if (nextState == STATE_SLEEP)
		{
			Serial.println("Since we did not receive enough bumps during OVERWATCH, we sleep to save battery");

			// Deep sleep
			// -----------

			prepareSleep();
			
			// Start the deep sleep
			esp_deep_sleep_start();

			#ifdef SERIAL_VERBOSE
			Serial.println("If you see this, there is a problem with the deep sleep");
			#endif

		}
		else // if the next step is NOT a sleep
		{
			//      Serial.println("The next state is NOT a SLEEP");

			if (nextState != currentState)
			{
				#ifdef SERIAL_VERBOSE
				Serial.print("Looks like we need to change the state to: ");
				Serial.print(nextState);
				Serial.println(" (1 = LOG, 2 = SLEEP, 3 = OVW)");
				Serial.print("Resetting some variables...");
				#endif

				nbr_bumpDetectedTotal   = 0;
				nbr_bumpDetectedLast    = 0;
				nbr_messagesWithBumps   = 0;
				cnt_Overwatch           = 0;
				cnt_Log                 = 0;

				#ifdef SERIAL_VERBOSE
				Serial.println("Done");
				#endif

				currentState = nextState; // For next loop

				#ifdef SERIAL_VERBOSE
				Serial.println("Ready for the next state");
				#endif

			}
			//      else
			//      {
			//        Serial.println("Just keep on looping");
			//      }
		}/* [] END OF "if" SLEEP: */


	} // END of goodMessageReceived_flag


	// Reset the timer (i.e. feed the watchdog)
	//------------------------------------------
	timerWrite(timer, 0); // need to be before a potential sleep

} /* [] END OF ininite loop */





// END OF FILE
