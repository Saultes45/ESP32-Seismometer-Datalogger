/* ========================================
*
* Copyright ??
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : ??
* Verified by   : Nathanaël Esnault
* Creation date : 2021-06-07 (Queen's b-day)
* Version       : 1.0 (finished on 2021-..-..)
* Modifications :
* Known bugs    :
*
*
* Possible Improvements
*
* Notes
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

// -------------------------- Defines and Const --------------------------
#define SERIAL_VERBOSE

//// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
//// Set to 'true' if you want to debug and listen to the raw GPS sentences
//#define GPSECHO false

#define SOM_LOG         '$' 
#define FORMAT_SEP      ',' 

const uint8_t GPS_BOOST_ENA_PIN  = 21;  // To turn the BOOST converter of the GPS ON and OFF

// -------------------------- Global Variables --------------------------
// Connect to the GPS on the hardware I2C port
Adafruit_GPS GPS(&Wire);

// -------------------------- Set up --------------------------

void setup()
{

  // GPS power
  //----------
  pinMode (GPS_BOOST_ENA_PIN    , OUTPUT);
  digitalWrite(GPS_BOOST_ENA_PIN, HIGH); // Start the 5V boost convert

  // Serial port
  //------------
  // make this baud rate fast enough so we aren't waiting on it
  Serial.begin(115200);

  Serial.println('');
  Serial.println('');
  Serial.println("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
  Serial.println("Adafruit GPS library I2C test");
  Serial.println('');


  Serial.println("Starting to address the GPS...");
  GPS.begin(0x10);  // The I2C address to use is 0x10


  // Set up communication
  // ---------------------
  GPS.sendCommand(PMTK_SET_BAUD_115200);      // Ask the GPS to send us data at 115200bps
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ); // 10 Hz update rate (message only)
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);  // Can't fix position faster than 5 times a second


  // Turn OFF all GPS output
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_OFF);

    // Turn OFF all GPS output
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_OFF);
  
  // Remove all data in buffer
  unsigned long startedWaiting = millis();
  while((GPS.available()) && (millis() - startedWaiting <= 2000))
  {
      GPS.read(); // don't save the data, just dump
//      Serial.write(GPS.read());    
      Serial.print('.');
  }
  Serial.println(" done");

  
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);


}


// -------------------------- Loop --------------------------
void loop()
{
  // read data from the GPS in the 'main loop'
  char c = GPS.read();

  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) 
  {
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
      //-------------------
      
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
}// END OF LOOP


// END OF SCRIPT
