
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
// Personal libraries
//--------------------
#include "Global.h"


// -------------------------- Defines and Const --------------------------


#define SERIAL_VERBOSE

// CPU frequency
#define MAX_CPU_FREQUENCY             240
#define TARGET_CPU_FREQUENCY          40



#define NBR_OVERWATCH_BEFORE_ACTION     50
#define NBR_BUMPS_DETECTED_BEFORE_LOG   100
#define NBR_LOG_BEFORE_ACTION           75
#define TIME_TO_SLEEP               10        // In [s], time spent in the deep sleep state
#define uS_TO_S_FACTOR              1000000ULL  // Conversion factor from us to s for the deepsleep // Source: https://github.com/espressif/arduino-esp32/issues/3286

// Bump detection
const int32_t BUMP_THRESHOLD_POS = +100000;
const int32_t BUMP_THRESHOLD_NEG = -100000;


// RS1D
#define       RX_BUFFER_SIZE        650     //640 actually needed, margin of 10 observed
const unsigned long RS1D_DEPILE_TIMEOUT   = 100;  // in [ms]
const uint16_t    rs1dWarmUpTime      = 5000; // in [ms]
#define       GEOPHONE_BAUD_RATE    230400    // Baudrate in [bauds] for serial communication with the Geophone 
#define       GEOPHONE_TIMEOUT    100     // For the character search in buffer
#define       GeophoneSerial        Serial1   // Use the 2nd (out of 3) hardware serial
const uint8_t     RS1D_PWR_PIN_1        = 25u;    // To turn the geophone ON and OFF
const uint8_t     RS1D_PWR_PIN_2        = 26u;    // To turn the geophone ON and OFF


// GPS time
#include <Adafruit_GPS.h>
Adafruit_GPS GPS(&Wire);

// WATCHDOG
hw_timer_t * timer = NULL;
const uint8_t wdtTimeout = 3; // Watchdog timeout in [s]

// LOG+SD
#define PIN_CS_SD                   33     // Chip Select (ie CS/SS) for SPI for SD card
const char SOM_LOG                = '$'; // Start of message indicator, mostly used for heath check (no checksum)
const char FORMAT_SEP               = ','; // Separator between the different files so that the data can be read/parsed by softwares
const uint16_t MAX_LINES_PER_FILES    = NBR_LOG_BEFORE_ACTION;  // Maximum number of lines that we want stored in 1 SD card file. It should be about ...min worth
const char SESSION_SEPARATOR_STRING[]   =  "----------------------------------";


// -------------------------- Global Variables --------------------------


// RS1D
char    RS1Dmessage[RX_BUFFER_SIZE];      // Have you seen the sheer size of that!!! Time...to die.
bool  goodMessageReceived_flag    = false;  // Set to "bad message" first
int32_t seismometerAcc[50];
uint8_t nbr_bumpDetectedLast    = 0;        // Max of 50; // This should be the output of a function and should be local


// State Machine
volatile uint8_t  currentState      = STATE_EMPTY;  // Used to store which step we are at, default is state "empty"
volatile uint8_t  nextState         = STATE_EMPTY;  // Used to store which step we are going to do next, default is state "empty"
uint8_t       cnt_Overwatch         = 0;      // This does NOT need to survive reboots NOR ISR, just global
uint8_t       cnt_Log               = 0;      // This does NOT need to survive reboots NOR ISR, just global
uint16_t      nbr_bumpDetectedTotal   = 0;      // <KEEP GLOBAL>, max of 50*nbr overwatch
uint8_t       nbr_messagesWithBumps   = 0;      // <KEEP GLOBAL>, max of nbr overwatch


// LOG + SD
char    timeStampFormat_Line[]      = "YYYY_MM_DD__hh_mm_ss";   // naming convention for EACH LINE OF THE FILE logged to the SD card
char    timeStampFormat_FileName[]  = "YYYY_MM_DD__hh_mm_ss";   // naming convention for EACH FILE NAME created on the SD card
uint16_t    cntLinesInFile        = 0;            // Written at the end of a file for check (36,000 < 65,535)
uint32_t    cntFile               = 0;            // Counter that counts the files written in the SD card this session (we don't include prvious files), included in the name of the file, can handle 0d to 99999d (need 17 bits)
String      fileName              = "";           // Name of the current opened file on the SD card
File        dataFile;                               // Only 1 file can be opened at a certain time, <KEEP GLOBAL>

// WATCHDOG
DateTime lastWatchdogTrigger;     // <NOT YET USED>
volatile uint32_t nbr_WatchdogTrigger = 0;     // <NOT YET USED>

//RTC
RTC_PCF8523         rtc;
DateTime            time_loop;             // MUST be global!!!!! or it won't update
DateTime            timestampForFileName;  // MUST be global!!!!! or it won't update


// -------------------------- Set up --------------------------

