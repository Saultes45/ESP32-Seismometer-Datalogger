
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
//ESP32
#include <esp_system.h>
#include <rom/rtc.h> // reset reason


//String Formatting
#include <string.h>
#include <stdio.h>

//SD card
#include <SPI.h>
#include <SD.h>

// RTC
#include <RTClib.h> // use the one from Adafruit, not the forks with the same name


// -------------------------- Defines and Const --------------------------


#define SERIAL_VERBOSE // Comment out to remove message to the console (and save some exec time and power)

// CPU frequency
#define MAX_CPU_FREQUENCY             240
#define TARGET_CPU_FREQUENCY          40


// Battery
#define BATT_VOLTAGE_DIVIDER_FACTOR   2       // [N/A]
#define LOW_BATTERY_THRESHOLD         3.1     // in [V]
#define BATT_PIN                      35      // To detect the Lipo battery remaining charge, GPIO35 on Adafruit ESP32 (35 on dev kit)


// STATE MACHINE
#ifndef STATES_H
#define STATES_H
#define STATE_OVERWATCH      (3u)
#define STATE_SLEEP          (2u)
#define STATE_LOG            (1u)
#define STATE_EMPTY          (0u)
#endif /* STATES_H */

#define NBR_OVERWATCH_BEFORE_ACTION     50
#define NBR_BUMPS_DETECTED_BEFORE_LOG   100
#define NBR_LOG_BEFORE_ACTION           75
#define TIME_TO_SLEEP               10          // In [s], time spent in the deep sleep state
#define S_TO_US_FACTOR              1000000ULL  // Conversion factor from [s] to [us] for the deepsleep // Source: https://github.com/espressif/arduino-esp32/issues/3286
#define S_TO_MS_FACTOR              1000        // Conversion factor from [s] to [ms] for the delay

// Bump detection
const int32_t BUMP_THRESHOLD_POS = +100000;
const int32_t BUMP_THRESHOLD_NEG = -100000;


// RS1D

const uint8_t  NBR_ACCELERATIONS_PER_MESSAGE   = 50; // scope controlled  + cannot be reassigned
#define       RX_BUFFER_SIZE        650     //640 actually needed, margin of 10 observed
const unsigned long RS1D_DEPILE_TIMEOUT_MS   = 100;  // in [ms]
const uint16_t    RS1D_WARMUP_TIME_MS      = 5000; // in [ms]
#define       GEOPHONE_BAUD_RATE    230400    // Baudrate in [bauds] for serial communication with the Geophone
#define       GEOPHONE_TIMEOUT_MS    100     // For the character search in buffer, in [ms]
#define       GeophoneSerial        Serial1   // Use the 2nd (out of 3) hardware serial of the ESP32
const uint8_t RS1D_PWR_PIN_1              = 14;      // To turn the geophone ON and OFF


// WATCHDOG
hw_timer_t * timer = NULL;
const uint8_t wdtTimeout = 3; // Watchdog timeout in [s]
const uint8_t  MAX_NBR_WDT = 10;

// LOG+SD
#define PIN_CS_SD                   33     // Chip Select (ie CS/SS) for SPI for SD card
const char SOM_LOG                = '$'; // Start of message indicator, mostly used for heath check (no checksum)
const char FORMAT_SEP               = ','; // Separator between the different files so that the data can be read/parsed by softwares
const uint16_t MAX_LINES_PER_FILES    = NBR_LOG_BEFORE_ACTION;  // Maximum number of lines that we want stored in 1 SD card file. It should be about ...min worth
const char SESSION_SEPARATOR_STRING[]   =  "----------------------------------";
const uint8_t     LOG_PWR_PIN_1        = 25;    // To turn the geophone ON and OFF
const uint8_t     LOG_PWR_PIN_2        = 26;    // To turn the geophone ON and OFF

// -------------------------- Global Variables --------------------------


