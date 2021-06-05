/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : N/A
* Creation date : 2021-06-03 (graduation+Queen's b-day)
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
* TODO
*
* Terminology
* ------------
* STM = Seismometer = Geophone = RS1D
*
*/

//RPI Rx buffer
#define RX_BUFFER_SIZE                  (650u)  //640 actually needed, margin of 10 observed
char    RS1Dmessage[RX_BUFFER_SIZE]; ////Have you seen the sheer size of that!!! Time...to die.

#define GEOPHONE_BAUD_RATE             230400  // Baudrate in [bauds] for serial communication with the Geophone TX??(GPIO_17) RX??(GPIO_16)
//insert complete pins table here

// Use the 2nd (out of 3) hardware serial
#define GeophoneSerial Serial1

#define RS1D_PWR_PIN_1                      25u      // To turn the geophone ON and OFF
#define RS1D_PWR_PIN_2                      26u      // To turn the geophone ON and OFF

// Bump detection
const int32_t BUMP_THRESHOLD_POS = +100000;           
const int32_t BUMP_THRESHOLD_NEG = -100000;


bool goodMessageReceived_flag  = false; // Set to "bad message" first
int32_t seismometerAcc[50];

uint8_t nbr_bumpDetectedLast = 0u; //  max of 50; // This should be the output of a function and should be local 


void parse_SR(void);
void readRS1DBuffer(void);
void parseGeophoneData(void);
uint32_t  hex2dec                     (char * a);


//String Formatting
#include <string.h>
#include <stdio.h>

//SD card
#include <SPI.h>
#include <SD.h>
uint16_t  cntLinesInFile  = 0; // Written at the end of a file for check (36,000 < 65,535)
uint32_t  cntFile         = 0; // Counter that counts the files written in the SD card this session (we don't include prvious files), included in the name of the file, can handle 0d to 99999d (need 17 bits)
String    fileName        = "";// Name of the current opened file on the SD card
// LOG
char timeStampFormat_Line[]     = "YYYY_MM_DD__hh_mm_ss"; // naming convention for EACH LINE OF THE FILE logged to the SD card
char timeStampFormat_FileName[] = "YYYY_MM_DD__hh_mm_ss"; // naming convention for EACH FILE NAME created on the SD card
// LOG
#define USE_SD                     // If defined then the ESP32 will log data to SD card (if not it will just read IMU) // <Not coded yet>
#define PIN_CS_SD       33        // Chip Select (ie CS/SS) for SPI for SD card
const char SOM_LOG = '$';       // Start of message indicator, mostly used for heath check (no checksum)
#define FORMAT_TEMP     1         // Numbers significative digits for the TEMPERATURE
#define FORMAT_ACC      6         // Numbers significative digits for the ACCELEROMETERS
#define FORMAT_GYR      6         // Numbers significative digits for the GYROSCOPES
const char FORMAT_SEP = ',';      // Separator between the different files so that the data can be read/parsed by softwares
#define FORMAT_END      "\r\n"    // End of line for 1 aquisition, to be printed in the SD card // <Not used>
const uint16_t MAX_LINES_PER_FILES = 40; // Maximum number of lines that we want stored in 1 SD card file. It should be about 1h worth
#define SESSION_SEPARATOR_STRING "----------------------------------"
// SD
File                dataFile;              // Only 1 file can be opened at a certain time, <KEEP GLOBAL>


// RTC
#include <RTClib.h> // use the one from Adafruit, not the forks with the same name
//RTC
RTC_PCF8523         rtc;
DateTime            time_loop;             // MUST be global!!!!! or it won't update
DateTime            timestampForFileName;  // MUST be global!!!!! or it won't update

//--------------------------------------------------
void setup() {

  // Declare pins
  // ----------------------------------------
  pinMode (RS1D_PWR_PIN_1    , OUTPUT);
  pinMode (RS1D_PWR_PIN_2    , OUTPUT);

  for (uint8_t cnt_fill=0; cnt_fill < 50; cnt_fill++)
  {
    seismometerAcc[cnt_fill] = (int32_t)0;
  }

  Serial.begin(115200);

  // Do some preliminary tests
  // -------------------------
   Serial.println("***********************************************************************"); //Indicates via the console that a new cycle started 
  testRTC();
  testSDCard();  

      
  // Start the HW serial for the Geophone/STM
  // ----------------------------------------
  GeophoneSerial.begin(GEOPHONE_BAUD_RATE);
  GeophoneSerial.setTimeout(100);// Set the timeout to 100 milliseconds (for findUntil)

  digitalWrite(RS1D_PWR_PIN_1, HIGH);
  digitalWrite(RS1D_PWR_PIN_2, HIGH);

  // Create the first file
  //----------------------
  createNewFile();

}

//--------------------------------------------------
void loop() 
{
    readRS1DBuffer();
    if (goodMessageReceived_flag)
    {
      goodMessageReceived_flag = false; // Reset the flag
      nbr_bumpDetectedLast = (uint8_t)0; //Reset


    // Create a string for assembling the data to log
    String dataString  = "";
    String myTimestamp = "";
  
    // Add start of message character
    //--------------------------------
    dataString += SOM_LOG;

    // Read the RTC time
    //------------------------------------------
    time_loop = rtc.now(); // MUST be global!!!!! or it won't update
    // We are obliged to do that horror because the method "toString" input parameter is also the output
    char timeStamp[sizeof(timeStampFormat_Line)]; 
    strncpy(timeStamp, timeStampFormat_Line, sizeof(timeStampFormat_Line));
    dataString += time_loop.toString(timeStamp); 
    dataString += FORMAT_SEP; 
      
      for (uint8_t cnt_Acc = 0; cnt_Acc < 50; cnt_Acc++)
      {
        //Serial.println(seismometerAcc[cnt_Acc]);
        

        /* Display the results (acceleration is measured in ???) */
        dataString += String(seismometerAcc[cnt_Acc]); 
        dataString += FORMAT_SEP;
      }
      Serial.println("Data going to SD card: ");
      Serial.println(dataString);
      Serial.flush();

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
        dataFile.print(dataString);
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
      dataFile.println(dataString);
      cntLinesInFile++; // Increment the lines-in-current-file counter
  
      #ifdef SERIAL_DEBUG
        Serial.println("Data have been written");
        Serial.print("Current number of lines: ");
        Serial.print(cntLinesInFile);
        Serial.print("/");
        Serial.println(MAX_LINES_PER_FILES);
      #endif
    }
      
    }
    // If the file isn't open, pop up an error
    else {
      #ifdef SERIAL_DEBUG
        Serial.print("Error writting to the file: ");
        Serial.println(fileName);
      #endif
      fileName = "";        // Reset the filename
      cntLinesInFile = 0;   // Reset the lines-in-current-file counter
    }
      
    } 
}


