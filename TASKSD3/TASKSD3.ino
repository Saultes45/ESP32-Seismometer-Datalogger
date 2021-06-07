

#define SERIAL_DEBUG

// RTC
#include <RTClib.h> // use the one from Adafruit, not the forks with the same name

// -------------------------- SD card ----------------
// 1 file = 1.67min recording @ 10Hz = 1,000 lines per files ~ 79KB
// If we lose a file, or it gets corrupted, we only lose 1.67min worth of data
uint16_t  cntLinesInFile  = 0; // Written at the end of a file for check (36,000 < 65,535)
uint32_t  cntFile         = 0; // Counter that counts the files written in the SD card this session (we don't include prvious files), included in the name of the file, can handle 0d to 99999d (need 17 bits)
String    fileName        = "";// Name of the current opened file on the SD card

#define MAX_LINES_PER_FILES 25 // Maximum number of lines that we want stored in 1 SD card file. It should be about 1h worth

// SD card
#include <SPI.h>
#include <SD.h>
File                dataFile;              // Only 1 file can be opened at a certain time, <KEEP GLOBAL>

char timeStampFormat_Line[]     = "YYYY_MM_DD__hh_mm_ss";

// -------------------------- Buffer ----------------
// Array of strings
#define STRINGS_ARRAY_SIZE 50               // Usually a max of 11 values
// Declare a REAL array of Strings, and NOT an array of pointers to Strings
// Careful because of easy memory fragmentation
String  dataToWrite[STRINGS_ARRAY_SIZE];
uint8_t globalSharedBufferCurrentIndex = 0; // Usually a max of 11 values
bool    overflowSharedBuffer = false;       // Tells if we have reached the limit of space in the shared buffer


#define US_TO_MS_CONVERSION           1000    // For the ISR task
#define WAIT_LOOP_MS                  100     // For the delay to wait, in [ms] at the end of the loop

// -------------------------- ISR ----------------

hw_timer_t * timerTask = NULL;

volatile bool isrExecTaskFlag = false;

// Tells when to execute the task
void IRAM_ATTR onTimer(){

  // Set the flag that is going to be read in the loop
  isrExecTaskFlag = true; 
  
}

// -------------------------- Tasks ----------------

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif


// define two tasks
void TaskRetrieveData ( void *pvParameters );
void TaskWriteToSD    ( void *pvParameters );

// define the shared buffer between Task#1 and Task#2 and a index on the array


// -------------------------- Functions declaration --------------------------

void createNewFile                  (void);                 // Create a name a new DATA file on the SD card, file variable is global


// -------------------------- Set up --------------------------

void setup() {
// initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  // Now set up two tasks to run independently
  //--------------------------------------------

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
    ,  1  // Priority
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.

  // Starting the task timer
  //------------------------
  // Use 2nd timer out of 4 (counted from zero)
  // Set 80 divider for prescaler
  timerTask = timerBegin(0, 80, true);
  // Attach onTimer function to this timer
  timerAttachInterrupt(timerTask, &onTimer, true);
  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the ISR function (third parameter)
  timerAlarmWrite(timerTask, US_TO_MS_CONVERSION * WAIT_LOOP_MS, true); // This should give us an accurate 10Hz


  // Enable all ISRs
  //----------------

  timerAlarmEnable(timerTask); // Task#1

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

 /* Block for 10ms. */
 const TickType_t xDelay = 10 / portTICK_PERIOD_MS;

pinMode(32, OUTPUT);

int sensorPin   = A0;    // Select the input pin for the potentiometer
int sensorValue = 0;     // variable to store the value coming from the sensor

  for (;;) // A Task shall never return or exit.
  {

    digitalWrite(32, HIGH);   // indicates the start of Task#1


    // Check if the time to collect the data has ellpased thanks to the flag set up in the ISR
    if (isrExecTaskFlag)
    {
      // read the value from the sensor:
      sensorValue = analogRead(sensorPin);

      // #ifdef SERIAL_DEBUG
      //   Serial.println("Caught interrupt");
      // #endif
  
      // Check if we have reached the limit size of the buffer, if we do, we have an overflow (this is REALLY bad)
      if (globalSharedBufferCurrentIndex >= STRINGS_ARRAY_SIZE)
      {
        overflowSharedBuffer = true;
        // do not put any data in the buffer, they are lost forever
      }
      else
      {
        overflowSharedBuffer = false;
        
        // Put the value in the buffer
        dataToWrite[globalSharedBufferCurrentIndex] = String(sensorValue);
    
        globalSharedBufferCurrentIndex ++; // Increment the counter (currently points on nothing)

        #ifdef SERIAL_DEBUG
          Serial.println("Data put in the buffer");
        #endif

        vTaskDelay( 80 / portTICK_PERIOD_MS ); 

      } 
    }
    else
    {
      vTaskDelay( 1 / portTICK_PERIOD_MS ); // xDelay is the amount of time, in tick periods, that the calling task should block itself, freeing CPU cycles for the other competing tasks
    }
    

    digitalWrite(32, LOW);   // indicates the end of Task#1
    
    
  }
}