// RS1D
char    RS1Dmessage[RX_BUFFER_SIZE];      // Have you seen the sheer size of that!!! Time...to die.
bool  goodMessageReceived_flag    = false;  // Set to "bad message" first
int32_t seismometerAcc[NBR_ACCELERATIONS_PER_MESSAGE];
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
const uint8_t GPS_BOOST_ENA_PIN           = 21;      // To turn the BOOST converter of the GPS ON and OFF
RTC_PCF8523         rtc;
DateTime            time_loop;             // MUST be global!!!!! or it won't update
DateTime            timestampForFileName;  // MUST be global!!!!! or it won't update

// -------------------------- Functions declaration --------------------------
void      pinSetUp            (void);
void      checkBatteryLevel   (void);

void      turnRS1DOFF         (void);
void      turnRS1DON          (void);

void      turnLogOFF          (void);
void      turnLogON           (void);

void      turnGPSOFF          (void);
void      turnGPSON           (void);

void      waitForRS1DWarmUp   (void);
void      testRTC             (void);
void      logToSDCard         (void);
void      readRS1DBuffer      (void);
void      parseGeophoneData   (void);
void      createNewFile       (void);
void      testSDCard          (void);
uint32_t  hex2dec             (char * a);
void      blinkAnError        (uint8_t errno);
void      changeCPUFrequency  (void);           // Change the CPU frequency and report about it over serial



// -------------------------- ISR ----------------
// Watchdog
void IRAM_ATTR resetModule()
{
  nbr_WatchdogTrigger ++; // For statistics
  lastWatchdogTrigger = rtc.now();

  #ifdef SERIAL_VERBOSE
    ets_printf("Problem! Watchdog trigger: reboot or sleep?\r\n");
    ets_printf("Number of WDT trigger: %d / %d\r\n", nbr_WatchdogTrigger, MAX_NBR_WDT);
  #endif

  if (nbr_WatchdogTrigger > MAX_NBR_WDT)
  {
    // Sleep instead of reboot

    #ifdef SERIAL_VERBOSE
      ets_printf("Sleeping...\r\n");
    #endif

    const uint64_t watchdog_recurrent_time_in_us = 1800;

    // Prepare the sleep with all the required parameters
    esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM,ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM,ESP_PD_OPTION_OFF);
    // Serial.println("Error = " + String(err)); // Tell the console the error status
    esp_sleep_enable_timer_wakeup(watchdog_recurrent_time_in_us * S_TO_US_FACTOR); // Set up deep sleep wakeup timer


    // Start the deep sleep
    esp_deep_sleep_start();
  }
  else
  {
    #ifdef SERIAL_VERBOSE
      ets_printf("Rebooting...\r\n");
    #endif

    esp_restart();
  }
}


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

  turnLogON(); // Start the feather datalogger
  testRTC();
  testSDCard();

  currentState = STATE_OVERWATCH;
  turnLogON(); // Turn the feather datalogger OFF, we will go to OVW and we don't need it


  // Start the RS1D
  //----------------
  turnRS1DON();

  // Start the HW serial for the Geophone/STM
  // ----------------------------------------
  GeophoneSerial.begin(GEOPHONE_BAUD_RATE);
  GeophoneSerial.setTimeout(GEOPHONE_TIMEOUT_MS); // Set the timeout in [ms] (for findUntil)

  waitForRS1DWarmUp();

#ifdef SERIAL_VERBOSE
  Serial.println("The next transmission might be the last transmission to the PC, if you turned verbose OFF");
#endif

  // Preaparing the night watch (watchdog)
  //-------------------------------------
  // Remember to hold the door
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * S_TO_US_FACTOR, false); // set time is in [us]
  // Enable all the ISR later

#ifdef SERIAL_VERBOSE
  Serial.println("And here, we, go, ...");
  // Do not go gentle into that good night
#endif

  // Enable all ISRs
  //----------------
  timerAlarmEnable(timer);     // Watchdog

}


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

            // Start the feather datalogger
            turnLogON();
			delay(50); // wait for the feather to be ready
			// Start the GPS
            turnGPSON();

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
		  
          turnLogOFF(); // Stop the feather datalogger
          turnGPSOFF(); // Stop the GPS
          // Leave the RS1D ON since we are going to overwatch

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
        delay(wdtTimeout * S_TO_MS_FACTOR);

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
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * S_TO_US_FACTOR); // Set up deep sleep wakeup timer

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

} /* [] END OF ininite loop */


