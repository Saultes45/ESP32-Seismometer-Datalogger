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

// STATE MACHINE
#ifndef STATES_H

#define STATES_H
#define STATE_OVERWATCH      (3u)
#define STATE_SLEEP          (2u)
#define STATE_LOG            (1u)
#define STATE_EMPTY          (0u)

#endif /* STATES_H */



#define NBR_OVERWATCH_BEFORE_ACTION     20
#define NBR_BUMPS_DETECTED_BEFORE_LOG   16
#define NBR_LOG_BEFORE_ACTION           40
#define TIME_TO_SLEEP                   10          // In [s], time spent in the deep sleep state

// Bump detection
const int32_t BUMP_THRESHOLD_POS =      +100000;
const int32_t BUMP_THRESHOLD_NEG =      -100000;



// -------------------------- Global Variables --------------------------

// State Machine
volatile  uint8_t     currentState            = STATE_EMPTY;  // Used to store which step we are at, default is state "empty"
volatile  uint8_t     nextState               = STATE_EMPTY;  // Used to store which step we are going to do next, default is state "empty"
uint8_t     cnt_Overwatch           = 0;            // This does NOT need to survive reboots NOR ISR, just global
uint8_t     cnt_Log                 = 0;            // This does NOT need to survive reboots NOR ISR, just global
uint16_t    nbr_bumpDetectedTotal   = 0;            // <KEEP GLOBAL>, max of 50*nbr overwatch
uint8_t     nbr_messagesWithBumps   = 0;            // <KEEP GLOBAL>, max of nbr overwatch


// END OF THE FILE
