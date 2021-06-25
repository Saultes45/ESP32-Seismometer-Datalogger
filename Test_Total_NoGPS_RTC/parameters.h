/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : N/A
* Creation date : 2021-06-23
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
*
*/

// -------------------------- Conditional defines --------------------------

#define SERIAL_VERBOSE // Comment out to remove message to the console (and save some exec time and power)
#define USE_GPS        // Comment out to not use any code for the GPS (even the library)

/* if you uncomment USE_GPS, you have to actually unconnect its SDA and SCL. 
 *  Otherwise the RTC will not work
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

// CPU frequency
#define MAX_CPU_FREQUENCY             240
#define TARGET_CPU_FREQUENCY          40


// Battery
#define BATT_VOLTAGE_DIVIDER_FACTOR   2.0       // [N/A]
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

#define NBR_OVERWATCH_BEFORE_ACTION     20
#define NBR_BUMPS_DETECTED_BEFORE_LOG   3
#define NBR_LOG_BEFORE_ACTION           40
#define TIME_TO_SLEEP                   10          // In [s], time spent in the deep sleep state
#define S_TO_US_FACTOR                  1000000ULL  // Conversion factor from [s] to [us] for the deepsleep // Source: https://github.com/espressif/arduino-esp32/issues/3286
#define S_TO_MS_FACTOR                  1000        // Conversion factor from [s] to [ms] for the delay

// Bump detection
const int32_t BUMP_THRESHOLD_POS =      +100000;
const int32_t BUMP_THRESHOLD_NEG =      -100000;


// RS1D
const uint8_t       NBR_ACCELERATIONS_PER_MESSAGE     =  50;     // Scope controlled  + cannot be reassigned
#define             RX_BUFFER_SIZE                       650     // 640 actually needed, margin of 10 observed
const unsigned long RS1D_DEPILE_TIMEOUT_MS            =  100;    // in [ms]
const uint16_t      RS1D_WARMUP_TIME_MS               =  5000;   // in [ms]
#define             GEOPHONE_BAUD_RATE                   230400  // Baudrate in [bauds] for serial communication with the Geophone
#define             GEOPHONE_TIMEOUT_MS                  100     // For the character search in buffer, in [ms]
#define             GeophoneSerial                       Serial1 // Use the 2nd (out of 3) hardware serial of the ESP32
const uint8_t       RS1D_PWR_PIN_1                    =  14;     // To turn the geophone ON and OFF


// WATCHDOG
hw_timer_t *    timer           = NULL;
const uint8_t   wdtTimeout      = 3;    // WDT timeout in [s]
const uint8_t   MAX_NBR_WDT     = 10;   // Recurring WDT threshold on counter
const int       WDT_SLP_RECUR_S = 1800; // Time to sleep if WDT triggers more than the threshold //Be careful for the type as the esprintf doesn't like some of them

// LOG+SD
#define         PIN_CS_SD                   33     // Chip Select (ie CS/SS) for SPI for SD card
const char      SOM_LOG                     = '$'; // Start of message indicator, mostly used for heath check (no checksum)
const char      FORMAT_SEP                  = ','; // Separator between the different files so that the data can be read/parsed by softwares
const uint16_t  MAX_LINES_PER_FILES         = NBR_LOG_BEFORE_ACTION;  // Maximum number of lines that we want stored in 1 SD card file. It should be about ...min worth
//const char      SESSION_SEPARATOR_STRING[]  =  "----------------------------------";
const uint8_t   LOG_PWR_PIN_1               = 25;  // To turn the geophone ON and OFF
const uint8_t   LOG_PWR_PIN_2               = 26;  // To turn the geophone ON and OFF
const uint8_t   LOG_PWR_PIN_3               = 12;  // To turn the geophone ON and OFF
const uint8_t   LOG_PWR_PIN_4               = 27;  // To turn the geophone ON and OFF


//GPS
#ifdef USE_GPS
  #include <Adafruit_GPS.h>
  // Connect to the GPS on the hardware I2C port
  Adafruit_GPS GPS(&Wire);
  /*
   * In case we use the GPS for the timestamps during 
   * log, we prepare for the case where the GPS cannot
   * get a fix in time. In that case we revert back to 
   * using the RTC (also in RTC time but no  [ms])
   */
  bool            noFixGPS              = true;                 // Initialise to the worst case: no fix
  const uint32_t  GPS_NO_FIX_TIMEOUT_MS   = 5 * S_TO_MS_FACTOR;   // 
  const uint32_t  GPS_NEW_FILE_TIMEOUT_MS = 2 * S_TO_MS_FACTOR;   // 
#endif


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
//DateTime lastWatchdogTrigger;     // <NOT YET USED>
//volatile uint32_t nbr_WatchdogTrigger = 0;
//RTC_DATA_ATTR unsigned long nbr_WatchdogTrigger =0;
/*
 * The attribute RTC_DATA_ATTR tells the compiler that 
 * the variable should be stored in the real 
 * time clock data area. This is a small area of 
 * storage in the ESP32 processor that is part of 
 * the Real Time Clock. This means that the value will be 
 * set to zero when the ESP32 first powers up but will 
 * retain its value after a deep sleep. 
 */

//RTC
RTC_PCF8523         rtc;
DateTime            time_loop;             // MUST be global!!!!! or it won't update
DateTime            timestampForFileName;  // MUST be global!!!!! or it won't update

// GPS
const uint8_t GPS_BOOST_ENA_PIN     = 21;      // To turn the BOOST converter of the GPS ON and OFF
bool          GPSNeeded             = false;  // Used in the mail loop to descide if the GPS I2C buffer should be depiled
bool          GPSTimeAvailable      = false;  // Used in logging to know if we can use the lastest GPS timestamp string
String        lastGPSTimestamp      = "";

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
#ifdef USE_GPS
void      testGPS             (void);
void      waitForGPSFix       (void);
#endif
uint32_t  hex2dec             (char * a);
void      blinkAnError        (uint8_t errno);
void      changeCPUFrequency  (void);           // Change the CPU frequency and report about it over serial
void      prepareWDT          (void); // For the Timer and ISR
void      getGPSTime          (void);


// END OF FILE
