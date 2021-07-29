
/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : N/A
* Creation date : 2021-06-03 (graduation+Queen's b-day)
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

// RTC
#include <RTClib.h> // use the one from Adafruit, not the forks with the same name

//WATCHDOG    
hw_timer_t * timer = NULL;
const uint8_t wdtTimeout = 3; // Watchdog timeout in [s]

#define TIME_TO_SLEEP  (5u)                           // In [s], time spent in the deep sleep state
#define uS_TO_S_FACTOR  (1000000u)                    // Conversion factor from us to s for the deepsleep

#define RS1D_PWR_PIN_1                      25      // To turn the geophone ON and OFF
#define RS1D_PWR_PIN_2                      26      // To turn the geophone ON and OFF

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


#define NBR_OVERWATCH_BEFORE_ACTION   10
#define NBR_BUMPS_DETECTED_BEFORE_LOG 5
#define NBR_LOG_BEFORE_ACTION         6

uint8_t cnt_Overwatch       = 0u; // This does NOT need to survive reboots NOR ISR, just global
uint8_t cnt_Log             = 0u; // This does NOT need to survive reboots NOR ISR, just global

bool RS1DDataAvailable          = true;
bool GoodMessageReceived_flag   = false; //<Probably can be LOCAL>
uint16_t nbr_bumpDetectedTotal   = 0u; // <KEEP GLOBAL>, max of 50*nbr overwatch
uint8_t nbr_messagesWithBumps   = 0u; // <KEEP GLOBAL>, max of nbr overwatch

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
  pinSetUp();

  Serial.println("Let's start as an EMPTY state");
  Serial.println("By default, the global variable holding the state starts as STATE_EMPTY");
  Serial.println("Now we do the initialisation and tests");

  Serial.println("We either come from a deep sleep of we just started");
  Serial.println("Let's switch to OVERWATCH step");
  currentState = STATE_OVERWATCH;

  // Starting the night watch (watchdog)
  //-------------------------------------
  // Remember to hold the door
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * uS_TO_S_FACTOR, false); // set time is in [us]
  // Enable all the ISR later

  // Start the RS1D
  //----------------
  Serial.println("Let's start the RS1D");
  turnRS1DON();

  Serial.println("RS1D warm up...");

  // Enable all ISRs
  //----------------
  timerAlarmEnable(timer);     // Watchdog <DEBUG>
 
}



void loop() {

  Serial.println("*******************************************************");
  Serial.printf("Current state is %d \r\n", currentState);
  Serial.println("Let's check if we received data from the seismometer");

  //  RS1DDataAvailable =  true; <DEBUG>
  if (RS1DDataAvailable) 
  {
    Serial.println("Depile the Serial1 buffer as fast as possible + save to a char array");

    Serial.println("Check if we recieved at least 1 complete message");
    
    GoodMessageReceived_flag =  true; //<DEBUG>
    if (GoodMessageReceived_flag) 
    {
      GoodMessageReceived_flag = false; //Reset the flag
      Serial.println("Parsing the data, hex2dec, detect bumps");
      Serial.println("This is going to take some time...");
      //Serial.println("We do that only if the state is OVERWATCH or LOG");

      Serial.println("The parsing function is going to tell us if AT LEAST 1 bump has been detected in the 50 accelerations logged");
      uint8_t nbr_bumpDetectedLast = 1u; //  max of 50; // This should be the output of a function and should be local 

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
            Serial.printf("We have done %d overwatch cycles over %d desired\r\n", cnt_Overwatch, NBR_OVERWATCH_BEFORE_ACTION);
            Serial.println("We have reached the desired number of overwatch cycles");
            Serial.println("Time to make a descision: LOG or SLEEP?");
            
            Serial.printf("We have detected %d overwatch cycles over %d desired\r\n", nbr_bumpDetectedTotal, NBR_BUMPS_DETECTED_BEFORE_LOG);
            if (nbr_bumpDetectedTotal >= NBR_BUMPS_DETECTED_BEFORE_LOG)
            {
              Serial.println("We reached our number of bumps goal, let's LOG");
              nextState = STATE_LOG;
            }
            else
            {
              Serial.println("We did NOT reach our number of bumps goal, let's SLEEP");
              nextState = STATE_SLEEP;
            }
          }
          else 
          {
            Serial.printf("Keep looping on OVERWATCH for %d more iterations\r\n", NBR_OVERWATCH_BEFORE_ACTION - cnt_Overwatch);
            nextState = STATE_OVERWATCH;
          }
          
          
          break;
        }/* [] END OF case STATE_OVERWATCH: */
        
      case STATE_LOG:
        {
          Serial.println("We are logging, we dont care about the bumps, are we?");
          cnt_Log ++;
          
          if (cnt_Log >= NBR_LOG_BEFORE_ACTION)
          {
            Serial.println("We reached our number of messages logged, let's overwatch (or log?)");
            nextState = STATE_OVERWATCH;

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
        
      case STATE_SLEEP: //Choose between: Continue, Sleep, Log 
        {
          Serial.println("State is STATE_SLEEP");
          Serial.println("We should never have been here, check your code");
          break;
        } /* [] END OF case STATE_SLEEP: */
        
      default:
        {
          Serial.println("Unknown step type, something went very wrong!");
          Serial.printf("Asked step: %d \r\n", currentState);

          break;        
        }/* [] END OF case problem: */
        
        
      } // END of SWITCH


      // Reset the timer (i.e. feed the watchdog)
      //------------------------------------------  
      timerWrite(timer, 0); // need to be before a potential sleep


      if (nextState == STATE_SLEEP)
      {
        Serial.println("Since we did not receive enough bumps during OVERWATCH, we sleep to save battery");
        


        //      currentState  = STATE_OVERWATCH; // <DEBUG>
        //      nextState     = STATE_OVERWATCH; // <DEBUG>

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
        delay(1000); // Wait to give time to the serial communication buffer to empty
        Serial.flush(); // Clear serial buffer <Please check because might be a bad idea: maybe sleep clear the buffer>
        
        // Start the deep sleep
        esp_deep_sleep_start(); 


      }/* [] END OF if STATE_SLEEP: */
      else
      {
        Serial.println("The next state is NOT a SLEEP");

        if (nextState != currentState)
        {
          Serial.println("Looks like we need to change the state");
          Serial.println("Resetting some variables");
          nbr_bumpDetectedTotal   = 0;
          nbr_bumpDetectedLast    = 0;
          nbr_messagesWithBumps   = 0;
          cnt_Overwatch           = 0;
          cnt_Log                 = 0;  
          
          currentState = nextState; // For next loop  
          
        }
        else
        {
          Serial.println("Just keep on looping");
        } 
      }
    }
    else
    {
      Serial.println("We did receive some data but bad one so we just do like we didn't see it");
    }
  } 
  else 
  {
    Serial.println("Looks like we didn't receive any data this round, continue looping");
  }

  delay(500); // <DEBUG>


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
}

//******************************************************************************************
void turnRS1DOFF(void) {
  
  #ifdef SERIAL_VERBOSE
    Serial.println("Turning RS1D OFF...");
  #endif
  
  digitalWrite(RS1D_PWR_PIN_1, LOW);
  digitalWrite(RS1D_PWR_PIN_2, LOW);
  //digitalWrite(RS1D_PWR_PIN_3, LOW);
  
  #ifdef SERIAL_VERBOSE
    Serial.println("RS1D is OFF...");
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
    Serial.println("RS1D is ON...");
  #endif

}
