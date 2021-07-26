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

// RS1D
const uint8_t       NBR_ACCELERATIONS_PER_MESSAGE     =  50;     // Scope controlled  + cannot be reassigned
#define             RX_BUFFER_SIZE                       650     // 640 actually needed, margin of 10 observed
const unsigned long RS1D_DEPILE_TIMEOUT_MS            =  100;    // in [ms]
const uint16_t      RS1D_WARMUP_TIME_MS               =  5000;   // in [ms]
#define             GEOPHONE_BAUD_RATE                   230400  // Baudrate in [bauds] for serial communication with the Geophone
#define             GEOPHONE_TIMEOUT_MS                  100     // For the character search in buffer, in [ms]
#define             GeophoneSerial                       Serial1 // Use the 2nd (out of 3) hardware serial of the ESP32
const uint8_t       RS1D_PWR_PIN_1                    =  14;     // To turn the geophone ON and OFF


// -------------------------- Global Variables --------------------------


// RS1D
char      RS1Dmessage[RX_BUFFER_SIZE];                              // Have you seen the sheer size of that!!! Time...to die.
bool      goodMessageReceived_flag                        = false;  // Set to "bad message" first
int32_t   seismometerAcc[NBR_ACCELERATIONS_PER_MESSAGE];
uint8_t   nbr_bumpDetectedLast                            = 0;      // Max of 50; // This should be the output of a function and should be local



// END OF THE FILE