/* ========================================
*
* Copyright Tonkin and Taylor Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Iain Billington, Nathanaël Esnault
* Verified by   : N/A
* Creation date : 2021-04-07
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
* Ressources
*
*
* TODO
*
*
* ========================================
*/




// -------------------------- Defines --------------------------

// CPU frequency
#define TARGET_CPU_FREQUENCY          80

// Global
#define CONSOLE_BAUD_RATE             115200  // Baudrate in [bauds] for serial communication to the console (used for debug only) TX0(GPIO_1) RX0(GPIO_3)
#define SERIAL_DEBUG        //if defined then the ESP32 will verbose to the console

// Deep sleep
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  2       /* Time ESP32 will go to sleep (in seconds) */

// SD
#define USE_SD             // if defined then the ESP32 will log data to SD card (if not it will just read IMU) // <Not coded yet>
#define SOM_LOG     '$'   // Start of message
#define FORMAT_SEP  ','   // separator between the different filed so that the data can be read
#define MAX_LINES_PER_FILES 1000//36000   // maximum number of lines that we want stored in 1 SD card file. It should be about 1h worth


// -------------------------- Global variables ----------------

char timeStampFormat_Line[]     = "YYYY_MM_DD__hh_mm_ss"; // naming convention for each line of the file logged to the SD card
char timeStampFormat_FileName[] = "YYYY_MM_DD__hh_mm_ss"; // naming convention for each file name created on the SD card

// -------------------------- Pins ------------------------------

#define PIN_CS_SD    33     // Chip select for SPI for SD card
#define PIN_TX_STM   10     // Transmit data line from the STM geophone (2nd ESP32 HW UART)
#define PIN_RX_STM   09     // Receive  data line from the STM geophone (2nd ESP32 HW UART)
#define PIN_PPS_GPS  33     // Pulse Per Second trigger (us precision rising edge) from the GPS
#define PIN_PWR_GPS  33     // Pin that goes into the V_in (3.3&5V tolerant) of the GPS that is used to turn it off for power saving
#define PIN_PW_STM   33     // Pin that goes into the V_in (3.3&5V tolerant) of the GPS that is used to turn it off for power saving

/*
|  UART |  RX IO |  TX IO |  CTS  |   RTS  |
|:-----:|:------:|:------:|:-----:|:------:|
| UART0 |  GPIO3 |  GPIO1 |  N/A  |   N/A  |
| UART1 |  GPIO9 | GPIO10 | GPIO6 | GPIO11 |
| UART2 | GPIO16 | GPIO17 | GPIO8 |  GPIO7 |
*/

// -------------------------- Structs --------------------------
//NONE


// END OF FILE
