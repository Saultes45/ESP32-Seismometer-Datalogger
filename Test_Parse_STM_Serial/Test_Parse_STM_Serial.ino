
/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : N/A
* Creation date : 2021-05-27 (red moon eclipse)
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
*
*
*
*
*
*
*https://stackoverflow.com/questions/60094545/esp32-softwareserial-livrary <---- This is not true for feather ESP32 (tested personally)
The ESP32 has 3 different Serial Ports (UART). You can just use one of them:

Serial0: RX0 on GPIO3, TX0 on GPIO1
Serial1: RX1 on GPIO9, TX1 on GPIO10 (+CTS1 and RTS1) <---- This is not true for feather ESP32 (tested personally)
Serial2: RX2 on GPIO16, TX2 on GPIO17 (+CTS2 and RTS2) <---- This is not true for feather ESP32 (tested personally)

You don't need the Software Serial Port, since the ESP32 can unconfigurate internally the Serial port pin to other pins.

To do that, you need to use the <HardwareSerial.h> - library


Code+Functions to write (no RTC yet)
------------------------------------
PPS GPS ISR (to get ms)
Parse GPS NMEA/I2C data to extract time (up to the second)
Sleepmode --
Watchdog --
Software serial for STM 230400
Parse STM data
Log to SD card
Sequencing/State Machine Overwatch-DeepSleep-Log

pins turn OFF/ON (STM and GPS only)
red 13 led
frequency change 240MHz 80MHz (/3)
test at the start
FreeRTOS


Pins
-----
uUSB - UART debug
RX TX pins for STM hw UART {2}
SDA/SCL for GPS & RTC {2}
PPS for GPS {1}
pin for GPS power {1}
pin for STM power {1}
SPI for SD card, choose SS/CS {3+1}


Who is ON all with the ESP32
-----------------------------
SD card
RTC

Who can be turned ON and OFF
-----------------------------
GPS
STM


PCBs
----
battery sides
microcontroller
test lipo power TI + MOSFET
*
*
*
*
*
*
*
*
* ========================================
*/

// -------------------------- Includes --------------------------
//ESP32
#include <esp_system.h>
#include <rom/rtc.h> // reset reason

//String Formatting
#include <string.h>
#include <stdio.h>

// Terminology
// STM = Seismometer = Geophone = RS1D

// RTC
#include <RTClib.h> // use the one from Adafruit, not the forks with the same name

// -------------------------- Defines --------------------------
// All moved in "config.h" now

#define BATT_PIN                      35      // To detect the Lipo battery remaining charge, GPIO35 on Adafruit ESP32 (35 on dev kit)

#define CONSOLE_BAUD_RATE             115200  // Baudrate in [bauds] for serial communication with the console TX??(GPIO_17) RX??(GPIO_16)

// CPU frequency
#define TARGET_CPU_FREQUENCY          80

// Use the 2nd (out of 3) hardware serial
#define GeophoneSerial Serial1

#define SERIAL_VERBOSE // if defined then a lot of information (for debug) is sent to the console

/*
Serial0: RX0 on GPIO3, TX0 on GPIO1
Serial?????: RX1 on GPIO9, TX1 on GPIO10  (+CTS1 and RTS1)
Serial1: RX2 on GPIO16, TX2 on GPIO17 (+CTS2 and RTS2)
*/

/*
|  UART |  RX IO |  TX IO |  CTS  |   RTS  |
|:-----:|:------:|:------:|:-----:|:------:|
| UART0 |  GPIO3 |  GPIO1 |  N/A  |   N/A  |
| UART1 |  GPIO9 | GPIO10 | GPIO6 | GPIO11 |
| UART2 | GPIO16 | GPIO17 | GPIO8 |  GPIO7 |
*/

#define GEOPHONE_BAUD_RATE             230400  // Baudrate in [bauds] for serial communication with the Geophone TX??(GPIO_17) RX??(GPIO_16)
//insert complete pins table here

volatile uint8_t bumpDetected             = 0;    //0= no bumps, 1= bumps
volatile uint8_t GoodMessageReceived_flag = 0;    //0= message not good, 1= message good
volatile uint32_t seismometerAcc[50];             // List of accelerations in 1 single message from the Geophone, received 4 times a second

