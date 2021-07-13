
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
#include "parameters.h"

// -------------------------- ISR ----------------
// Watchdog (IRAM_ATTR)
void  resetModule()
{

unsigned int nbr_WDTTrig;
  
  // Stop the timer
  //----------------
  //  timer(hw_timer_disarm());
  timerAlarmDisable(timer); // You need to be careful of the order: 1st disable the IST, 2nd stop the timer
  timerEnd(timer);
  timer = NULL;
  
  // Disable/Detach interrupts
  //--------------------------
  //detachInterrupt(interrupt);
  noInterrupts();

  //lastWatchdogTrigger = rtc.now(); // <NOT YET USED>

  //nbr_WDTTrig = 12; // <DEBUG> <REMOVE ASAP>
  nbr_WDTTrig = MAX_NBR_WDT; // To make sure the module chooses to sleep


  nbr_WDTTrig++;

  #ifdef SERIAL_VERBOSE
    ets_printf("\r\n**** /!\\ Problem ! /!\\ ****\r\n");
    ets_printf("\r\nWatchdog trigger: reboot or sleep?\r\n");
    //ets_printf("Number of WDT triggers: %d / %d\r\n", nbr_WDTTrig, MAX_NBR_WDT);
  #endif

// normally since we go to sleep we shouldn't need that but 
// there are some wierd slow discharge when we do not do the following
turnRS1DOFF         ();
turnLogOFF          ();
turnGPSOFF          ();


  if (nbr_WDTTrig > MAX_NBR_WDT)
  {
    // Sleep instead of reboot

    int watchdog_recurrent_time_in_s = WDT_SLP_RECUR_S;
    
    #ifdef SERIAL_VERBOSE
      ets_printf("Sleeping for %d s...\r\n", watchdog_recurrent_time_in_s);
    #endif

    // Prepare the sleep with all the required parameters
    esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM,ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM,ESP_PD_OPTION_OFF);
    // Serial.println("Error = " + String(err)); // Tell the console the error status
    esp_sleep_enable_timer_wakeup(watchdog_recurrent_time_in_s * S_TO_US_FACTOR); // Set up deep sleep wakeup timer


    // Start the deep sleep
    esp_deep_sleep_start();
  }
  else
  {
    #ifdef SERIAL_VERBOSE
      ets_printf("Rebooting...\r\n");
    #endif

    esp_restart();

    #ifdef SERIAL_VERBOSE
      ets_printf("You shouldn't see this message\r\n");
    #endif
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


  // Check battery level
  // --------------------
   #ifdef SERIAL_VERBOSE
  Serial.println("Let's check the battery level, since we are boot");
  #endif
  checkBatteryLevel();
  // The file has already been closed in the function logToSDCard


  // Variable reset
  // --------------
  for (uint8_t cnt_fill=0; cnt_fill < 50; cnt_fill++)
  {
    seismometerAcc[cnt_fill] = (int32_t)0;
  }

  // Set up RTC + SD
  // ----------------

//  turnLogON(); // Start the feather datalogger
//  delay(1000); // Wait for the LOG to be ready
//  testRTC();
//  testSDCard();
//  turnLogOFF(); // Turn the feather datalogger OFF, we will go to OVW and we don't need it

  currentState = STATE_OVERWATCH;
  GPSNeeded = false;
  goodMessageReceived_flag = false; // Reset the flag

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
  

#ifdef SERIAL_VERBOSE
  Serial.println("And here, we, go, ...");
  // Do not go gentle into that good night
#endif

  // Enable all ISRs
  //----------------
  #ifdef SERIAL_VERBOSE
  Serial.println("Watchdog ISR ready");
  #endif
  timerAlarmEnable(timer);     // Watchdog

  //delay(wdtTimeout * S_TO_MS_FACTOR); // DEBUG: trigger watchdog at every loop
  //delay(wdtTimeout * S_TO_MS_FACTOR); // DEBUG: trigger watchdog at every loop


}


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
          cnt_Overwatch = 0; // RESET (redundant but security)
          #ifdef SERIAL_VERBOSE
          Serial.println("End of the watch");
          Serial.printf("We have reached the desired number of overwatch cycles: %d over %d \r\n", cnt_Overwatch, NBR_OVERWATCH_BEFORE_ACTION);
          Serial.println("Time to make a descision: LOG or SLEEP?");

          Serial.printf("We have detected %d bumps - %d desired (or %d messages)\r\n", nbr_bumpDetectedTotal, nbr_messagesWithBumps, NBR_BUMPS_DETECTED_BEFORE_LOG);
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
            Serial.println("Done");
            #endif
          }
          else
          {
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
  pinMode (LOG_PWR_PIN_3    , OUTPUT);
  pinMode (LOG_PWR_PIN_4    , OUTPUT);

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

    Serial.printf("Voltage: %f [V]\r\n", battVoltage);
    #endif

    if (battVoltage < LOW_BATTERY_THRESHOLD) // check if the battery is low (i.e. below the threshold)
    {
      #ifdef SERIAL_VERBOSE
      Serial.println("Warning: Low battery!");
      #endif
      // Force the board to sleep by triggering an error by 
      //   using the blocking function: blinkAnError
      blinkAnError(1);
    }
    else
    {
      #ifdef SERIAL_VERBOSE
      Serial.println("Battery level OK");
      Serial.println("----------------------------------------");
      #endif
    }
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
  digitalWrite(LOG_PWR_PIN_3, LOW);
  digitalWrite(LOG_PWR_PIN_4, LOW);

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
  digitalWrite(LOG_PWR_PIN_3, HIGH);
  digitalWrite(LOG_PWR_PIN_4, HIGH);


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

  #ifdef USE_GPS

  #ifdef SERIAL_VERBOSE
  Serial.print("Are we going to use the GPS or the RTC for the TIMESTAMP?...");
  #endif

  //noFixGPS = false; //<DEBUG> <REMOVE ME>
  
  if (noFixGPS) // Then use RTC
  {

    #ifdef SERIAL_VERBOSE
    Serial.println("RTC it is");
    #endif
    
     // Read the RTC time
    //------------------------------------------
    time_loop = rtc.now(); // MUST be global!!!!! or it won't update
    // We are obliged to do that horror because the method "toString" input parameter is also the output
    char timeStamp[sizeof(timeStampFormat_Line)];
    strncpy(timeStamp, timeStampFormat_Line, sizeof(timeStampFormat_Line));
    dataString += time_loop.toString(timeStamp);
    dataString += ".000"; // add the [ms] placeholder 
    
  }
  else // Then use GPS
  {
    #ifdef SERIAL_VERBOSE
    Serial.println("GPS it is");
    Serial.print("Is there any GPS messages that has been parsed?...");
    #endif

    if (GPSTimeAvailable)
    {
      #ifdef SERIAL_VERBOSE
      Serial.println("Yes there is");
      #endif
      dataString = lastGPSTimestamp;
    }
    else
    {
      #ifdef SERIAL_VERBOSE
      Serial.println("Nope :(");
      Serial.println("Using a random date");
      #endif
      dataString += "2011_11_11__11_11_11.100"; // <DEBUG>
    }
      // Reset the GPS messages: a GPS message can only be used once!
      lastGPSTimestamp = "";
      GPSTimeAvailable = false; // <REDUNDANT with top of the loop>
  }
  #else

    // Read the RTC time
    //------------------------------------------
    time_loop = rtc.now(); // MUST be global!!!!! or it won't update
    // We are obliged to do that horror because the method "toString" input parameter is also the output
    char timeStamp[sizeof(timeStampFormat_Line)];
    strncpy(timeStamp, timeStampFormat_Line, sizeof(timeStampFormat_Line));
    dataString += time_loop.toString(timeStamp);
    dataString += ".000"; // add the [ms] placeholder 

  #endif


 
  dataString += FORMAT_SEP;

  Serial.print("Filename: ");
  Serial.println(fileName);

  // Write the accelerations in the message
  //------------------------------------------
  for (uint8_t cnt_Acc = 0; cnt_Acc < NBR_ACCELERATIONS_PER_MESSAGE -1 ; cnt_Acc++)
  {
    // Save the results (acceleration is measured in ???)
    dataString += String(seismometerAcc[cnt_Acc]);
    dataString += FORMAT_SEP;
  }
  // Write the last value of the accelerometer w/out a FORMAT_SEP
  dataString += String(seismometerAcc[NBR_ACCELERATIONS_PER_MESSAGE-1]);

#ifdef SERIAL_VERBOSE
  Serial.print("Data going to SD card: ");
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
      // Write last acceleration
      dataFile.println(dataString);
      // Write the battery voltage to the file w/out the "\r\n"
      dataFile.print(String(analogRead(BATT_PIN) * (3.30 / 4095.00) * BATT_VOLTAGE_DIVIDER_FACTOR));
      // Close the file
      dataFile.close();
      // Reset the line counter
      cntLinesInFile = 0;

      /* The next commented out lines are removed since 1 LOG step = 1 file
      // Create a new file
      createNewFile();
      
      #ifdef SERIAL_VERBOSE
      Serial.println("Reached the max number of lines per file, starting a new one");
      #endif
      */

      
      // Limit back the frequency of the CPU to consume less power
      // setCpuFrequencyMhz(TARGET_CPU_FREQUENCY);
    }
    else // We are still under the limit of number of lines per file
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
    Serial.print("Error writting to the following file: ");
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

  nbr_bumpDetectedLast = 0; //RESET

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

    // Parse the aquired message to get 1st acceleration (1st is different)
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
  Serial.println("RTC is good to go...");

}