//******************************************************************************************
void pinSetUp (void)
{

  // Declare which pins of the ESP32 will be used

  // LED pins
  //--------------
  pinMode (LED_BUILTIN , OUTPUT);

  // Analog pins
  //--------------
  pinMode (BATT_PIN    , INPUT);

  // Power pins
  //--------------
  pinMode (RS1D_PWR_PIN_1    , OUTPUT);

  pinMode (LOG_PWR_PIN_1    , OUTPUT);
  pinMode (LOG_PWR_PIN_2    , OUTPUT);

  pinMode (GPS_BOOST_ENA_PIN    , OUTPUT);

  // Initial pin state
  //-------------------
  digitalWrite(LED_BUILTIN, LOW);

  turnRS1DOFF();
  turnLogOFF();
  turnGPSOFF();

}

//******************************************************************************************
void checkBatteryLevel (void)
{

  uint16_t rawBattValue = analogRead(BATT_PIN); //read the BATT_PIN pin value,   //12 bits (0 – 4095)
  float battVoltage     = rawBattValue * (3.30 / 4095.00) * BATT_VOLTAGE_DIVIDER_FACTOR; //convert the value to a true voltage

  #ifdef SERIAL_VERBOSE
    Serial.println("----------------------------------------");
    Serial.println("Estimating battery level");

    Serial.print("Raw ADC (0-4095): ");
    Serial.print(rawBattValue);
    Serial.println(" LSB");

    Serial.printf("Voltage: %f [V]", battVoltage);

    if (battVoltage < LOW_BATTERY_THRESHOLD) // check if the battery is low (i.e. below the threshold)
    {
      Serial.println("Warning: Low battery!");
    }
    else
    {
      Serial.println("Battery level OK");
    }

    Serial.println("----------------------------------------");
  #endif

}

//******************************************************************************************
void turnRS1DOFF(void) {

#ifdef SERIAL_VERBOSE
  Serial.println("Turning RS1D OFF...");
#endif

  digitalWrite(RS1D_PWR_PIN_1, LOW);

#ifdef SERIAL_VERBOSE
  Serial.println("RS1D is OFF");
#endif

}

//******************************************************************************************
void turnRS1DON(void) {

#ifdef SERIAL_VERBOSE
  Serial.println("Turning RS1D ON...");
#endif

  digitalWrite(RS1D_PWR_PIN_1, HIGH);

#ifdef SERIAL_VERBOSE
  Serial.println("RS1D is ON");
#endif

}

//******************************************************************************************
void turnLogOFF(void) {

#ifdef SERIAL_VERBOSE
  Serial.println("Turning Log OFF...");
#endif

  digitalWrite(LOG_PWR_PIN_1, LOW);
  digitalWrite(LOG_PWR_PIN_2, LOW);

#ifdef SERIAL_VERBOSE
  Serial.println("Log is OFF");
#endif

}

//******************************************************************************************
void turnLogON(void) {

#ifdef SERIAL_VERBOSE
  Serial.println("Turning Log ON...");
#endif

  digitalWrite(LOG_PWR_PIN_1, HIGH);
  digitalWrite(LOG_PWR_PIN_2, HIGH);


#ifdef SERIAL_VERBOSE
  Serial.println("Log is ON");
#endif

}

//******************************************************************************************
void turnGPSOFF(void) {

#ifdef SERIAL_VERBOSE
  Serial.println("Turning GPS OFF...");
#endif

  digitalWrite(GPS_BOOST_ENA_PIN, LOW);

#ifdef SERIAL_VERBOSE
  Serial.println("GPS is OFF");
#endif

}

//******************************************************************************************
void turnGPSON(void) {

#ifdef SERIAL_VERBOSE
  Serial.println("Turning GPS ON...");
#endif

  digitalWrite(GPS_BOOST_ENA_PIN, HIGH);


#ifdef SERIAL_VERBOSE
  Serial.println("GPS is ON");
#endif

}