// Bump detection
//#define BUMP_THRESHOLD                  (1u)
#define BUMP_THRESHOLD_POS              (20000u)      //   20000u   0x00008888   34952
#define BUMP_THRESHOLD_NEG              (4294965965u)
#define NBR_BUMPS_DETECTED_BEFORE_LOG   (5u)          // Minimum number of seismic events to be detected during overwatch for entering the LOG state
#define NBR_OVERWATCH_BEFORE_ACTION     (10u)         // Number of overwatch cycles before we go we make a descision
#define TIMELOG_SECONDS                 (30u)         // In [s], time spent in the LOG state before going for overwatch state,  prev. was (2u)

#define TIME_TO_SLEEP  (5u)                           // In [s], time spent in the deep sleep state
#define uS_TO_S_FACTOR  (1000000u)                    // Conversion factor from us to s for the deepsleep

#define RxBufferSize  (650u)        // 640 actually needed, margin of 10 observed
char    RS1Dmessage[RxBufferSize];  // Have you seen the sheer size of that!!
volatile uint8_t RS1D_SOM_flag = 0; //
volatile uint8_t RS1D_EOM_flag = 0; //


//#define SEPARATOR_CHAR  0x2C    // ',' Separate fields in messages
//#define EOM_CHAR_SR     0x7D    // '}'
//#define SOM_CHAR_SR     0x7B    // '{'
//#define EMPTY_CHAR      0x00    // '\0'
//#define OutBufferSize (26u)     //

// State machine
#ifndef STEPS_H
  #define STEPS_H
  #define STATE_OVERWATCH      (3u)
  #define STATE_SLEEP          (2u)
  #define STATE_LOG            (1u)
  #define STATE_EMPTY          (0u)
#endif /* STEPS_H */      
volatile uint8_t currentState = (0u); // Used to store which step we are at, default is state "empty"

//WATCHDOG    
hw_timer_t * timer = NULL;
const uint8_t wdtTimeout = 3; // Watchdog timeout in [s]

// Battery
#define BATT_VOLTAGE_DIVIDER_FACTOR   2       // [N/A]
#define LOW_BATTERY_THRESHOLD         3.1     // in [V]

//RTC
RTC_PCF8523         rtc;
DateTime            time_loop;             // MUST be global!!!!! or it won't update
DateTime            timestampForFileName;  // MUST be global!!!!! or it won't update


// Relay Pins (for turning external components ON and OFF)
#define GPS_PWR_PIN_1                      14      // To turn the GPS ON and OFF
//#define GPS_PWR_PIN_2                      32      // To turn the GPS ON and OFF
//#define GPS_PWR_PIN_3                      15      // To turn the GPS ON and OFF

#define RS1D_PWR_PIN_1                      25      // To turn the geophone ON and OFF
#define RS1D_PWR_PIN_2                      26      // To turn the geophone ON and OFF
//#define RS1D_PWR_PIN_3                      04      // To turn the geophone ON and OFF



// -------------------------- Structs --------------------------
// All moved in "config.h" now

// -------------------------- Classes -------------------------
// NONE

// -------------------------- Global variables ----------------
// Most are moved in "config.h" now

// Non Volatile Memory (NVM) for dee sleeps
//This variable will count how many times the ESP32 has woken up from deep sleep.
#include <Preferences.h>
Preferences preferences;

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
#define USE_SD                     // If defined then the ESP32 will log data to SD card (if not it will just read IMU) // <Not coded yet>
#define PIN_CS_SD       33        // Chip Select (ie CS/SS) for SPI for SD card
#define SOM_LOG         '$'       // Start of message indicator, mostly used for heath check (no checksum)
#define FORMAT_TEMP     1         // Numbers significative digits for the TEMPERATURE
#define FORMAT_ACC      6         // Numbers significative digits for the ACCELEROMETERS
#define FORMAT_GYR      6         // Numbers significative digits for the GYROSCOPES
#define FORMAT_SEP      ','       // Separator between the different files so that the data can be read/parsed by softwares
#define FORMAT_END      "\r\n"    // End of line for 1 aquisition, to be printed in the SD card // <Not used>
#define MAX_LINES_PER_FILES 18000 // Maximum number of lines that we want stored in 1 SD card file. It should be about 1h worth
#define SESSION_SEPARATOR_STRING "----------------------------------"
// SD
File                dataFile;              // Only 1 file can be opened at a certain time, <KEEP GLOBAL>





// -------------------------- ISR ----------------
// Watchdog
void IRAM_ATTR resetModule() {
  ets_printf("Problem! Watchdog trigger: Rebooting...\r\n");
  esp_restart();
}




