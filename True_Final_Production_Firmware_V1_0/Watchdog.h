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


// WATCHDOG
//DateTime lastWatchdogTrigger;     // <NOT YET USED>
//volatile uint32_t nbr_WatchdogTrigger = 0;
//RTC_DATA_ATTR unsigned long nbr_WatchdogTrigger =0;
/*
 * The attribute RTC_DATA_ATTR tells the compiler that 
 * the variable should be stored in the real 
 * time clock data area. This is a small area of 
 * storage in the ESP32 processor that is part of 
 * the Real Time Clock. This means that the value will be 
 * set to zero when the ESP32 first powers up but will 
 * retain its value after a deep sleep. 
 */



// -------------------------- Defines and Const --------------------------
// WATCHDOG

const uint8_t   wdtTimeout      = 4;    // WDT timeout in [s]
const uint8_t   MAX_NBR_WDT     = 10;   // Recurring WDT threshold on counter
const int       WDT_SLP_RECUR_S = 1800; // Time to sleep if WDT triggers more than the threshold //Be careful for the type as the esprintf doesn't like some of them

// -------------------------- Global Variables --------------------------

hw_timer_t *    timer           = NULL;

// END OF THE FILE
