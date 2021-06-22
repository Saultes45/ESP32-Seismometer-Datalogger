


// -------------------------- Includes --------------------------

#define SERIAL_VERBOSE

//ESP32
#include <esp_system.h>
//#include <rom/rtc.h> // reset reason


//String Formatting
#include <string.h>
#include <stdio.h>

//SD card
#include <SPI.h>
#include <SD.h>

//LED
#include <Arduino.h>
#include <Ticker.h>

// Battery
#define BATT_VOLTAGE_DIVIDER_FACTOR   2       // [N/A]
#define LOW_BATTERY_THRESHOLD         3.1     // in [V]
#define BATT_PIN                      35      // To detect the Lipo battery remaining charge, GPIO35 on Adafruit ESP32 (35 on dev kit)

#define TIME_TO_SLEEP               10 // in ms for the loop
// LOG+SD
#define PIN_CS_SD                   33     // Chip Select (ie CS/SS) for SPI for SD card

const uint8_t     LOG_PWR_PIN_1        = 25;    // To turn the geophone ON and OFF
const uint8_t     LOG_PWR_PIN_2        = 26;    // To turn the geophone ON and OFF

String      fileName              = "";           // Name of the current opened file on the SD card
File        dataFile;                               // Only 1 file can be opened at a certain time, <KEEP GLOBAL>


float blinkerPace = 0.1;  //seconds
const float togglePeriod = 5; //seconds

void blink() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}


void setup() {
  
#ifdef SERIAL_VERBOSE
  Serial.begin(115200);
  Serial.println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"); // Indicates a wakeup to the console
#endif


  // LED pins
  //--------------
  pinMode (LED_BUILTIN , OUTPUT);

    // Analog pins
  //--------------
  pinMode (BATT_PIN    , INPUT);

    // Initial pin state
  //-------------------
  digitalWrite(LED_BUILTIN, LOW);

//LED
  blinker.attach(blinkerPace, blink);

Create a file 

declare battery pin

}

void loop() {
  // put your main code here, to run repeatedly:

}
