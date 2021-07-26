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
// WATCHDOG
hw_timer_t *    timer           = NULL;
const uint8_t   wdtTimeout      = 4;    // WDT timeout in [s]
const uint8_t   MAX_NBR_WDT     = 10;   // Recurring WDT threshold on counter
const int       WDT_SLP_RECUR_S = 1800; // Time to sleep if WDT triggers more than the threshold //Be careful for the type as the esprintf doesn't like some of them

// -------------------------- Global Variables --------------------------

// END OF THE FILE
