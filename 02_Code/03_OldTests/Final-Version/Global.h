/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : Nathanaël Esnault
* Creation date : 2021-07-26
* Version       : 0.1 (finished on 2021-..-..)
* Modifications :
* Known bugs    :
*
*
* Possible Improvements
*
* Notes
*
*
* Ressources (Boards + Libraries Manager)
*
*
* TODO
*
* ========================================
*/


// -------------------------- Includes --------------------------

// Personal libraries
//--------------------
#include "RS1D.h"
#include "Battery.h"
#include "StateMachine.h"


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



// -------------------------- Defines --------------------------
// General

// -------------------------- ISR [1]----------------
// Watchdog
void IRAM_ATTR resetModule() 
{
  nbr_WatchdogTrigger ++; // For statistics
  lastWatchdogTrigger = rtc.now();
  
  #ifdef SERIAL_VERBOSE
    ets_printf("Problem! Watchdog trigger: reboot or sleep?\r\n");
  #endif
  
  if (nbr_WatchdogTrigger > 10)
  {
    // Sleep instead of reboot
    
    #ifdef SERIAL_VERBOSE
      ets_printf("Sleeping...\r\n");
    #endif
    
    const uint64_t watchdog_recurrent_time_in_us = 1800; // 0.5h
    
    // Prepare the sleep with all the required parameters
    esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM,ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM,ESP_PD_OPTION_OFF);
    // Serial.println("Error = " + String(err)); // Tell the console the error status
    esp_sleep_enable_timer_wakeup(watchdog_recurrent_time_in_us * uS_TO_S_FACTOR); // Set up deep sleep wakeup timer
    
    
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


// -------------------------- Functions declaration [15]--------------------------
void      pinSetUp            (void);
void      checkBatteryLevel   (void);
void      turnRS1DOFF         (void);
void      turnRS1DON          (void);
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
void      startGPS            (void); // prepare the GPS for logging (baudrate, messages, update rate,  etc)


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

  delay(rs1dWarmUpTime); // Just wait for the sensor to be ready
}

//******************************************************************************************
void logToSDCard(void) 
{

  // Create a string for assembling the data to log
  //-----------------------------------------------
  String dataString  = "";

  // Add start of message character
  //--------------------------------
  dataString += SOM_LOG;

  #ifndef USE_GPS_FOR_TIME

    String myTimestamp = "";

    // Read the RTC time (up to [s] only)
    //------------------------------------------
    time_loop = rtc.now(); // MUST be global!!!!! or it won't update
    // We are obliged to do that horror because the method "toString" input parameter is also the output
    char timeStamp[sizeof(timeStampFormat_Line)];
    strncpy(timeStamp, timeStampFormat_Line, sizeof(timeStampFormat_Line));
    dataString += time_loop.toString(timeStamp);
    
  #else
    // Read the GPS time (up to deciseconds)
    //--------------------------------------
    
    dataString += String(2000 + GPS.year);
    dataString += "_";
    if (GPS.month < 10) { dataString += "0"; } 
    dataString += String(GPS.month);
    dataString += "_";
    if (GPS.day < 10) { dataString += "0"; } 
    dataString += String(GPS.day);
    dataString += "__";
    if (GPS.hour < 10) { dataString += "0"; } 
    dataString += String(GPS.hour);
    dataString += "_";
    if (GPS.hour < 10) { dataString += "0"; } 
    dataString += String(GPS.minute);
    dataString += "_";
    if (GPS.hour < 10) { dataString += "0"; } 
    dataString += String(GPS.seconds);
    dataString += ".";

    if (GPS.milliseconds < 10) 
    {
      dataString += "00";
    } 
    else if (GPS.milliseconds > 9 && GPS.milliseconds < 100) 
    {
      dataString += "0";
    }
    dataString += String(GPS.milliseconds);

  #endif

  dataString += FORMAT_SEP;

  // Write the accelerations in the message
  //------------------------------------------
  for (uint8_t cnt_Acc = 0; cnt_Acc < 50; cnt_Acc++)
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
    const uint8_t  max_nbr_extractedCharPerAcc  = 9; // scope controlled  + cannot be reassigned
    const uint8_t  nbr_accelerationPerMessage   = 50; // scope controlled  + cannot be reassigned
    
    char    temp1[max_nbr_extractedCharPerAcc];
    uint8_t cnt_accvalues = 0;
    char    *p = RS1Dmessage;
    uint8_t cnt = 0;

    nbr_bumpDetectedLast = 0; //RESET

    for (uint8_t cnt_fill=0; cnt_fill < nbr_accelerationPerMessage; cnt_fill++)
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
    for (cnt_accvalues = 1; cnt_accvalues < nbr_accelerationPerMessage; cnt_accvalues++)
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
}// END OF FUNCTION

