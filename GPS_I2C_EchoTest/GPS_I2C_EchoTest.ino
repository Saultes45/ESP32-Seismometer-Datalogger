
/*
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
*/


#include <Adafruit_GPS.h>

#define GPS_PPS_PIN                      27      // To get an interrupt each time there is a PPS from GPS

//#define WAIT_GPS_FIX                              // Comment out to NOT wait until we have a GPS fix before we begin

// Connect to the GPS on the hardware I2C port
Adafruit_GPS GPS(&Wire);

#include <RTClib.h> 

DateTime            timeStamp;             // MUST be global!!!!! or it won't update

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
//
//void IRAM_ATTR ISR_GPS_PPS() {
//    ppsDetected = true; //set the flag
//}


void setup() {
  
  while (!Serial); // wait for hardware serial to appear <DEBUG>

  pinMode (GPS_PPS_PIN    , INPUT);
  //pinMode(GPS_PPS_PIN, INPUT_PULLDOWN);
  //attachInterrupt(GPS_PPS_PIN, ISR_GPS_PPS, RISING);
  //detachInterrupt(GPS_PPS_PIN);
  // LOW, HIGH, CHANGE, FALLING, RISING

  // make this baud rate fast enough to we aren't waiting on it
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");

  Serial.println("Adafruit GPS library I2C test");
  GPS.begin(0x10);  // The I2C address to use is 0x10

  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_OFF);  // turn data OFF while we do a set up
  // Remove all data in buffer
  unsigned long startedWaiting = millis();
  while((GPS.available()) && (millis() - startedWaiting <= 1000))
  {
     GPS.read();
  }
  Serial.println("GPS buffer clean");


  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ);     // 10 Hz update rate  
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);      // Position fix update rate commands.



  Serial.println("GPS antenna info:");
  GPS.sendCommand(PGCMD_ANTENNA);
  startedWaiting = millis();
  while((GPS.available()) && (millis() - startedWaiting <= 1000))
  {
     Serial.write(GPS.read());
  }
  Serial.println("Time's up");




  Serial.println("GPS firmware info:");
  GPS.println(PMTK_Q_RELEASE);
  startedWaiting = millis();
  while((GPS.available()) && (millis() - startedWaiting <= 1000))
  {
     Serial.write(GPS.read());
  }
  Serial.println("Time's up");




  Serial.println("Asking for RMC (+GGA) continuously");
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


//-------------------------------------------------------------------------------------
void loop() 
{
  //  if (Serial.available()) {
  //    char c = Serial.read();
  //    GPS.write(c);
  //  }
  
  if (ppsDetected) {
        Serial.println("PPS pulse received");
        ppsDetected = false;
    }
  
//    if (GPS.available()) {
//      char c = GPS.read();
//      Serial.write(c);
//    }

    if (GPS.newNMEAreceived()) 
    {

    // Save time to struct
    //------------------------------------------

      
      Serial.print("$20");
      if (GPS.year < 10) { Serial.print('0'); }
      Serial.print(GPS.year, DEC);
      Serial.print("-");
      if (GPS.month < 10) { Serial.print('0'); }
      Serial.print(GPS.month, DEC);
      Serial.print("-");
      if (GPS.day < 10) { Serial.print('0'); } 
      Serial.print(GPS.day, DEC);
      Serial.print("--");
      if (GPS.hour < 10) { Serial.print('0'); }
      Serial.print(GPS.hour, DEC);
      Serial.print("-");
      if (GPS.minute < 10) { Serial.print('0'); }
      Serial.print(GPS.minute, DEC); 
      Serial.print("-");
      if (GPS.seconds < 10) { Serial.print('0'); }
      Serial.print(GPS.seconds, DEC); 
      Serial.print(".");
      Serial.println(GPS.milliseconds);

      // Does not support ms (but GPS do)
      timeStamp = DateTime(GPS.year,GPS.month,GPS.day, GPS.hour, GPS.minute, GPS.seconds); // MUST be global!!!!! or it won't update
    //  DateTime now = DateTime(2000,01,01, 00, 00, 00); // <DEBUG>
    //setTime(hr,min,sec,day,month,yr); // Another way to set
  
    Serial.print("Time from GPS: ");
    Serial.println(timeStamp.toString(timeStampFormat_Line));
      
//      Serial.print("MS: ");
//      Serial.println(GPS.milliseconds);
      
    }
  
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
