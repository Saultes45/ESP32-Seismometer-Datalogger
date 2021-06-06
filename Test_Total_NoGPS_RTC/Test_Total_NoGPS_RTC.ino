
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

//WATCHDOG    
hw_timer_t * timer = NULL;
const uint8_t wdtTimeout = 3; // Watchdog timeout in [s]

#define TIME_TO_SLEEP  (10u)                           // In [s], time spent in the deep sleep state
#define uS_TO_S_FACTOR  (1000000u)                    // Conversion factor from us to s for the deepsleep
// Battery
#define BATT_VOLTAGE_DIVIDER_FACTOR   2       // [N/A]
#define LOW_BATTERY_THRESHOLD         3.1     // in [V]
#define BATT_PIN                      35      // To detect the Lipo battery remaining charge, GPIO35 on Adafruit ESP32 (35 on dev kit)


// -------------------------- States --------------------------
// State machine
#ifndef STATES_H
  #define STATES_H
  #define STATE_OVERWATCH      (3u)
  #define STATE_SLEEP          (2u)
  #define STATE_LOG            (1u)
  #define STATE_EMPTY          (0u)
#endif /* STATES_H */      
volatile uint8_t currentState = STATE_EMPTY; // Used to store which step we are at, default is state "empty"
volatile uint8_t nextState = STATE_EMPTY; // Used to store which step we are going to do next, default is state "empty"


#define NBR_OVERWATCH_BEFORE_ACTION   50
#define NBR_BUMPS_DETECTED_BEFORE_LOG 100
#define NBR_LOG_BEFORE_ACTION         75

uint8_t cnt_Overwatch       = 0u; // This does NOT need to survive reboots NOR ISR, just global
uint8_t cnt_Log             = 0u; // This does NOT need to survive reboots NOR ISR, just global

//bool RS1DDataAvailable          = true;
uint16_t nbr_bumpDetectedTotal   = 0u; // <KEEP GLOBAL>, max of 50*nbr overwatch
uint8_t nbr_messagesWithBumps   = 0u; // <KEEP GLOBAL>, max of nbr overwatch




//RPI Rx buffer
#define RX_BUFFER_SIZE                  (650u)  //640 actually needed, margin of 10 observed
char    RS1Dmessage[RX_BUFFER_SIZE]; ////Have you seen the sheer size of that!!! Time...to die.
const unsigned long RS1D_DEPILE_TIMEOUT     = 100; // in [ms]

#define GEOPHONE_BAUD_RATE             230400  // Baudrate in [bauds] for serial communication with the Geophone TX??(GPIO_17) RX??(GPIO_16)
//insert complete pins table here

// Use the 2nd (out of 3) hardware serial
#define GeophoneSerial Serial1

const uint8_t RS1D_PWR_PIN_1 = 25u;      // To turn the geophone ON and OFF
const uint8_t RS1D_PWR_PIN_2 = 26u ;     // To turn the geophone ON and OFF

// Bump detection
const int32_t BUMP_THRESHOLD_POS = +100000;           
const int32_t BUMP_THRESHOLD_NEG = -100000;


bool goodMessageReceived_flag  = false; // Set to "bad message" first
int32_t seismometerAcc[50];

uint8_t nbr_bumpDetectedLast = 0u; //  max of 50; // This should be the output of a function and should be local 


//String Formatting
#include <string.h>
#include <stdio.h>

//SD card
#include <SPI.h>
#include <SD.h>
uint16_t  cntLinesInFile  = 0; // Written at the end of a file for check (36,000 < 65,535)
uint32_t  cntFile         = 0; // Counter that counts the files written in the SD card this session (we don't include prvious files), included in the name of the file, can handle 0d to 99999d (need 17 bits)
String    fileName        = "";// Name of the current opened file on the SD card
// LOG
char timeStampFormat_Line[]     = "YYYY_MM_DD__hh_mm_ss"; // naming convention for EACH LINE OF THE FILE logged to the SD card
char timeStampFormat_FileName[] = "YYYY_MM_DD__hh_mm_ss"; // naming convention for EACH FILE NAME created on the SD card
// LOG
#define PIN_CS_SD       33        // Chip Select (ie CS/SS) for SPI for SD card
const char SOM_LOG = '$';       // Start of message indicator, mostly used for heath check (no checksum)
const char FORMAT_SEP = ',';      // Separator between the different files so that the data can be read/parsed by softwares
const uint16_t MAX_LINES_PER_FILES = 40; // Maximum number of lines that we want stored in 1 SD card file. It should be about ...min worth
#define SESSION_SEPARATOR_STRING "----------------------------------"
// SD
File                dataFile;              // Only 1 file can be opened at a certain time, <KEEP GLOBAL>


