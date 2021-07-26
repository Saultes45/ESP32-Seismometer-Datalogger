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

// -------------------------- Global Variables --------------------------

// LOG + SD
char        timeStampFormat_Line[]      = "YYYY_MM_DD__hh_mm_ss";   // naming convention for EACH LINE OF THE FILE logged to the SD card
char        timeStampFormat_FileName[]  = "YYYY_MM_DD__hh_mm_ss";   // naming convention for EACH FILE NAME created on the SD card
uint16_t    cntLinesInFile              = 0;                        // Written at the end of a file for check (36,000 < 65,535)
uint32_t    cntFile                     = 0;                        // Counter that counts the files written in the SD card this session (we don't include prvious files), included in the name of the file, can handle 0d to 99999d (need 17 bits)
String      fileName                    = "";                       // Name of the current opened file on the SD card
File        dataFile;                                               // Only 1 file can be opened at a certain time, <KEEP GLOBAL>


// END OF THE FILE
