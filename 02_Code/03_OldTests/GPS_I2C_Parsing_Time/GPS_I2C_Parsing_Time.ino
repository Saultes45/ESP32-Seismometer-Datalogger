
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

#define SERIAL_VERBOSE

#include <Adafruit_GPS.h>

// Connect to the GPS on the hardware I2C port
Adafruit_GPS GPS(&Wire);

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO false

uint32_t lastTime = millis(); // Just for display <REMOVE>
uint32_t timer = millis();// Just for display <REMOVE>

// My variables
char    timeStampFormat_Line[]      = "YYYY_MM_DD__hh_mm_ss";   // naming convention for EACH LINE OF THE FILE logged to the SD card
char    timeStampFormat_FileName[]  = "YYYY_MM_DD__hh_mm_ss";   // naming convention for EACH FILE NAME created on the SD card
String      fileName              = "";           // Name of the current opened file on the SD card
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

  //  GPS.flush();

  // Set up communication
  // ---------------------
  GPS.sendCommand(PMTK_SET_BAUD_115200); // Ask the GPS to send us data as 115200
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ); // 10 Hz update rate (message only)
  //GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);  // Can't fix position faster than 5 times a second



  // Just once, request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);
  delay(1000);
  //  #ifdef SERIAL_VERBOSE
  //    while (GPS.available())
  //    {
  //      Serial.print(GPS.read());
  //    }
  //  #endif

  //  Just once, request firmware version
  GPS.println(PMTK_Q_RELEASE);
  delay(1000);
  //    #ifdef SERIAL_VERBOSE
  //    while (GPS.available())
  //    {
  //      Serial.print(GPS.read());
  //    }
  //  #endif

  // Turn on RMC (recommended minimum) and GGA (fix data) including altitude
  //  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);


}


// -------------------------- Loop --------------------------
void loop() // run over and over again
{
  // read data from the GPS in the 'main loop'
  char c = GPS.read();

  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {


    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
    return; // we can fail to parse a sentence in which case we should just wait for another

    if(not(GPS.fix))
    {
      Serial.print("Fix: ");
      Serial.print((int)GPS.fix);
      Serial.print(" quality: ");
      Serial.println((int)GPS.fixquality);
    }
    else
    {
      // My time formating
      //------------------
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
      dataString += String(GPS.seconds);
      dataString += ".";

//      if (GPS.milliseconds < 10) 
//      {
//        dataString += "00";
//      } 
//      else if (GPS.milliseconds > 9 && GPS.milliseconds < 100) 
//      {
//        dataString += "0";
//      }
      dataString += String(GPS.milliseconds);


      dataString += FORMAT_SEP;

      #ifdef SERIAL_VERBOSE
      Serial.print("Timestamp: ");
      Serial.println(dataString);
      #endif
    }
  }
}