void setup()
{
#ifdef SERIAL_VERBOSE
  Serial.begin(115200);
  Serial.println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"); // Indicates a wakeup to the console
#endif

  changeCPUFrequency(); // Limit CPU frequency to limit power consumption

  // Declare pins
  // -------------
  pinSetUp();


  // Variable reset
  // --------------
  for (uint8_t cnt_fill=0; cnt_fill < 50; cnt_fill++)
  {
    seismometerAcc[cnt_fill] = (int32_t)0;
  }

  // Set up RTC + SD
  // ----------------
  testRTC();
  testSDCard();

  currentState = STATE_OVERWATCH;


  // Start the RS1D
  //----------------
  turnRS1DON();

  // Start the HW serial for the Geophone/STM
  // ----------------------------------------
  GeophoneSerial.begin(GEOPHONE_BAUD_RATE);
  GeophoneSerial.setTimeout(GEOPHONE_TIMEOUT);// Set the timeout to 100 milliseconds (for findUntil)

  waitForRS1DWarmUp();

#ifdef SERIAL_VERBOSE
  Serial.println("The next transmission might be the last transmission to the PC, if you turned verbose OFF");
#endif  

  // Preaparing the night watch (watchdog)
  //-------------------------------------
  // Remember to hold the door
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * uS_TO_S_FACTOR, false); // set time is in [us]
  // Enable all the ISR later

#ifdef SERIAL_VERBOSE
  Serial.println("And here, we, go, ...");
  // Do not go gentle into that good night
#endif

  // Enable all ISRs
  //----------------
  timerAlarmEnable(timer);     // Watchdog

} // END OF SET UP


// -------------------------- Loop --------------------------
void loop() {

  readRS1DBuffer();

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
        }
        
        if (cnt_Overwatch >= NBR_OVERWATCH_BEFORE_ACTION)
        {
          #ifdef SERIAL_VERBOSE
          Serial.println("End of the watch");
          Serial.printf("We have reached the desired number of overwatch cycles: %d over %d \r\n", cnt_Overwatch, NBR_OVERWATCH_BEFORE_ACTION);
          Serial.println("Time to make a descision: LOG or SLEEP?");
          
          Serial.printf("We have detected %d bumps (or %d messages) over %d desired\r\n", nbr_bumpDetectedTotal, nbr_messagesWithBumps, NBR_BUMPS_DETECTED_BEFORE_LOG);
          #endif
          
          if (nbr_bumpDetectedTotal >= NBR_BUMPS_DETECTED_BEFORE_LOG)
          {
            #ifdef SERIAL_VERBOSE
            Serial.println("We reached our number of bumps goal, let's LOG");
            #endif
            nextState = STATE_LOG;

            // Start the GPS to get the UTC time up to 100ds of ms
            startGPS();
            waitUntilGPSFix();
            

            // Create a new file
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
          #ifdef SERIAL_VERBOSE
          Serial.println("We reached our number of messages logged, let's overwatch (or log?)");
          Serial.printf("We have detected %d bumps (or %d messages) during this log cycle of %d messages\r\n", nbr_bumpDetectedTotal, nbr_messagesWithBumps, NBR_LOG_BEFORE_ACTION);
          #endif
          nextState = STATE_OVERWATCH;

          // Close the file (even if the number of lines per file has not been reached)
          dataFile.close();
          // Reset the line counter
          cntLinesInFile = 0;

          #ifdef SERIAL_VERBOSE
          Serial.println("Let's check the battery level, since we are at the end of the LOG");
          #endif
          checkBatteryLevel();
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
        delay(wdtTimeout * 1000);

        break;
      }/* [] END OF "unknown" and "other" case */
    } // END of SWITCH

    // Sleep management
    //------------------------------------------
    if (nextState == STATE_SLEEP)
    {
      Serial.println("Since we did not receive enough bumps during OVERWATCH, we sleep to save battery");

      // Deep sleep
      // -----------
      
      // Prepare the sleep with all the required parameters
      esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_OFF);
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM,ESP_PD_OPTION_OFF);
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM,ESP_PD_OPTION_OFF);
      // Serial.println("Error = " + String(err)); // Tell the console the error status
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); // Set up deep sleep wakeup timer
      
      #ifdef SERIAL_VERBOSE
      Serial.println("Going to sleep...");
      Serial.flush(); // Wait until serial buffer is clear <Please check because might be a waste of time: maybe sleep also clears the buffer?>
      #endif
      
      
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
        Serial.println("Looks like we need to change the state");
        Serial.println("Resetting some variables...");
        #endif
        
        nbr_bumpDetectedTotal   = 0;
        nbr_bumpDetectedLast    = 0;
        nbr_messagesWithBumps   = 0;
        cnt_Overwatch           = 0;
        cnt_Log                 = 0;
        
        currentState = nextState; // For next loop
        
      }
      //      else
      //      {
      //        Serial.println("Just keep on looping");
      //      }
    }/* [] END OF if SLEEP: */


  } // END of goodMessageReceived_flag


  // Reset the timer (i.e. feed the watchdog)
  //------------------------------------------
  timerWrite(timer, 0); // need to be before a potential sleep

} // END OF LOOP




// END OF FILE