//******************************************************************************************
void waitForRS1DWarmUp(void) {

#ifdef SERIAL_VERBOSE
  Serial.println("RS1D Warmup...");
#endif

  delay(RS1D_WARMUP_TIME_MS); // Just wait for the sensor to be ready
}

//******************************************************************************************
void logToSDCard(void) {

  // Create a string for assembling the data to log
  //-----------------------------------------------
  String dataString  = "";
  String myTimestamp = "";

  // Add start of message character
  //--------------------------------
  dataString += SOM_LOG;

  // Read the RTC time (will be GPS soon)
  //------------------------------------------
  time_loop = rtc.now(); // MUST be global!!!!! or it won't update
  // We are obliged to do that horror because the method "toString" input parameter is also the output
  char timeStamp[sizeof(timeStampFormat_Line)];
  strncpy(timeStamp, timeStampFormat_Line, sizeof(timeStampFormat_Line));
  dataString += time_loop.toString(timeStamp);
  dataString += ".000"; // add the [ms] placeholder 
  dataString += FORMAT_SEP;

  // Write the accelerations in the message
  //------------------------------------------
  for (uint8_t cnt_Acc = 0; cnt_Acc < NBR_ACCELERATIONS_PER_MESSAGE; cnt_Acc++)
  {
    // Save the results (acceleration is measured in ???)
    dataString += String(seismometerAcc[cnt_Acc]);
    dataString += FORMAT_SEP;
  }

#ifdef SERIAL_VERBOSE
  Serial.println("Data going to SD card: ");
  Serial.println(dataString);
  Serial.flush();
#endif

  // Log data in the SD card
  //------------------------

  // Check if the file is available
  if (dataFile)
  {
    // We do it like this because of the "\r\n" not desired at the end of a file
    if (cntLinesInFile >= MAX_LINES_PER_FILES - 1) // Check if we have reached the max. number of lines per file
    {
      // Boost the frequency of the CPU to the MAX so that the writing takes less time
      // setCpuFrequencyMhz(MAX_CPU_FREQUENCY);
      // Write to the file w/out the "\r\n"
      dataFile.print(dataString);
      // Close the file
      dataFile.close();
      // Reset the line counter
      cntLinesInFile = 0;
      // Create a new file
      createNewFile();
      #ifdef SERIAL_VERBOSE
      Serial.println("Reached the max number of lines per file, starting a new one");
      #endif
      // Limit back the frequency of the CPU to consume less power
      // setCpuFrequencyMhz(TARGET_CPU_FREQUENCY);
    }
    else // wE ARE STILL UNDER THE LIMIT OF NUMBER OF LINES PER FILE
    {
      dataFile.println(dataString);
      cntLinesInFile++; // Increment the lines-in-current-file counter

      #ifdef SERIAL_VERBOSE
      Serial.println("Data have been written");
      Serial.print("Current number of lines: ");
      Serial.print(cntLinesInFile);
      Serial.print("/");
      Serial.println(MAX_LINES_PER_FILES);
      #endif
    }
  }
  // If the file isn't open, pop up an error
  else {
    #ifdef SERIAL_VERBOSE
    Serial.print("Error writting to the file: ");
    Serial.println(fileName);
    #endif
    fileName = "";        // Reset the filename
    cntLinesInFile = 0;   // Reset the lines-in-current-file counter
  }

}


//******************************************************************************************
void readRS1DBuffer(void)
{
  // const uint16_t  max_nbr_depiledChar = 500; // scope controlled  + cannot be reassigned
  uint16_t  cnt_savedMessage = 0;

  if (GeophoneSerial.available())
  {
    if(GeophoneSerial.find("[\""))// test the received buffer for SOM_CHAR_SR
    {

      cnt_savedMessage = 0;
      RS1Dmessage[cnt_savedMessage] = '[';
      cnt_savedMessage ++;
      RS1Dmessage[cnt_savedMessage] = '\"';
      cnt_savedMessage ++;

      unsigned long startedWaiting = millis();

      while((RS1Dmessage[cnt_savedMessage-1] != ']') && (millis() - startedWaiting <= RS1D_DEPILE_TIMEOUT_MS) && (cnt_savedMessage < RX_BUFFER_SIZE))
      {
        if (GeophoneSerial.available())
        {
          RS1Dmessage[cnt_savedMessage] = GeophoneSerial.read();
          cnt_savedMessage++;
        }
      }

      if ((RS1Dmessage[cnt_savedMessage-1] == ']')) // if any EOM found then we parse
      {
        delay(1);             // YES, it IS ABSOLUTELY necessay, do NOT remove it
        parseGeophoneData();
      }
    }
    memset(RS1Dmessage, 0, RX_BUFFER_SIZE); // clean the message field anyway
  }
}