//******************************************************************************************
void startGPS(void) // prepare the GPS for logging (baudrate, messages, update rate,  etc)
{
  #ifdef SERIAL_VERBOSE
  Serial.println("I2C GPS test");
  #endif

  GPS.begin(0x10);  // The I2C address of this GPS model is the default 0x10


  // Turn OFF all GPS output
  // ---------------------
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_OFF);

  // Set up communication
  // ---------------------
  GPS.sendCommand(PMTK_SET_BAUD_115200); // Ask the GPS to send us data as 115200
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ); // 10 Hz update rate (message only)
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);  // Can't fix position faster than 5 times a second

  // Just once, request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);
//  delay(1000);
//  #ifdef SERIAL_VERBOSE
//    while (GPS.available())
//    {
//      Serial.print(GPS.read());
//    }
//  #endif

  //  Just once, request firmware version, comment out to keep quiet
  GPS.println(PMTK_Q_RELEASE);
//  delay(1000);
//    #ifdef SERIAL_VERBOSE
//    while (GPS.available())
//    {
//      Serial.print(GPS.read());
//    }
//  #endif

  // Turn on RMC (recommended minimum)
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);

  #ifdef SERIAL_VERBOSE
  Serial.println("GPS set up done");
  #endif

}// END OF FUNCTION

//******************************************************************************************
void waitUntilGPSFix(void) // prepare the GPS for logging (baudrate, messages, update rate,  etc)
{
  // if (not(GPS)) // Security: if the GPS object has not been intentiated then trigger the WDT by waiting
  // {
  //   delay(wdtTimeout * 1000); // Conversion from [s] to [ms]
  //   #ifdef SERIAL_VERBOSE
  //     Serial.println("You should never see this");
  //   #endif 
  //   return; // Normally not necessary

  // }
  // else
  // {
    // Then we know the GPS object has been created

    #ifdef SERIAL_VERBOSE
    Serial.println("Let's wait until we have a fix");
    #endif

    do // Keep looping until we have a fix
    {

      // Reset the timer (i.e. feed the watchdog)
      //------------------------------------------  
      timerWrite(timer, 0);
      //delay(3000); // DEBUG: Trigger the watchdog

      char c = GPS.read(); // WE ABSOLUTELY NEED THAT!!
    
      // if a sentence is received, we can check the checksum, parse it...
      if (GPS.newNMEAreceived()) 
      {
        //  #ifdef SERIAL_VERBOSE
        //    Serial.println(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
        //  #endif
        
        if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
        {
          //return; // Failed to parse a sentence, waiting for another
        }
        else
        {
          #ifdef SERIAL_VERBOSE
          Serial.print("Fix: "); Serial.print((int)GPS.fix);
          Serial.print(" quality: "); Serial.println((int)GPS.fixquality);
          Serial.println(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
          #endif
        }
      }
    } while(not(GPS.fix));

    #ifdef SERIAL_VERBOSE
    Serial.println("We have a fix");
    #endif

  // }


} // END OF FUNCTION


// END OF THE FILE