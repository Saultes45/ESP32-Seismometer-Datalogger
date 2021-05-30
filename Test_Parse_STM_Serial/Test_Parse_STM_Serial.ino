
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
* ========================================
*/

// -------------------------- Includes --------------------------
//ESP32
#include <esp_system.h>
#include <rom/rtc.h> // reset reason

//String Formatting
#include <string.h>
#include <stdio.h>

// STM = Seismometer = Geophone = RS1D

// -------------------------- Defines --------------------------
// All moved in "config.h" now

// Use the 2nd (out of 3) hardware serial
#define GeophoneSerial Serial1

#define GEOPHONE_BAUD_RATE             230400  // Baudrate in [bauds] for serial communication with the  TX??(GPIO_17) RX??(GPIO_16)
//insert pins table here

volatile uint8_t bumpDetected = 0;                //0= no bumps, 1= bumps
volatile uint8_t GoodMessageReceived_flag = 0;    //0= message not good, 1= message good
uint32_t seismometerAcc[50]; // List of accelerations in 1 single message from the Geophone, received 4 times a second

// Bump detection
//#define BUMP_THRESHOLD                  (1u)
#define BUMP_THRESHOLD_POS              (20000u) //   20000u   0x00008888   34952
#define BUMP_THRESHOLD_NEG              (4294965965u)
#define NBR_BUMPS_DETECTED_BEFORE_LOG   (5u) // 5u
#define NBR_OVERWATCH_BEFORE_ACTION     (10u) // 10u
#define TIMELOG_SECONDS                 (30u)  //s prev (2u)

#define RxBufferSize  (650u)  //640 actually needed, margin of 10 observed
char    RS1Dmessage[RxBufferSize]; ////Have you seen the sheer size of that!!

//#define SEPARATOR_CHAR  0x2C    // ',' Separate fields in messages
//#define EOM_CHAR_SR     0x7D    // '}'
//#define SOM_CHAR_SR     0x7B    // '{'
//#define EMPTY_CHAR      0x00    // '\0'
//#define OutBufferSize (26u)   //

#define STATE_OVERWATCH      (3u)
#define STATE_SLEEP          (2u)
#define STATE_LOG            (1u)
#define STATE_EMPTY          (0u)
    
volatile uint8_t currentState = (0u); // Used to store which step we are at, default is empty
    

// -------------------------- Structs --------------------------
// All moved in "config.h" now

// -------------------------- Classes -------------------------
// NONE

// -------------------------- Global variables ----------------
// Most are moved in "config.h" now


// -------------------------- ISR ----------------


// -------------------------- Functions declaration --------------------------
uint32_t  hex2dec                     (char * a); // Change hexadecimal values of the seismometer ADC to decimal
void      parseGeophoneData           (void);     // Parse geophone data
uint8_t   detectBump                  (void);     // Detect if there was a "bump" or "spike" in the geophone data


// -------------------------- Set up --------------------------

void setup() {
  
  // Start the HW serial for the Geophone/STM
  // ----------------------------------------
  GeophoneSerial.begin(GEOPHONE_BAUD_RATE);

}

// -------------------------- Loop --------------------------
void loop() {
  
// Check we are in the correct state
if ( currentState == STATE_)
if (GeophoneSerial.available()) {
    char c = GPSSerial.read();
    Serial.write(c);
  }


// DeepSleep
//------------

// Change the mode
currentState = STATE_SLEEP;

// <ADD DEEP SLEEP CODE HERE>

}

//******************************************************************************************
uint32_t hex2dec(char * a)
{
  char c;
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
  GoodMessageReceived_flag = 0; // Set to "bad message" first
  bumpDetected = (0u); // Init as "nothing detected", move along move along

  if (strstr(RS1Dmessage, "]}"))
  {
    char    temp1[9];
    uint8_t cnt_accvalues = 0;
    char    *p = RS1Dmessage;
    
    GoodMessageReceived_flag = 1; // <----- the F***?

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
  uint8_t bump = 0; // Init as "nothing detected": move along, move along
  
  uint8_t cnt_msg = 0; // Used to count the number of seismometer messages received and checked
  
  //Only keep looking when NO BUMPS DETECTED and all data in 1 message analysed
    do
  {
    if((seismometerAcc[cnt_msg] > BUMP_THRESHOLD_POS) & (seismometerAcc[cnt_msg] < BUMP_THRESHOLD_NEG)) bump = 1; //These aren't the droids you're looking for.
    cnt_msg++;
    
  }while((bump == 0) & (cnt_msg < sizeof(seismometerAcc)));
  
  return bump;
}

// END OF FILE
