
/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : N/A
* Creation date : 2021-06-07 (Queen's b-day)
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
*
* TODO
*
*
*/

#include <Adafruit_GPS.h>

// Connect to the GPS on the hardware I2C port
Adafruit_GPS GPS(&Wire);

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO false

uint32_t lastTime = millis(); // Just for display <REMOVE>

// My variables
char    timeStampFormat_Line[]      = "YYYY_MM_DD__hh_mm_ss";   // naming convention for EACH LINE OF THE FILE logged to the SD card
char    timeStampFormat_FileName[]  = "YYYY_MM_DD__hh_mm_ss";   // naming convention for EACH FILE NAME created on the SD card
String      fileName              = "";           // Name of the current opened file on the SD card
File        dataFile;                               // Only 1 file can be opened at a certain time, <KEEP GLOBAL>
#define SOM_LOG         '$' 
#define FORMAT_SEP      ',' 

// -------------------------- Set up --------------------------

void setup()
{

#ifdef SERIAL_VERBOSE
  Serial.begin(115200);
  Serial.println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"); // Indicates a wakeup to the console
  Serial.println("I2C GPS test");
#endif

  GPS.begin(0x10);  // The I2C address to use is 0x10


  // Turn OFF all GPS output
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_OFF);

  GPS.flush();

  // Set up communication
  // ---------------------
  GPS.sendCommand(PMTK_SET_BAUD_115200); // Ask the GPS to send us data as 115200
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ); // 10 Hz update rate (message only)
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);  // Can't fix position faster than 5 times a second



  // Just once, request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);
  delay(1000);

  //  Just once, request firmware version
  GPS.println(PMTK_Q_RELEASE);
  delay(1000);

  // Turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);

}


// -------------------------- Loop --------------------------
void loop() // run over and over again
{

  //  char c = GPS.read();
  //  // if you want to debug, this is a good time to do it!
  //  
  //  if (c) 
  //  {
  //    Serial.print(c);
  //  }

  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) 
  {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    // so be very wary if using OUTPUT_ALLDATA and trying to print out data
    #ifdef SERIAL_VERBOSE
      Serial.println(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
    #endif
    
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
    {
      return; // we can fail to parse a sentence in which case we should just wait for another
    }
    
  }

  // approximately every 2 seconds or so, print out the current stats
  if (millis() - lastTime > 2000) 
  {
    lastTime = millis(); // reset the timer

    //Serial.print(" quality: "); Serial.println((int)GPS.fixquality);
    Serial.print("Fix: "); Serial.print((int)GPS.fix);

    #ifdef SERIAL_VERBOSE
    if (GPS.fix) 
    {
      Serial.println("We have a fix");
    }

    Serial.print("Datetime: ");

    Serial.print("20");
    Serial.print(GPS.year, DEC);
    Serial.print("_");
    Serial.print(GPS.month, DEC);
    Serial.print("_");
    Serial.print(GPS.day, DEC);
    Serial.print("__"); 
    
   
    if (GPS.hour < 10) { Serial.print('0'); }
    Serial.print(GPS.hour, DEC); 
    Serial.print('_');
    
    if (GPS.minute < 10) { Serial.print('0'); }
    Serial.print(GPS.minute, DEC);
    Serial.print('_');
    
    if (GPS.seconds < 10) { Serial.print('0'); }
    Serial.print(GPS.seconds, DEC); 
    Serial.print('.');
    
    if (GPS.milliseconds < 10) 
    {
      Serial.print("00");
    } 
    else if (GPS.milliseconds > 9 && GPS.milliseconds < 100) 
    {
      Serial.print("0");
    }
    Serial.println(GPS.milliseconds);
    #endif


    // My time formating
    //----------------------------------------------------------------------------------
    // Create a string for assembling the data to log
    String dataString  = "";
    String myTimestamp = "";
  
    // Add start of message charachter
    //--------------------------------
    dataString += SOM_LOG;
  
    // Read the GPS time
    //------------------------------------------
    
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
    dataString += String(GPS.second);
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


    dataString += FORMAT_SEP;

    #ifdef SERIAL_VERBOSE
      Serial.print(dataString);
    #endif

  }
}