// -------------------------- Functions declaration --------------------------
uint32_t  hex2dec                     (char * a); // Change hexadecimal values of the seismometer ADC to decimal
void      parseGeophoneData           (void);     // Parse geophone data
uint8_t   detectBump                  (void);     // Detect if there was a "bump" or "spike" in the geophone data
void    pinSetUp                   (void);        // Declare which pins of the ESP32 will be used
void    checkBatteryLevel          (void);        // Print the voltage of the lipo battery between R10 and R11 (voltage divider)
void displayWakeUpReason            (void);       // Display why the module went to sleep
void changeCPUFrequency             (void);       // Change the CPU frequency and report about it over serial
void testRTC                        (void);       // Test that the RTC can be accessed
void turnGPSOFF                     (void);       // Truns GPS OFF
void turnGPSON                     (void);        // Truns GPS ON
void turnRS1DOFF                     (void);       // Truns RS1D OFF
void turnRS1DON                     (void);        // Truns RS1D ON
void testSDCard                     (void);                 // Test that the SD card can be accessed
//void createNewFile                  (void);                 // Create a name a new DATA file on the SD card, file variable is global
void blinkAnError                   (uint8_t errno);        // Use an on-board LED (the red one close to the micro USB connector, left of the enclosure) to signal errors (RTC/SD)
//void createNewSeparatorFile         (void);                 // Create a name a new SEPARATOR file on the SD card and close it immediatly (just a beautifier)

// -------------------------- Set up --------------------------

void setup() {

  // Pin declaration
  // -----------------
  pinSetUp();

digitalWrite(LED_BUILTIN, HIGH);

  // DEBUG
  currentState = STATE_OVERWATCH;

  // Start the HW serial for the console (micro USB)
  // -----------------------------------------------
  Serial.begin(CONSOLE_BAUD_RATE);


  // Do some preliminary tests
  // -------------------------
   Serial.println("***********************************************************************"); //Indicates via the console that a new cycle started 
  displayWakeUpReason   ();
  changeCPUFrequency    ();
  checkBatteryLevel     ();
  testRTC();
  testSDCard();  
  Serial.println("----------------------------------------"); // Indicates the end of test
  Serial.println("All tests done");

  // Update the boot counter value
  // ----------------------------
  // Open Preferences with prefOne namespace. Each application module, library, etc
  // has to use a namespace name to prevent key name collisions. We will open storage in
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  preferences.begin("prefOne", false);
  // Get the counter value, if the key does not exist, return a default value of 0
  // Note: Key name is limited to 15 chars.
  unsigned int savedBootCount = preferences.getUInt("savedBootCount", 0);
  #ifdef SERIAL_VERBOSE
    // Print the savedBootCount to Serial Monitor
    Serial.printf("Current savedBootCount value: %u\n", savedBootCount);
  #endif
  
  savedBootCount++; // Increment the boot count

  // Store the counter to the Preferences
  preferences.putUInt("savedBootCount", savedBootCount);
  // Close the Preferences
  preferences.end();

  Serial.println("----------------------------------------"); // Indicates the end of NVM

  // Starting the night watch (watchdog)
  //-------------------------------------
  // Remember to hold the door
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * uS_TO_S_FACTOR, false); // set time is in [us]
  // Enable all the ISR later

  // Initialise parsed seismometer data array (50 values / message)
  // ----------------------------------------------------------------
  uint8_t cnt_parsedAccData = 0;
    for (cnt_parsedAccData = 0; cnt_parsedAccData < 50; cnt_parsedAccData++)
  {
    seismometerAcc[cnt_parsedAccData] = (uint32_t)80000; //Initialise the seismometer array with the decimal value 80000, but why tough?
  }
  
  // Start the HW serial for the Geophone/STM
  // ----------------------------------------
  GeophoneSerial.begin(GEOPHONE_BAUD_RATE);

  
  // Enable all ISRs
  //----------------
  Serial.println("And here, we, go, ...");
  Serial.println("----------------------------------------"); // Indicates the end of the set up
  digitalWrite(LED_BUILTIN, LOW); // Switch off the red built-in led to indicate end of set up
  // Do not go gentle into that good night

  //timerAlarmEnable(timer);     // Watchdog <DEBUG>

}