// RTC
#include <RTClib.h> // use the one from Adafruit, not the forks with the same name
//RTC
RTC_PCF8523         rtc;
DateTime            time_loop;             // MUST be global!!!!! or it won't update
DateTime            timestampForFileName;  // MUST be global!!!!! or it won't update

// -------------------------- Functions declaration --------------------------
void 		  pinSetUp 			      (void);
void 		  checkBatteryLevel 	(void);
void 		  turnRS1DOFF			    (void);
void 		  turnRS1DON			    (void); 
void 		  waitForRS1DWarmUp	  (void);
void      testRTC             (void); 
void 		  logToSDCard			    (void); 
void 		  readRS1DBuffer		  (void);
void 		  parseGeophoneData	  (void);
void      createNewFile       (void);
void      testSDCard          (void);
uint32_t 	hex2dec				      (char * a);
void      blinkAnError        (uint8_t errno);



// -------------------------- ISR ----------------
// Watchdog
void IRAM_ATTR resetModule() {
  ets_printf("Problem! Watchdog trigger: Rebooting...\r\n");
  esp_restart();
}


// -------------------------- Set up --------------------------

void setup() {
  
  Serial.begin(115200);
  Serial.println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%");

  Serial.println("Let's set up the pins");
  // Declare pins
  // ----------------------------------------
  pinSetUp();

   for (uint8_t cnt_fill=0; cnt_fill < 50; cnt_fill++)
  {
    seismometerAcc[cnt_fill] = (int32_t)0;
  }

  testRTC();
  testSDCard();  

  Serial.println("Let's start as an EMPTY state");

  Serial.println("Let's switch to OVERWATCH step");
  currentState = STATE_OVERWATCH;


  // Start the RS1D
  //----------------
  Serial.println("Let's start the RS1D");
  turnRS1DON();
  // Start the HW serial for the Geophone/STM
  // ----------------------------------------
  GeophoneSerial.begin(GEOPHONE_BAUD_RATE);
  GeophoneSerial.setTimeout(100);// Set the timeout to 100 milliseconds (for findUntil)

  waitForRS1DWarmUp();

  Serial.println("The next transmission might be the last transmission to the PC, if you turned verbose OFF");

  // Preaparing the night watch (watchdog)
  //-------------------------------------
  // Remember to hold the door
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * uS_TO_S_FACTOR, false); // set time is in [us]
  // Enable all the ISR later

    Serial.println("And here, we, go, ...");
  // Do not go gentle into that good night

  // Enable all ISRs
  //----------------
  timerAlarmEnable(timer);     // Watchdog
 
}


