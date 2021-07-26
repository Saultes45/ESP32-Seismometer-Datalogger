/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : Nathanaël Esnault
* Creation date : 2021-07-26
* Version       : 0.1 (finished on 2021-..-..)
* Modifications :
* Known bugs    :
*
*
* Possible Improvements
*
* Notes
*
*
* Ressources (Boards + Libraries Manager)
*
*
* TODO
*
* ========================================
*/


// -------------------------- Defines and Const --------------------------

//GPS

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
  const uint32_t  GPS_NO_FIX_TIMEOUT_MS   = 10 * S_TO_MS_FACTOR;   // 
  const uint32_t  GPS_NEW_FILE_TIMEOUT_MS = 2 * S_TO_MS_FACTOR;   // 


// -------------------------- Global Variables --------------------------

// GPS
const uint8_t GPS_BOOST_ENA_PIN     = 21;      // To turn the BOOST converter of the GPS ON and OFF
bool          GPSNeeded             = false;  // Used in the mail loop to descide if the GPS I2C buffer should be depiled
bool          GPSTimeAvailable      = false;  // Used in logging to know if we can use the lastest GPS timestamp string
String        lastGPSTimestamp      = "";




// END OF THE FILE