//******************************************************************************************
void parseGeophoneData(void)
{
  if (strstr(RS1Dmessage, "\"]"))
  {
    const uint8_t  max_nbr_extractedCharPerAcc  = 9; // scope controlled  + cannot be reassigned

    char    temp1[max_nbr_extractedCharPerAcc];
    uint8_t cnt_accvalues = 0;
    char    *p = RS1Dmessage;
    uint8_t cnt = 0;

    nbr_bumpDetectedLast = 0; //RESET

    for (uint8_t cnt_fill=0; cnt_fill < NBR_ACCELERATIONS_PER_MESSAGE; cnt_fill++)
    {
      seismometerAcc[cnt_fill] = (int32_t)0;
    }

    // Parse the aquited message to get 1st acceleration (1st is different)
    //----------------------------------------------------------------------
    p = strchr(p, '[')+1;
    p++; // Avoid the "

    memset(temp1,'\0', max_nbr_extractedCharPerAcc);
    while((*p != '"') && (cnt < max_nbr_extractedCharPerAcc)) // No error checking... Try not. Do, or do not. There is no try
    {
      temp1[cnt] = *p;
      cnt++;
      p++;
    }
    seismometerAcc[0] = (int32_t)(hex2dec(temp1));
    if((seismometerAcc[0] > BUMP_THRESHOLD_POS) || (seismometerAcc[0] < BUMP_THRESHOLD_NEG))
    {
      nbr_bumpDetectedLast++;
    }

    // Parse the rest of the accelerations
    //-------------------------------------
    for (cnt_accvalues = 1; cnt_accvalues < NBR_ACCELERATIONS_PER_MESSAGE; cnt_accvalues++)
    {
      p = strchr(p, ',')+1;
      p++; //Avoid the "
      cnt = 0;
      memset(temp1, '\0', max_nbr_extractedCharPerAcc);
      while((*p != '"') & (cnt < max_nbr_extractedCharPerAcc))
      {
        temp1[cnt] = *p;
        cnt++;
        p++;
      }
      seismometerAcc[cnt_accvalues] = (int32_t)(hex2dec(temp1));
      if((seismometerAcc[cnt_accvalues] > BUMP_THRESHOLD_POS) || (seismometerAcc[cnt_accvalues] < BUMP_THRESHOLD_NEG))
      {
        nbr_bumpDetectedLast++;
      }
    }
    goodMessageReceived_flag = true; // Set the flag because the function was able to run all the above code
  }
}


//******************************************************************************************
uint32_t hex2dec(char * a) // For the Geophone data stream
{

  // Transforms an hex coded number encoded as a char to a number in decimal

  char     c = 0;
  uint32_t n = 0;

  while (*a) {
    c = *a++;

    if (c >= '0' && c <= '9') {
      c -= '0';

    } else if (c >= 'a' && c <= 'f') {
      c = (c - 'a') + 10;

    } else if (c >= 'A' && c <= 'F') {
      c = (c - 'A') + 10;

    } else {
      goto INVALID;
    }

    n = (n << 4) + c;
  }

  return n;

INVALID:
  return -1;
}