// -------------------------- Loop --------------------------
void loop() {

//  Serial.println("*******************************************************");
//  Serial.printf("Current state is %d \r\n", currentState);
//  Serial.println("Let's check if we received data from the seismometer");

  readRS1DBuffer();
  //  goodMessageReceived_flag =  true; <DEBUG>
  if (goodMessageReceived_flag) 
  {
    goodMessageReceived_flag = false; // Reset the flag
    //nbr_bumpDetectedLast = (uint8_t)0; //Reset
      switch (currentState) // check which mode we are in
      {
      case STATE_OVERWATCH: //Choose between: Continue, Sleep, Log 
      {
        cnt_Overwatch++;
        
        nbr_bumpDetectedTotal = nbr_bumpDetectedTotal + nbr_bumpDetectedLast;
        
        if (nbr_bumpDetectedLast > 0)
        {
          nbr_messagesWithBumps++;
        }
        
        if (cnt_Overwatch >= NBR_OVERWATCH_BEFORE_ACTION)
        {
          Serial.println("End of the watch");
          Serial.printf("We have reached the desired number of overwatch cycles: %d over %d \r\n", cnt_Overwatch, NBR_OVERWATCH_BEFORE_ACTION);
          Serial.println("Time to make a descision: LOG or SLEEP?");
          
          Serial.printf("We have detected %d bumps (or %d messages) over %d desired\r\n", nbr_bumpDetectedTotal, nbr_messagesWithBumps, NBR_BUMPS_DETECTED_BEFORE_LOG);
          if (nbr_bumpDetectedTotal >= NBR_BUMPS_DETECTED_BEFORE_LOG)
          {
            Serial.println("We reached our number of bumps goal, let's LOG");
            nextState = STATE_LOG;

            // Create a new file
            createNewFile();
            Serial.println("New file created");
          }
          else
          {
            Serial.println("We did NOT reach our number of bumps goal, let's SLEEP");
            nextState = STATE_SLEEP;
          }
        }
        else 
        {
          //Serial.printf("Keep looping on OVERWATCH for %d more iterations\r\n", NBR_OVERWATCH_BEFORE_ACTION - cnt_Overwatch);
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
          Serial.println("We reached our number of messages logged, let's overwatch (or log?)");
          Serial.printf("We have detected %d bumps (or %d messages) during this log cycle of %d messages\r\n", nbr_bumpDetectedTotal, nbr_messagesWithBumps, NBR_LOG_BEFORE_ACTION);
          nextState = STATE_OVERWATCH;

          // Close the file (even if the number of lines per file has not been reached)
          dataFile.close();
          // Reset the line counter
          cntLinesInFile = 0;

          Serial.println("Let's check the battery level, since we are at the end of the LOG");
          checkBatteryLevel();
        }
        else
        {
          Serial.printf("Keep looping on LOG for %d more iterations\r\n", NBR_LOG_BEFORE_ACTION - cnt_Log);
          nextState = STATE_LOG;

          // Trigger the watchdog <DEBUG>
          //delay(4000);
        }

        break;
      }/* [] END OF case STATE_LOG: */
        
      // case STATE_SLEEP:
      // {
      //   Serial.println("State is STATE_SLEEP");
      //   Serial.println("We should never have been here, check your code");
      //   break;
      // } /* [] END OF case STATE_SLEEP: */
        
      default:
      {
        Serial.println("Unknown step type, something went very wrong!");
        Serial.printf("Asked step: %d \r\n", currentState);

        break;        
      }/* [] END OF "unknown" and "other" case */
    } // END of SWITCH

    // Sleep management
    //------------------------------------------  
    if (nextState == STATE_SLEEP)
    {
      Serial.println("Since we did not receive enough bumps during OVERWATCH, we sleep to save battery");
      
      // Switch the RS1D off immediately to save power
      // Note: it will switched off anyway late when, the esp is put to deep sleep
      turnRS1DOFF();

      // Deep sleep
      // -----------
      
      //            // Change the mode
      //            currentState = STATE_SLEEP;
      //            savedCurrState = currentState;
      
      //            // Save the current state in NVM
      //            //savedCurrState = currentState;
      //            // Store the current state to the Preferences
      //            preferences.putUInt("savedCurrState", savedCurrState);
      //            // Close the Preferences
      //            preferences.end();
      
      // Prepare the sleep with all the required parameters
      esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_OFF);
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM,ESP_PD_OPTION_OFF);
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM,ESP_PD_OPTION_OFF);
      // Serial.println("Error = " + String(err)); // Tell the console the error status
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); // Set up deep sleep wakeup timer
      Serial.println("Going to sleep...");
      //delay(1000); // Wait to give time to the serial communication buffer to empty
      Serial.flush(); // Clear serial buffer <Please check because might be a bad idea: maybe sleep clear the buffer>
      
      // Start the deep sleep
      esp_deep_sleep_start(); 

    }
    else // if the next step is NOT a sleep
    {
//      Serial.println("The next state is NOT a SLEEP");

      if (nextState != currentState)
      {
        Serial.println("Looks like we need to change the state");
        Serial.println("Resetting some variables...");
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
  pinMode (LED_BUILTIN , OUTPUT);
  pinMode (BATT_PIN    , INPUT);

  pinMode (RS1D_PWR_PIN_1    , OUTPUT);
  pinMode (RS1D_PWR_PIN_2    , OUTPUT);

  turnRS1DOFF();

}

//******************************************************************************************
void checkBatteryLevel (void)
{
  Serial.println("----------------------------------------");
  Serial.println("Estimating battery level");
  uint16_t rawBattValue = analogRead(BATT_PIN); //read the BATT_PIN pin value,   //12 bits (0 – 4095)
  Serial.print("Raw ADC (0-4095): ");
  Serial.print(rawBattValue);
  Serial.println(" LSB");
  //resultadcEnd(BATT_PIN); // Turn off the ADC for that channel to save power
  //12 bits (0 – 4095)
  float battVoltage    = rawBattValue * (3.30 / 4095.00) * BATT_VOLTAGE_DIVIDER_FACTOR; //convert the value to a true voltage
  Serial.print("Voltage: ");
  Serial.print(battVoltage);
  Serial.println(" V");
  if (battVoltage < LOW_BATTERY_THRESHOLD) // check if the battery is low (i.e. below the threshold)
  {
    Serial.println("Warning: Low battery!");
  }
  else
  {
    Serial.println("Battery level OK");
  }

  Serial.println("----------------------------------------");
  
}

//******************************************************************************************
void turnRS1DOFF(void) {
  
  #ifdef SERIAL_VERBOSE
    Serial.println("Turning RS1D OFF...");
  #endif
  
  digitalWrite(RS1D_PWR_PIN_1, LOW);
  digitalWrite(RS1D_PWR_PIN_2, LOW);
  
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
  digitalWrite(RS1D_PWR_PIN_2, HIGH);
  //digitalWrite(RS1D_PWR_PIN_3, HIGH);
  
  #ifdef SERIAL_VERBOSE
    Serial.println("RS1D is ON");
  #endif

}

//******************************************************************************************
void waitForRS1DWarmUp(void) {

  #ifdef SERIAL_VERBOSE
    Serial.println("RS1D Warmup...");
  #endif

  delay(5000); // Just wait for the sensor to be ready
}

//******************************************************************************************
void logToSDCard(void) {

  // Create a string for assembling the data to log
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
  dataString += FORMAT_SEP; 

  for (uint8_t cnt_Acc = 0; cnt_Acc < 50; cnt_Acc++)
  {
    // Save the results (acceleration is measured in ???)
    dataString += String(seismometerAcc[cnt_Acc]); 
    dataString += FORMAT_SEP;
  }
  Serial.println("Data going to SD card: ");
  Serial.println(dataString);
  Serial.flush();

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
  char      temp             = 0; // Init must be different from SOM_CHAR_SR

  if (GeophoneSerial.available()) 
  {
    if(GeophoneSerial.find("[\""))// test the received buffer for SOM_CHAR_SR
    {
      
      cnt_savedMessage = 0;
      RS1Dmessage[cnt_savedMessage] = '[';
      cnt_savedMessage ++;
      RS1Dmessage[cnt_savedMessage] = '\"';
      cnt_savedMessage ++;

      unsigned long startedWaiting    = millis();

      while((RS1Dmessage[cnt_savedMessage-1] != ']') && (millis() - startedWaiting <= RS1D_DEPILE_TIMEOUT) && (cnt_savedMessage < RX_BUFFER_SIZE))
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
    const uint8_t  max_nbr_extractedCharPerAcc = 9; // scope controlled  + cannot be reassigned
    char    temp1[max_nbr_extractedCharPerAcc];
    uint8_t cnt_accvalues = 0;
    char    *p = RS1Dmessage;
    uint8_t cnt = 0;

    nbr_bumpDetectedLast = 0; //RESET

    for (uint8_t cnt_fill=0; cnt_fill < 50; cnt_fill++)
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
    for (cnt_accvalues = 1; cnt_accvalues < 50; cnt_accvalues++)
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

  Serial.println("Testing the RTC...");

  if (!rtc.begin()) 
  {
    #ifdef SERIAL_VERBOSE
      Serial.println("Couldn't find RTC, is it properly connected?");
      Serial.flush(); // Wait until there all the text for the console have been sent
    #endif
    //blinkAnError(6);
  }

  if (!rtc.initialized() || rtc.lostPower()) 
  {
    #ifdef SERIAL_VERBOSE
      Serial.println("RTC is NOT initialized. Use the NTP sketch to set the time!");
      Serial.println("This is not an important error");
      Serial.println("The datalogger might still be able to function properly");
    #endif
    //blinkAnError(7);
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
void testSDCard(void) {
  Serial.print("Initializing SD card...");
//  String testString = "Test 0123456789";

  // see if the card is present and can be initialized:
  if (!SD.begin(PIN_CS_SD)) {
    Serial.println("Card failed, or not present");
    blinkAnError(2);
    // Don't do anything more: infinite loop just here
    //while (1);
  }
//  Serial.println("Card initialized");
//
//  if (SD.exists("/00_test.txt")) { // The "00_" prefix is to make sure it is displayed by win 10 explorer at the top
//    Serial.println("Looks like a test file already exits on the SD card"); // Just a warning
//  }
//
//  // Create and open the test file. Note that only one file can be open at a time,
//  // so you have to close this one before opening another.
//  File dataFile = SD.open("/00_test.txt", FILE_WRITE);
//
//  // if the file is available, write to it:
//  if (dataFile) {
//    dataFile.println(testString);
//    dataFile.close();
//    // print to the serial port too:
//    Serial.println(testString);
//  }
//  // If the file isn't open, pop up an error:
//  else {
//    Serial.println("Error while opening /00_test.txt");
//    //blinkAnError(3);
//  }

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

// END OF SCRIPT