//--------------------------------------------------


//******************************************************************************************
void readRS1DBuffer(void)
{
  uint16_t  max_nbr_depiledChar = 500;
  uint16_t  cnt_savedMessage = 0;
  char      temp             = 0; // Init must be different from SOM_CHAR_SR

  if (GeophoneSerial.available()) 
  {
    if(GeophoneSerial.find("[\""))// test the received buffer for SOM_CHAR_SR
    {
      
      cnt_savedMessage = 0;
      RS1Dmessage[cnt_savedMessage] = '[';
      cnt_savedMessage ++;
      RS1Dmessage[cnt_savedMessage] = '\"';
      cnt_savedMessage ++;

      unsigned long startedWaiting    = millis();
      unsigned long howLongToWait     = 100; // in [ms]
      while((RS1Dmessage[cnt_savedMessage-1] != ']') && (millis() - startedWaiting <= howLongToWait) && (cnt_savedMessage < RX_BUFFER_SIZE))
      {
        if (GeophoneSerial.available())
        {
          RS1Dmessage[cnt_savedMessage] = GeophoneSerial.read();
          cnt_savedMessage++;
        }
      }

      if ((RS1Dmessage[cnt_savedMessage-1] == ']')) // if any SOM found then we continue
      {
        delay(1);
        parseGeophoneData();
      }
    }
    memset(RS1Dmessage, 0, RX_BUFFER_SIZE); // clean the message field anyway
  }
}


//******************************************************************************************
void parseGeophoneData(void)
{
  if (strstr(RS1Dmessage, "\"]"))
  {
    const uint8_t  max_nbr_extractedCharPerAcc = 9; // scope controlled  + cannot be reassigned
    char    temp1[max_nbr_extractedCharPerAcc];
    uint8_t cnt_accvalues = 0;
    char    *p = RS1Dmessage;
    uint8_t cnt = 0;

    nbr_bumpDetectedLast = 0; //RESET

      for (uint8_t cnt_fill=0; cnt_fill < 50; cnt_fill++)
      {
        seismometerAcc[cnt_fill] = (int32_t)0;
      }

    // Parse the aquited message to get 1st acceleration (1st is different)
    //----------------------------------------------------------------------
    p = strchr(p, '[')+1;
    p++; // Avoid the "
    
    memset(temp1,'\0', max_nbr_extractedCharPerAcc);
    while((*p != '"') && (cnt < max_nbr_extractedCharPerAcc)) // No error checking... Try not. Do, or do not. There is no try
    {
      temp1[cnt] = *p;
      cnt++;
      p++;
    }
    seismometerAcc[0] = (int32_t)(hex2dec(temp1));
    if((seismometerAcc[0] > BUMP_THRESHOLD_POS) || (seismometerAcc[0] < BUMP_THRESHOLD_NEG))
    {
      nbr_bumpDetectedLast++;
    }

    // Parse the rest of the accelerations
    //-------------------------------------
    for (cnt_accvalues = 1; cnt_accvalues < 50; cnt_accvalues++)
    {
      p = strchr(p, ',')+1;
      p++; //Avoid the "
      cnt = 0;
      memset(temp1, '\0', max_nbr_extractedCharPerAcc);
      while((*p != '"') & (cnt < max_nbr_extractedCharPerAcc))
      {
        temp1[cnt] = *p;
        cnt++;
        p++;
      }
      seismometerAcc[cnt_accvalues] = (int32_t)(hex2dec(temp1));
      if((seismometerAcc[cnt_accvalues] > BUMP_THRESHOLD_POS) || (seismometerAcc[cnt_accvalues] < BUMP_THRESHOLD_NEG))
      {
        nbr_bumpDetectedLast++;
      }
    }
    goodMessageReceived_flag = true; // Set the flag because the function was able to run all the above code
  }
}


