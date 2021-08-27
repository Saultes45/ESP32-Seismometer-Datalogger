/* ========================================
*
* Copyright ??
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : ??
* Verified by   : Nathanaël Esnault
* Creation date : 2021-??-??
* Version       : 1.0 (finished on 2021-..-..)
* Modifications :
* Known bugs    :
*
*
* Possible Improvements
*
* Notes
*
  $GPZDA (not available for our unit)
  Date & Time
  
  UTC, day, month, year, and local time zone.
  
  $--ZDA,hhmmss.ss,xx,xx,xxxx,xx,xx
  hhmmss.ss = UTC
  xx = Day, 01 to 31
  xx = Month, 01 to 12
  xxxx = Year
  xx = Local zone description, 00 to +/- 13 hours
  xx = Local zone minutes description (same sign as hours)
*
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
#include <Adafruit_GPS.h>
#include <RTClib.h> 


// -------------------------- Defines and Const --------------------------
#define GPS_PPS_PIN                      27   // To get an interrupt each time there is a PPS from GPS
const uint8_t GPS_BOOST_ENA_PIN        = 21;  // To turn the BOOST converter of the GPS ON and OFF
//#define WAIT_GPS_FIX                        // Comment out to NOT wait until we have a GPS fix before we begin

// -------------------------- Global Variables --------------------------
// Connect to the GPS on the hardware I2C port
Adafruit_GPS  GPS(&Wire);
DateTime      timeStamp;             // MUST be global!!!!! or it won't update

char timeStampFormat_Line[]     = "YYYY_MM_DD__hh_mm_ss";
    //source https://github.com/adafruit/RTClib/blob/master/examples/toString/toString.ino
    // HERE -> look for user "cattledog" : https://forum.arduino.cc/t/now-rtc-gets-stuck-if-called-in-setup/632619/3 
    //buffer can be defined using following combinations:
    // hh   - hour with a leading zero (00 to 23)
    // mm   - minute with a leading zero (00 to 59)
    // ss   - whole second with a leading zero where applicable (00 to 59)
    // YYYY - year as four digit number
    // YY   - year as two digit number (00-99)
    // MM   - month as number with a leading zero (01-12)
    // MMM  - abbreviated English month name ('Jan' to 'Dec')
    // DD   - day as number with a leading zero (01 to 31)
    // DDD  - abbreviated English day name ('Mon' to 'Sun')

// -------------------------- ISR ----------------
volatile bool ppsDetected             = false;    //false= no rising edge, true= rising edge

//void IRAM_ATTR ISR_GPS_PPS() 
//{
//    ppsDetected = true; //set the flag
//}

// -------------------------- Set up --------------------------
void setup() 
{
  
  while (!Serial); // wait for hardware serial to appear <DEBUG>

  // Dealing with PPS
  //-------------------
  //pinMode (GPS_PPS_PIN    , INPUT);
  //pinMode(GPS_PPS_PIN, INPUT_PULLDOWN);
  //attachInterrupt(GPS_PPS_PIN, ISR_GPS_PPS, RISING);
  //detachInterrupt(GPS_PPS_PIN);
  // LOW, HIGH, CHANGE, FALLING, RISING

  // GPS power
  //----------
  pinMode (GPS_BOOST_ENA_PIN    , OUTPUT);
  digitalWrite(GPS_BOOST_ENA_PIN, HIGH); // Start the 5V boost convert

  // Serial port
  //------------
  // make this baud rate fast enough so we aren't waiting on it
  Serial.begin(115200);
  Serial.println("Adafruit GPS library I2C test");
  Serial.println("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
  Serial.println("");
  Serial.println("");
  
  Serial.println("Starting to address the GPS...");
  GPS.begin(0x10);  // The I2C address to use is 0x10


  // Set up communication
  // ---------------------
  GPS.sendCommand(PMTK_SET_BAUD_115200);      // Ask the GPS to send us data at 115200bps
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ); // 10 Hz update rate (message only)
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);  // Can't fix position faster than 5 times a second


  // Turn OFF all GPS output
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_OFF);
  
  // Remove all data in buffer
  unsigned long startedWaiting = millis();
  while((GPS.available()) && (millis() - startedWaiting <= 10000))
  {
      GPS.read(); // don't save the data, just dump
//      Serial.write(GPS.read());    
      Serial.print(".");
  }
  Serial.println(" done");




  // Request GPS antenna info
  // -------------------------
  Serial.println("GPS antenna info");
  Serial.println("***************************");
  GPS.sendCommand(PGCMD_ANTENNA);
  startedWaiting = millis();
  while((GPS.available()) && (millis() - startedWaiting <= 1000))
  {
     Serial.write(GPS.read());
  }
  Serial.println("");
  Serial.println("Time's up");
  Serial.println("***************************");



  // Request GPS firmware info
  // -------------------------
  Serial.println("GPS firmware info");
  Serial.println("***************************");
  GPS.println(PMTK_Q_RELEASE);
  startedWaiting = millis();
  while((GPS.available()) && (millis() - startedWaiting <= 1000))
  {
     Serial.write(GPS.read());
  }
  Serial.println("Time's up");
  Serial.println("***************************");




  Serial.println("Asking for RMC continuously");
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);  // RMC or RMC + GGA




  Serial.println("Waiting here for a GPS fix ...");
  #ifdef WAIT_GPS_FIX
    while(not(GPS.fix))
    {
      delay(100);
    }
  #endif
  Serial.println("We have the fix, PPS should be working now");
  Serial.println("Fix: " + String(GPS.fix));
    
  }





// -------------------------- Loop --------------------------
void loop() 
{

  // ISR check
  //----------
  if (ppsDetected) {
    Serial.println("PPS pulse received");
    ppsDetected = false;
  }

  
  //  PC --> GPS
  //-------------
  //    if (Serial.available()) 
  //  {
  //      char c = Serial.read();
  //      GPS.write(c);
  //    }


  //  GPS --> PC
  //-------------  
  if (GPS.available()) 
  {
    char c = GPS.read();
    Serial.write(c);
  }

//    if (GPS.newNMEAreceived()) 
//    {
//
//    // Save time to struct
//    //------------------------------------------
//
//      
//      Serial.print("$20");
//      if (GPS.year < 10) { Serial.print('0'); }
//      Serial.print(GPS.year, DEC);
//      Serial.print("-");
//      if (GPS.month < 10) { Serial.print('0'); }
//      Serial.print(GPS.month, DEC);
//      Serial.print("-");
//      if (GPS.day < 10) { Serial.print('0'); } 
//      Serial.print(GPS.day, DEC);
//      Serial.print("--");
//      if (GPS.hour < 10) { Serial.print('0'); }
//      Serial.print(GPS.hour, DEC);
//      Serial.print("-");
//      if (GPS.minute < 10) { Serial.print('0'); }
//      Serial.print(GPS.minute, DEC); 
//      Serial.print("-");
//      if (GPS.seconds < 10) { Serial.print('0'); }
//      Serial.print(GPS.seconds, DEC); 
//      Serial.print(".");
//      Serial.println(GPS.milliseconds);
//
//      // Does not support ms (but GPS do)
//      timeStamp = DateTime(GPS.year,GPS.month,GPS.day, GPS.hour, GPS.minute, GPS.seconds); // MUST be global!!!!! or it won't update
//    //  DateTime now = DateTime(2000,01,01, 00, 00, 00); // <DEBUG>
//    //setTime(hr,min,sec,day,month,yr); // Another way to set
//  
//    Serial.print("Time from GPS: ");
//    Serial.println(timeStamp.toString(timeStampFormat_Line));
//      
////      Serial.print("MS: ");
////      Serial.println(GPS.milliseconds);
//      
//    }
  
  // Get GPS time
  //  uint8_t hour;          ///< GMT hours
  //  uint8_t minute;        ///< GMT minutes
  //  uint8_t seconds;       ///< GMT seconds
  //  uint16_t milliseconds; ///< GMT milliseconds
  //  uint8_t year;          ///< GMT year
  //  uint8_t month;         ///< GMT month
  //  uint8_t day;           ///< GMT day
    
}

// END OF SCRIPT