//******************************************************************************************
void testRTC(void) {

  #ifdef SERIAL_VERBOSE
  Serial.println("Testing the RTC...");
  #endif

  if (!rtc.begin())
  {
    #ifdef SERIAL_VERBOSE
    Serial.println("Couldn't find RTC, is it properly connected?");
    Serial.flush(); // Wait until there all the text for the console have been sent
    #endif
    blinkAnError(6);
  }

  if (!rtc.initialized() || rtc.lostPower())
  {
    #ifdef SERIAL_VERBOSE
    Serial.println("RTC is NOT initialized. Use the NTP sketch to set the time!");
    Serial.println("This is not an important error");
    Serial.println("The datalogger might still be able to function properly");
    #endif
    //blinkAnError(7); // <---- this might be unnecessary
  }

  // When the RTC was stopped and stays connected to the battery, it has
  // to be restarted by clearing the STOP bit. Let's do this to ensure
  // the RTC is running.
  rtc.start();

}


//******************************************************************************************
void blinkAnError(uint8_t errno) {  // Use an on-board LED (the red one close to the micro USB connector, left of the enclosure) to signal errors (RTC/SD)

  //errno argument tells how many blinks per period to do. Must be  strictly less than 10

  while(1) { // Infinite loop: stay here until power cycle
    uint8_t i;

    // This part is executed errno times, quick blink
    for (i=0; i<errno; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }


    // This part is executed (10 - errno) times, led off (waiting to reblink)
    for (i=errno; i<10; i++) {
      delay(200);
    }

    // Total time spent is: errno * (100 + 100) + (10 - errno) * 200 = 2000ms
  }
}

//******************************************************************************************
void testSDCard(void)
{
#ifdef SERIAL_VERBOSE
  Serial.print("Initializing SD card...");
#endif

  // Check if the card is present and can be initialized:
  if (!SD.begin(PIN_CS_SD)) {
    #ifdef SERIAL_VERBOSE
    Serial.println("Card failed, or not present");
    #endif
    blinkAnError(2);
  }

#ifdef SERIAL_VERBOSE
  Serial.println("Card initialized");
#endif

  // Add a separator file (empty but with nice title) so the user knows a new DAQ session started
  //createNewSeparatorFile();

}

//******************************************************************************************
void createNewFile(void) {

  cntFile ++; // Increment the counter of files

  // To name the file we need to know the date : ask the RTC
  timestampForFileName = rtc.now(); // MUST be global!!!!! or it won't update
  fileName = "";                    // Reset the filename
  fileName += "/";                  // To tell it to put in the root folder, absolutely necessary

  char timeStamp[sizeof(timeStampFormat_FileName)]; // We are obliged to do that horror because the method "toString" input parameter is also the output
  strncpy(timeStamp, timeStampFormat_FileName, sizeof(timeStampFormat_FileName));
  fileName += timestampForFileName.toString(timeStamp);

  fileName += "-"; // Add a separator between datetime and filenumber

  char buffer[5];
  sprintf(buffer, "%05d", cntFile); // Making sure the file number is always printed with 5 digits
  //  Serial.println(buffer);
  fileName += String(buffer);

  fileName += ".txt"; // Add the file extension


  if (SD.exists(fileName))
  {
    #ifdef SERIAL_VERBOSE
    Serial.print("Looks like a file already exits on the SD card with that name: ");
    Serial.println(fileName);
    #endif
  }

  // Open the file. Note that only one file can be open at a time,
  // so you have to close this one before opening another.
#ifdef SERIAL_VERBOSE
  Serial.print("Creating the following file on the SD card: ");
  Serial.println(fileName);
#endif
  dataFile = SD.open(fileName, FILE_WRITE);

}

//******************************************************************************************
void changeCPUFrequency(void)
{
  #ifdef SERIAL_VERBOSE
  Serial.println("----------------------------------------");
  Serial.print("Current CPU frequency [MHz]: ");
  Serial.println(getCpuFrequencyMhz());
  Serial.println("Changing...");
  #endif

  setCpuFrequencyMhz(TARGET_CPU_FREQUENCY);

  #ifdef SERIAL_VERBOSE
  Serial.print("Current CPU frequency [MHz]: ");
  Serial.println(getCpuFrequencyMhz());
  Serial.println("----------------------------------------");
  #endif
}


// END OF SCRIPT