//******************************************************************************************
void blinkAnError(uint8_t errno) {  // Use an on-board LED (the red one close to the micro USB connector, left of the enclosure) to signal errors (RTC/SD)

  //errno argument tells how many blinks per period to do. Must be  strictly less than 10

/* NOTICE: 
 *  There should be ASOLUTELY NO watchdog feed in that function
 */

  while(1) { // Infinite loop: stay here until power cycle or watchdog
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
    blinkAnError(2); // This is going to get caught by the WDT
  }

#ifdef SERIAL_VERBOSE
  Serial.println("Card initialized");
#endif

  // Add a separator file (empty but with nice title) so the user knows a new DAQ session started
  //createNewSeparatorFile();

}

//******************************************************************************************
void createNewFile(void) {

  /* NOTICE: 
      Normally, this variable should be LOCAL: timestampForFileName
      However, MUST be global!!!!! or it won't update

      List of global variable modified:
      -cntFile
      -fileName
      -timestampForFileName
      -dataFile
  */
  
  #ifdef SERIAL_VERBOSE
  Serial.println("Creating a new file on SD...");
  #endif

  cntFile ++; // Increment the counter of files

  timestampForFileName = ""; // Reset of this global variable

  // To name the file we need to know the date : ask the GPS or the RTC
  
  fileName = "";                    // Reset the filename
  fileName += "/";                  // To tell it to put in the root folder, absolutely necessary

  #ifdef SERIAL_VERBOSE
  Serial.print("Starting to build a new file name: ");
  Serial.println(fileName);
  #endif



  #ifdef USE_GPS

  #ifdef SERIAL_VERBOSE
  Serial.print("Are we going to use the GPS or the RTC for the FILENAME?...");
  #endif
  
  if (noFixGPS) // Then use RTC
  {

    #ifdef SERIAL_VERBOSE
    Serial.println("RTC it is");
    #endif
    
    // Read the RTC time
    //------------------------------------------
    timestampForFileName = rtc.now(); // MUST be global!!!!! or it won't update
    char timeStamp[sizeof(timeStampFormat_FileName)]; // We are obliged to do that horror because the method "toString" input parameter is also the output
    strncpy(timeStamp, timeStampFormat_FileName, sizeof(timeStampFormat_FileName));
    fileName += timestampForFileName.toString(timeStamp);
  }
  else // Then use GPS for the file name
  {

    #ifdef SERIAL_VERBOSE
    Serial.println("GPS it is");
    #endif
    
    // Read the GPS time
    //------------------
    
    // 2 securities: internal: timeout AND external: watchdog
    unsigned long startedWaiting = millis();
    unsigned int cnt_whileLoop = 0;
    Serial.print((millis() - startedWaiting));
    Serial.print(" < ");
    Serial.print(GPS_NEW_FILE_TIMEOUT_MS);
    Serial.println(" ?");
    Serial.printf("Looping %d\r\n", cnt_whileLoop);
  
    // Big waiting loop
    // -----------------
    while (((millis() - startedWaiting) <= GPS_NEW_FILE_TIMEOUT_MS))
    {
      //      // < DEBUG > <REMOVE WHEN FINISHED>
      //      cnt_whileLoop ++;
      //      Serial.print((millis() - startedWaiting));
      //      Serial.print(" < ");
      //      Serial.print(GPS_NEW_FILE_TIMEOUT_MS);
      //      Serial.println(" ?");
      //      Serial.printf("Looping %d\r\n", cnt_whileLoop);
      //      Serial.printf("Still waiting for fix? %d\r\n", noFixGPS);
        
      // Reset the timer (i.e. feed the watchdog)
      //------------------------------------------
      timerWrite(timer, 0); // need to be before a potential sleep
        
      // read data from the GPS in the 'main loop'
      char c = GPS.read();
    
      // if a sentence is received, we can check the checksum, parse it...
      if (GPS.newNMEAreceived()) 
      {
        // <REMOVED  CHECK>
        //if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
        //return; // we can fail to parse a sentence in which case we should just wait for another
  
        if(not(GPS.fix)) // Display stats and keep looping in the while loop
        {
          #ifdef SERIAL_VERBOSE
          Serial.print("Still no fix, keep looping for: ");
          Serial.print(GPS_NO_FIX_TIMEOUT_MS - (millis() - startedWaiting));
          Serial.println(" [ms]");
          Serial.print("Fix: ");
          Serial.print((int)GPS.fix);
          Serial.print(" quality: ");
          Serial.println((int)GPS.fixquality);
          #endif
        }
        else // We have a fix! Yay!
        {
           
        // Read the GPS time
        //-------------------
        
          fileName += String(2000 + GPS.year);
          fileName += "_";
          if (GPS.month < 10) { fileName += "0"; } 
          fileName += String(GPS.month);
          fileName += "_";
          if (GPS.day < 10) { fileName += "0"; } 
          fileName += String(GPS.day);
          fileName += "__";
          if (GPS.hour < 10) { fileName += "0"; } 
          fileName += String(GPS.hour);
          fileName += "_";
          if (GPS.hour < 10) { fileName += "0"; } 
          fileName += String(GPS.minute);
          fileName += "_";
          if (GPS.hour < 10) { fileName += "0"; } 
          fileName += String(GPS.seconds);
          fileName += ".";
          fileName += String(GPS.milliseconds);
  
        }
  
        // Reset the timer (i.e. feed the watchdog)
        //------------------------------------------
        timerWrite(timer, 0); // need to be before a potential sleep
      }
    } // END OF WHILE wait for GPS

    // TODO#1: if we have a GPS timeout here, we should revert back to the RTC. Add code
    fileName += "2011_11_11__11_11_11"; // <DEBUG> <PLACEHOLDER>
  }
  #else
    // Read the RTC time
    //------------------
    timestampForFileName = rtc.now(); // MUST be global!!!!! or it won't update
    char timeStamp[sizeof(timeStampFormat_FileName)]; // We are obliged to do that horror because the method "toString" input parameter is also the output
    strncpy(timeStamp, timeStampFormat_FileName, sizeof(timeStampFormat_FileName));
    fileName += timestampForFileName.toString(timeStamp);
  #endif

  #ifdef SERIAL_VERBOSE
  Serial.print("Current file name: ");
  Serial.println(fileName);
  #endif

  
  
  fileName += "-"; // Add a separator between datetime and filenumber

  char buffer[5];
  sprintf(buffer, "%05d", cntFile); // Making sure the file number is always printed with 5 digits
  //  Serial.println(buffer);
  fileName += String(buffer);

  fileName += ".txt"; // Add the file extension

   #ifdef SERIAL_VERBOSE
  Serial.print("Current file name: ");
  Serial.println(fileName);
  #endif


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

   #ifdef SERIAL_VERBOSE
  Serial.print("File name created successfully: ");
  Serial.println(fileName);
  #endif

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

//******************************************************************************************
void prepareWDT(void)
 {
  // Remember to hold the door
  #ifdef SERIAL_VERBOSE
  Serial.print("Preparing watchdog, timeout is: ");
  Serial.print(wdtTimeout);
  Serial.println(" [s]");
  #endif
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * S_TO_US_FACTOR, false); // set time is in [us]
  // The ISR is enabled later (@ the end of the "set up") but timer has already started
  #ifdef SERIAL_VERBOSE
  Serial.println("Watchdog timer started, no ISR yet");
  #endif
 }