//******************************************************************************************
uint32_t hex2dec(char * a) // For the Geophone data stream
{

  // Transforms an hex coded number encoded as a char to a number in decimal
  
  char     c = 0;
  uint32_t n = 0;
  
  while (*a) {
    c = *a++;

    if (c >= '0' && c <= '9') {
      c -= '0';

    } else if (c >= 'a' && c <= 'f') {
      c = (c - 'a') + 10;

    } else if (c >= 'A' && c <= 'F') {
      c = (c - 'A') + 10;

    } else {
      goto INVALID;
    }

    n = (n << 4) + c;
  }

  return n;

INVALID:
  return -1;
}


//******************************************************************************************
void testSDCard(void) {
  Serial.print("Initializing SD card...");
  String testString = "Test 0123456789";

  // see if the card is present and can be initialized:
  if (!SD.begin(PIN_CS_SD)) {
    Serial.println("Card failed, or not present");
    //blinkAnError(2);
    // Don't do anything more: infinite loop just here
    while (1);
  }
  Serial.println("Card initialized");

  if (SD.exists("/00_test.txt")) { // The "00_" prefix is to make sure it is displayed by win 10 explorer at the top
    Serial.println("Looks like a test file already exits on the SD card"); // Just a warning
  }

  // Create and open the test file. Note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("/00_test.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(testString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(testString);
  }
  // If the file isn't open, pop up an error:
  else {
    Serial.println("Error while opening /00_test.txt");
    //blinkAnError(3);
  }

  // Add a separator file (empty but with nice title) so the user knows a new DAQ session started
  //createNewSeparatorFile();

}

//******************************************************************************************
void createNewFile(void) {

  cntFile ++; // Increment the counter of files
  
  // To name the file we need to know the date : ask the RTC
  timestampForFileName = rtc.now(); // MUST be global!!!!! or it won't update
  fileName = "";                    // Reset the filename
  fileName += "/";                  // To tell it to put in the root folder, absolutely necessary

  char timeStamp[sizeof(timeStampFormat_FileName)]; // We are obliged to do that horror because the method "toString" input parameter is also the output
  strncpy(timeStamp, timeStampFormat_FileName, sizeof(timeStampFormat_FileName));
  fileName += timestampForFileName.toString(timeStamp);

  fileName += "-"; // Add a separator between datetime and filenumber

  char buffer[5];
  sprintf(buffer, "%05d", cntFile); // Making sure the file number is always printed with 5 digits
  //  Serial.println(buffer);
  fileName += String(buffer); 

  fileName += ".txt"; // Add the file extension


  if (SD.exists(fileName)) {
    #ifdef SERIAL_DEBUG
      Serial.print("Looks like a file already exits on the SD card with that name: ");
      Serial.println(fileName);
   #endif
  }

  // Open the file. Note that only one file can be open at a time,
  // so you have to close this one before opening another.
  #ifdef SERIAL_DEBUG
    Serial.print("Creating the following file on the SD card: ");
    Serial.println(fileName);
 #endif
  dataFile = SD.open(fileName, FILE_WRITE);

}

//******************************************************************************************
void testRTC(void) {

  Serial.println("Testing the RTC...");

  if (!rtc.begin()) {
//    Serial.println("Couldn't find RTC, is it properly connected?");
    Serial.flush(); // Wait until there all the text for the console have been sent
    abort();
  }

  if (!rtc.initialized() || rtc.lostPower()) {
//    Serial.println("RTC is NOT initialized. Use the NTP sketch to set the time!");
//    Serial.println("This is not an important error");
//    Serial.println("The datalogger might still be able to function properly");
    //blinkAnError(6);
  }

  // When the RTC was stopped and stays connected to the battery, it has
  // to be restarted by clearing the STOP bit. Let's do this to ensure
  // the RTC is running.
  rtc.start();

}



// END OF THE SCRIPT