//*************************************************************************************************
void TaskWriteToSD(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

  /*
  Explanation of what it does
  */

  /* Block for 100ms. */
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;

  pinMode(14, OUTPUT);


  for (;;) // A Task shall never return or exit.
  {

    digitalWrite(14, HIGH);   // indicates the start of Task#2

    // Check if there are any data in the buffer by reading the counter
    
    #ifdef SERIAL_DEBUG
      Serial.printf("There are %d Strings in the buffer\r\n", globalSharedBufferCurrentIndex);
    #endif
    
    while (globalSharedBufferCurrentIndex > 0)
    {
      // Log data in the SD card
      //------------------------
  
      // Check if the file is available
      if (dataFile) 
      {
        // We do it like this because of the "\r\n" not desired at the end of a file
        if (cntLinesInFile >= MAX_LINES_PER_FILES - 1) // Check if we have reached the max. number of lines per file
        { 
          // Boost the frequency of the CPU to the MAX so that the writing takes less time
          //setCpuFrequencyMhz(MAX_CPU_FREQUENCY);
          // Write to the file w/out the "\r\n"
          dataFile.print(dataToWrite[globalSharedBufferCurrentIndex]);
          dataToWrite[globalSharedBufferCurrentIndex] = ""; // Clear the String we just wrote on the SD card
          globalSharedBufferCurrentIndex --; // make the index point on the previous String in the buffer
          // Close the file
          dataFile.close();
          // Reset the line counter
          cntLinesInFile = 0;
          // Create a new file
          createNewFile();
          #ifdef SERIAL_DEBUG
            Serial.println("Reached the max number of lines per file, starting a new one");
          #endif
          // Limit back the frequency of the CPU to consume less power
          //setCpuFrequencyMhz(TARGET_CPU_FREQUENCY);
      }
      else // wE ARE STILL UNDER THE LIMIT OF NUMBER OF LINES PER FILE
      {
        dataFile.println(dataToWrite[globalSharedBufferCurrentIndex]);
        dataToWrite[globalSharedBufferCurrentIndex] = ""; // Clear the String we just wrote on the SD card
        globalSharedBufferCurrentIndex --; // make the index point on the previous String in the buffer
        cntLinesInFile++; // Increment the lines-in-current-file counter
    
  //      #ifdef SERIAL_DEBUG
  //        Serial.println("Data have been written");
  //        Serial.print("Current number of lines: ");
  //        Serial.print(cntLinesInFile);
  //        Serial.print("/");
  //        Serial.println(MAX_LINES_PER_FILES);
  //      #endif
      }
        
      }
    // If the file isn't open, pop up an error
    else {
//      #ifdef SERIAL_DEBUG
//        Serial.print("Error writting to the file: ");
//        Serial.println(fileName);
//      #endif
      fileName = "";        // Reset the filename
      cntLinesInFile = 0;   // Reset the lines-in-current-file counter
    }
      
    }
    // Else then there are no data to save
    
    digitalWrite(14, LOW);   // indicates the end of Task#2
    
    vTaskDelay( xDelay ); // xDelay is the amount of time, in tick periods, that the calling task should block itself, freeing CPU cycles for the other competing tasks
  }
}


//******************************************************************************************
void createNewFile(void) {

  cntFile ++; // Increment the counter of files
  
  // To name the file we need to know the date : ask the RTC
//  timestampForFileName = now(); // MUST be global!!!!! or it won't update
  fileName = "";                    // Reset the filename
  fileName += "/";                  // To tell it to put in the root folder, absolutely necessary


  
  //fileName += timestampFile.toString(timeStampFormat_Line); // <-----------------------

  fileName += "-"; // Add a separator between datetime and filenumber

  char buffer[5];
  sprintf(buffer, "%05d", cntFile); // Making sure the file number is always printed with 5 digits
  //  Serial.println(buffer);
  fileName += String(buffer); 

  fileName += ".txt"; // Add the file extension


  if (SD.exists(fileName)) {
    //#ifdef SERIAL_DEBUG
      //Serial.print("Looks like a file already exits on the SD card with that name: ");
      //Serial.println(fileName);
   //#endif
  }

  // Open the file. Note that only one file can be open at a time,
  // so you have to close this one before opening another.
//  #ifdef SERIAL_DEBUG
//    Serial.print("Creating the following file on the SD card: ");
//    Serial.println(fileName);
// #endif
  dataFile = SD.open(fileName, FILE_WRITE);

}