//******************************************************************************************
#ifdef USE_GPS
void testGPS(void)
{

  #ifdef SERIAL_VERBOSE
  Serial.println("Starting to address the GPS...");
  #endif
  
  GPS.begin(0x10);  // The I2C address to use is 0x10

  #ifdef SERIAL_VERBOSE
  Serial.println("Changing GPS settings...");
  #endif
  // Turn OFF all GPS output
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_OFF);

  //  GPS.flush();

  // Set up communication
  // ---------------------
  GPS.sendCommand(PMTK_SET_BAUD_115200); // Ask the GPS to send us data as 115200
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ); // 10 Hz update rate (message only)
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);  // Can't fix position faster than 5 times a second


  // Just once, request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);
//  delay(1000);
  //  Just once, request firmware version
  GPS.println(PMTK_Q_RELEASE);
//  delay(1000);

  // Turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);

  #ifdef SERIAL_VERBOSE
  Serial.println("GPS set up done");
  #endif
}
#endif

//******************************************************************************************
#ifdef USE_GPS
void waitForGPSFix(void)
{

  // Reset form global flags (probably not needed)
  //GPSTimeAvailable = false;
  //noFixGPS  = true; // Init // Already set lower in that function
  // GPSNeeded is set if necessary outside this function
  
  
  #ifdef SERIAL_VERBOSE
  Serial.println("Starting to wait for GPS fix");
  Serial.print("Timeout is: ");
  Serial.print(GPS_NO_FIX_TIMEOUT_MS);
  Serial.println(" [ms] or until watchdog triggers IF stuck");
  #endif
  

  // 2 securities: internal: timeout AND external: watchdog
  unsigned long startedWaiting = millis();

  noFixGPS  = true; // Init

  // < DEBUG > <REMOVE WHEN FINISHED>
  unsigned int cnt_whileLoop = 0;
  Serial.print((millis() - startedWaiting));
  Serial.print(" < ");
  Serial.print(GPS_NO_FIX_TIMEOUT_MS);
  Serial.println(" ?");
  Serial.printf("Looping %d\r\n", cnt_whileLoop);

  // Big waiting loop
  // -----------------
  while (noFixGPS && ((millis() - startedWaiting) <= GPS_NO_FIX_TIMEOUT_MS))
  {
//      // < DEBUG > <REMOVE WHEN FINISHED>
//      cnt_whileLoop ++;
//      Serial.print((millis() - startedWaiting));
//      Serial.print(" < ");
//      Serial.print(GPS_NO_FIX_TIMEOUT_MS);
//      Serial.println(" ?");
//      Serial.printf("Looping %d\r\n", cnt_whileLoop);
//      Serial.printf("Still waiting for fix? %d\r\n", noFixGPS);
      
      // Reset the timer (i.e. feed the watchdog)
      //------------------------------------------
      timerWrite(timer, 0); // need to be before a potential sleep
      
    // read data from the GPS in the 'main loop'
    char c = GPS.read();
  
    // if a sentence is received, we can check the checksum, parse it...
    if (GPS.newNMEAreceived()) {
  
  
      if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  
      if(not(GPS.fix))
      {
        #ifdef SERIAL_VERBOSE
        Serial.print("Still no fix, keep looping for: ");
        Serial.print(GPS_NO_FIX_TIMEOUT_MS - (millis() - startedWaiting));
        Serial.println(" [s]");
        Serial.print("Fix: ");
        Serial.print((int)GPS.fix);
        Serial.print(" quality: ");
        Serial.println((int)GPS.fixquality);
        #endif
      }
      else
      {
        noFixGPS = false; // We have a fix! Yay!
      }

      // Reset the timer (i.e. feed the watchdog)
      //------------------------------------------
      timerWrite(timer, 0); // need to be before a potential sleep
  }
  }
  
        #ifdef SERIAL_VERBOSE
        Serial.println("Done waiting for GPS fix: FIX or TIMEOUT?...");
        Serial.print("Fix: ");
        Serial.print((int)GPS.fix);
        Serial.print(" quality: ");
        Serial.println((int)GPS.fixquality);
        #endif

        if (noFixGPS)
        {
          // GPS cannot give us a fix so we turn it off and use the RTC
          turnGPSOFF();
        }
}
#endif