// -------------------------- Loop --------------------------
void loop() {

turnRS1DON();
turnGPSON();


delay(100000); // <DEBUG>
  // Prepare for saving the current state
  // ------------------------------------
  // Open Preferences with prefTwo namespace. Each application module, library, etc
  // has to use a namespace name to prevent key name collisions. We will open storage in
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  preferences.begin("prefTwo", false);
  // Get the state value, if the key does not exist, return a default value of "STATE_EMPTY"
  // Note: Key name is limited to 15 chars.
  unsigned int savedCurrState = preferences.getUInt("savedCurrState", STATE_EMPTY);
  #ifdef SERIAL_VERBOSE
    // Print the savedCurrState to Serial Monitor
    Serial.printf("Current savedCurrState value: %u\n", savedCurrState);
  #endif

delay (1000);

  // Deal with the state machine
  // -----------------------------
  
  // If we were previously in deep sleep then we need to go to overwatch
  if (currentState == STATE_SLEEP)
  {

    currentState = STATE_OVERWATCH;
  }

  // Print dat as for RTC
  // ---------------------

  Serial.println("RTC time");
  
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

// DEBUG
  GeophoneSerial.println("This is a test");
  Serial.println("This is a test");

  

  // Deal with incoming geophone data
  // ----------------------------------

  
  // Check we are in the correct state
  if ( (currentState == STATE_OVERWATCH) ||  (currentState == STATE_LOG) )
  {

    //digitalWrite(LED_BUILTIN, HIGH);
    
    // If we are in a state that requires reading the Geophone then read the GEOPHONE serial buffer until no more character available
    if (GeophoneSerial.available()) 
    {
        char c = GeophoneSerial.read();
        Serial.write(c);
    }


/* Example 1st message part:
 *  {"MSEC": 743500,"LS": 0}
 */

 /* Example 2nd message part:
  *  {"MA": "RS1D-6-4.5","DF": "1.0","CN": "SH3","TS": 0,"TSM": 0,"TQ": 45,"SI":  5000,"DS": ["1E52","1F94","2011","1F57","1E45","1DC6","1E94","1FE7","20A3","203F","1ED5","1E05","1E66","1F5A","1FEF","1F94","1F16","1EDC","1F25","1F7C","1F54","1EC4","1E7D","1ECD","1FFF","20B7","2065","1F4C","1E80","1EEA","1FE0","2085","1FE8","1EBB","1E5C","1EE4","2024","20C3","2024","1F2D","1EA8","1F44","2027","2087","2064","1FAA","1F2E","1F65","1FD4","2063"]}
  */
    //RS1Dmessage = ""; // <DEBUG>
    // If you have at least 1 complete message from the geophone (both flags: SOM and EOM), parse it
    if(RS1D_EOM_flag == 1) // if we have the "End Of Message" then, with our code, we have 
    {
      RS1D_EOM_flag = 0;      // Immediatly reset the flag
      parseGeophoneData();    // Parse the Gophone data present in our own buffer and put it the parsed acc array
    }
  }
    
  
  // Deep sleep
  // -----------
  
  // Change the mode
  currentState = STATE_SLEEP;
  savedCurrState = currentState;

  // Save the current state in NVM
  //savedCurrState = currentState;
  // Store the current state to the Preferences
  preferences.putUInt("savedCurrState", savedCurrState);
  // Close the Preferences
  preferences.end();

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
void parseGeophoneData(void)
{
  GoodMessageReceived_flag  = (0u); // Set to "bad message" first
  bumpDetected              = (0u); // Init as "nothing detected": move along, move along

  if (strstr(RS1Dmessage, "]}"))
  {
    char    temp1[9];
    uint8_t cnt_accvalues = 0;
    char    *p = RS1Dmessage;
    
    GoodMessageReceived_flag = 1; // <----- the F***? TODO: Check

    // get 1st acceleration
    p = strchr(p, '[')+1;
    p++; //Avoid the "
    uint8_t cnt = 0;
    memset(temp1,'\0',9);
    while((*p != '"') & (cnt < 9)) // No error checking... Try not. Do, or do not. There is no try
    {
      temp1[cnt] = *p;
      cnt++;
      p++;
    }
    seismometerAcc[0] = hex2dec(temp1);
    if((seismometerAcc[0] > BUMP_THRESHOLD_POS) & (seismometerAcc[0] < BUMP_THRESHOLD_NEG)) bumpDetected = (1u);
    
    for (cnt_accvalues = 1; cnt_accvalues < 50; cnt_accvalues++)
    {
      p = strchr(p, ',')+1;
      p++; //Avoid the "
      cnt = 0;
      memset(temp1,'\0',9);
      while((*p != '"') & (cnt < 9))
      {
        temp1[cnt] = *p;
        cnt++;
        p++;
      }
      seismometerAcc[cnt_accvalues] = hex2dec(temp1);
      if((seismometerAcc[cnt_accvalues] > BUMP_THRESHOLD_POS) & (seismometerAcc[cnt_accvalues] < BUMP_THRESHOLD_NEG)) bumpDetected |= (1u);
    }
  }
}

//******************************************************************************************
uint8_t DetectBump(void)
{
  uint8_t bump      = 0; // Init as "nothing detected": move along, move along  
  uint8_t cnt_msg   = 0; // Used to count the number of seismometer messages received and checked
  
  //Only keep looking when NO BUMPS DETECTED and all data in 1 message analysed
  do
  {
    if((seismometerAcc[cnt_msg] > BUMP_THRESHOLD_POS) & (seismometerAcc[cnt_msg] < BUMP_THRESHOLD_NEG)) bump = 1; //These aren't the droids you're looking for.
    cnt_msg++;
    
  }while((bump == 0) & (cnt_msg < sizeof(seismometerAcc)));
  
  return bump;
}

//******************************************************************************************
void pinSetUp (void)
{

/*
|  UART |  RX IO |  TX IO |  CTS  |   RTS  |
|:-----:|:------:|:------:|:-----:|:------:|
| UART0 |  GPIO3 |  GPIO1 |  N/A  |   N/A  |
| UART1 |  GPIO9 | GPIO10 | GPIO6 | GPIO11 |
| UART2 | GPIO16 | GPIO17 | GPIO8 |  GPIO7 |
*/
  
  // Declare which pins of the ESP32 will be used
    pinMode (LED_BUILTIN , OUTPUT);
    pinMode (BATT_PIN    , INPUT);

 // Relay pins (GPS+RS1D)   
pinMode (GPS_PWR_PIN_1    , OUTPUT);
//pinMode (GPS_PWR_PIN_2    , OUTPUT);
//pinMode (GPS_PWR_PIN_3    , OUTPUT);

turnGPSOFF();

pinMode (RS1D_PWR_PIN_1    , OUTPUT);
pinMode (RS1D_PWR_PIN_2    , OUTPUT);
//pinMode (RS1D_PWR_PIN_3    , OUTPUT);
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
void displayWakeUpReason (void)
{
    Serial.println("----------------------------------------");
    Serial.print("CPU0 reset reason:");
    //print_reset_reason(rtc_get_reset_reason(0));
    verbose_print_reset_reason(rtc_get_reset_reason(0));
    Serial.print("CPU1 reset reason:");
    //print_reset_reason(rtc_get_reset_reason(1));
    verbose_print_reset_reason(rtc_get_reset_reason(1));
}

//******************************************************************************************
void print_reset_reason(RESET_REASON reason)
{
  switch ( reason)
  {
    case 1 : Serial.println ("POWERON_RESET");break;          /**<1,  Vbat power on reset*/
    case 3 : Serial.println ("SW_RESET");break;               /**<3,  Software reset digital core*/
    case 4 : Serial.println ("OWDT_RESET");break;             /**<4,  Legacy watch dog reset digital core*/
    case 5 : Serial.println ("DEEPSLEEP_RESET");break;        /**<5,  Deep Sleep reset digital core*/
    case 6 : Serial.println ("SDIO_RESET");break;             /**<6,  Reset by SLC module, reset digital core*/
    case 7 : Serial.println ("TG0WDT_SYS_RESET");break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8 : Serial.println ("TG1WDT_SYS_RESET");break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9 : Serial.println ("RTCWDT_SYS_RESET");break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10 : Serial.println ("INTRUSION_RESET");break;       /**<10, Instrusion tested to reset CPU*/
    case 11 : Serial.println ("TGWDT_CPU_RESET");break;       /**<11, Time Group reset CPU*/
    case 12 : Serial.println ("SW_CPU_RESET");break;          /**<12, Software reset CPU*/
    case 13 : Serial.println ("RTCWDT_CPU_RESET");break;      /**<13, RTC Watch dog Reset CPU*/
    case 14 : Serial.println ("EXT_CPU_RESET");break;         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : Serial.println ("RTCWDT_BROWN_OUT_RESET");break;/**<15, Reset when the vdd voltage is not stable*/
    case 16 : Serial.println ("RTCWDT_RTC_RESET");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : Serial.println ("NO_MEAN");
  }
}

//******************************************************************************************
void verbose_print_reset_reason(RESET_REASON reason)
{
  switch ( reason)
  {
    case 1  : Serial.println ("Vbat power on reset");break;
    case 3  : Serial.println ("Software reset digital core");break;
    case 4  : Serial.println ("Legacy watch dog reset digital core");break;
    case 5  : Serial.println ("Deep Sleep reset digital core");break;
    case 6  : Serial.println ("Reset by SLC module, reset digital core");break;
    case 7  : Serial.println ("Timer Group0 Watch dog reset digital core");break;
    case 8  : Serial.println ("Timer Group1 Watch dog reset digital core");break;
    case 9  : Serial.println ("RTC Watch dog Reset digital core");break;
    case 10 : Serial.println ("Instrusion tested to reset CPU");break;
    case 11 : Serial.println ("Time Group reset CPU");break;
    case 12 : Serial.println ("Software reset CPU");break;
    case 13 : Serial.println ("RTC Watch dog Reset CPU");break;
    case 14 : Serial.println ("for APP CPU, reseted by PRO CPU");break;
    case 15 : Serial.println ("Reset when the vdd voltage is not stable");break;
    case 16 : Serial.println ("RTC Watch dog reset digital core and rtc module");break;
    default : Serial.println ("NO_MEAN");
  }
}

//******************************************************************************************
void changeCPUFrequency (void)
{
  Serial.println("----------------------------------------");
  Serial.print("Current CPU frequency [MHz]: ");
  Serial.println(getCpuFrequencyMhz());
  Serial.println("Changing...");
  setCpuFrequencyMhz(TARGET_CPU_FREQUENCY);//sets CPU to 80Mhz
  Serial.print("Current CPU frequency [MHz]: ");
  Serial.println(getCpuFrequencyMhz());
}

//******************************************************************************************
void testRTC(void) {
  Serial.println("----------------------------------------");
  Serial.println("Testing the RTC...");

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (!rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC is NOT initialized. Use the NTP sketch to set the time!");
    //blinkAnError(6);
  }

  // When the RTC was stopped and stays connected to the battery, it has
  // to be restarted by clearing the STOP bit. Let's do this to ensure
  // the RTC is running.
  rtc.start();

  Serial.println("Let's see if the RTC is running");
  Serial.println("There should a difference of about 1s");
  
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  delay(1000); // Wait 1s so the user can see if the RTC is running by looking at the console

  now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  Serial.println("Please check");

}

//******************************************************************************************
void turnGPSOFF(void) {
  
  #ifdef SERIAL_VERBOSE
    Serial.println("Turning GPS OFF...");
  #endif
  
  digitalWrite(GPS_PWR_PIN_1, LOW);
  //digitalWrite(GPS_PWR_PIN_2, LOW);
  //digitalWrite(GPS_PWR_PIN_3, LOW);
  
  #ifdef SERIAL_VERBOSE
    Serial.println("GPS is OFF...");
  #endif

}

//******************************************************************************************
void turnGPSON(void) {
  
  #ifdef SERIAL_VERBOSE
    Serial.println("Turning GPS ON...");
  #endif
  
  digitalWrite(GPS_PWR_PIN_1, HIGH);
  //digitalWrite(GPS_PWR_PIN_2, HIGH);
  //digitalWrite(GPS_PWR_PIN_3, HIGH);
  
  #ifdef SERIAL_VERBOSE
    Serial.println("GPS is ON...");
  #endif

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
  String testString = "Test 0123456789";

  // see if the card is present and can be initialized:
  if (!SD.begin(PIN_CS_SD)) {
    Serial.println("Card failed, or not present");
    blinkAnError(2);
    // Don't do anything more: infinite loop just here
    while (1);
  }
  Serial.println("Card initialized");

  if (SD.exists("/00_test.txt")) { // The "00_" prefix is to make sure it is displayed by win 10 explorer at the top
    Serial.println("Looks like a test file already exits on the SD card"); // Just a warning
  }

  // Create and open the test file. Note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("/00_test.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(testString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(testString);
  }
  // If the file isn't open, pop up an error:
  else {
    Serial.println("Error while opening /00_test.txt");
    blinkAnError(3);
  }

  // Add a separator file (empty but with nice title) so the user knows a new DAQ session started
  //createNewSeparatorFile();

}

// END OF FILE
