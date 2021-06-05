


#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include <math.h> // for the stress test

// define two tasks
void TaskRetrieveData ( void *pvParameters );
void TaskWriteToSD    ( void *pvParameters );




void setup() {
  
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(
    TaskRetrieveData
    ,  "TaskRetrieveData"   // A name just for humans
    ,  1024*10  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskWriteToSD
    ,  "TaskWriteToSD"
    ,  1024*10  // Stack size
    ,  NULL
    ,  2  // Priority
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.

}

void loop() {

  // Empty. Things are done in Tasks.

}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

//*************************************************************************************************
void TaskRetrieveData(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

/*
 Explanation of what it does
*/

 /* Block for 50ms. */
 const TickType_t xDelay = 50 / portTICK_PERIOD_MS;

pinMode(32, OUTPUT);

bool stateTask = false;

  for (;;) // A Task shall never return or exit.
  {

    digitalWrite(32, HIGH);   // turn the LED on (HIGH is the voltage level)
    
    stateTask = not(stateTask); // toggle state

    //Fake some code execution: look busy
    uint32_t cnt_fake = 0;
    double fakeVariable = 0.0;

      for (cnt_fake = 0; cnt_fake < 1000; cnt_fake++)
      {
        fakeVariable += exp(atan(cnt_fake));
      }

//
//    if (stateTask)
//    {
//      digitalWrite(32, HIGH);   // turn the LED on (HIGH is the voltage level)
//    }
//    else
//    {
//      digitalWrite(32, LOW);   // turn the LED on (HIGH is the voltage level)
//    }

digitalWrite(32, LOW);   // turn the LED on (HIGH is the voltage level)
    
    vTaskDelay( xDelay ); // xDelay is the amount of time, in tick periods, that the calling task should block itself, freeing CPU cycles for the other competing tasks
  }
}

//*************************************************************************************************
void TaskWriteToSD(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

/*
 Explanation of what it does
*/

 /* Block for 50ms. */
 const TickType_t xDelay = 50 / portTICK_PERIOD_MS;

pinMode(14, OUTPUT);

bool stateTask = false;

  for (;;) // A Task shall never return or exit.
  {

    digitalWrite(14, HIGH);   // turn the LED on (HIGH is the voltage level)
    
    stateTask = not(stateTask); // toggle state

    //Fake some code execution: look busy
    uint32_t cnt_fake = 0;
    double fakeVariable = 0.0;

      for (cnt_fake = 0; cnt_fake < 200; cnt_fake++)
      {
        fakeVariable += exp(atan(cnt_fake));
      }

//
//    if (stateTask)
//    {
//      digitalWrite(14, HIGH);   // turn the LED on (HIGH is the voltage level)
//    }
//    else
//    {
//      digitalWrite(14, LOW);   // turn the LED on (HIGH is the voltage level)
//    }

digitalWrite(14, LOW);   // turn the LED on (HIGH is the voltage level)
    
    vTaskDelay( xDelay ); // xDelay is the amount of time, in tick periods, that the calling task should block itself, freeing CPU cycles for the other competing tasks
  }
}