//******************************************************************************************
#ifdef USE_GPS
void getGPSTime(void)
{

   lastGPSTimestamp = "";
   

  // read data from the GPS in the 'main loop'
  char c = GPS.read();

  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) 
  {


  if (GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
  {
    if(GPS.fix)
    {
    // Read the GPS time
    //-------------------
    
    lastGPSTimestamp += String(2000 + GPS.year);
    lastGPSTimestamp += "_";
    if (GPS.month < 10) { lastGPSTimestamp += "0"; } 
    lastGPSTimestamp += String(GPS.month);
    lastGPSTimestamp += "_";
    if (GPS.day < 10) { lastGPSTimestamp += "0"; } 
    lastGPSTimestamp += String(GPS.day);
    lastGPSTimestamp += "__";
    if (GPS.hour < 10) { lastGPSTimestamp += "0"; } 
    lastGPSTimestamp += String(GPS.hour);
    lastGPSTimestamp += "_";
    if (GPS.hour < 10) { lastGPSTimestamp += "0"; } 
    lastGPSTimestamp += String(GPS.minute);
    lastGPSTimestamp += "_";
    if (GPS.hour < 10) { lastGPSTimestamp += "0"; } 
    lastGPSTimestamp += String(GPS.seconds);
    lastGPSTimestamp += ".";

    // if (GPS.milliseconds < 10) 
    // {
    //   lastGPSTimestamp += "00";
    // } 
    // else if (GPS.milliseconds > 9 && GPS.milliseconds < 100) 
    // {
    //   lastGPSTimestamp += "0";
    // }
    lastGPSTimestamp += String(GPS.milliseconds);
    GPSTimeAvailable = true; // Indicates we can use the lastGPSTimestamp

      #ifdef SERIAL_VERBOSE
      Serial.print("Timestamp: ");
      Serial.println(lastGPSTimestamp);
      #endif
    }
    }
  }
}  
#endif



// END OF SCRIPT
