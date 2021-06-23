


// -------------------------- Includes --------------------------

#define SERIAL_VERBOSE

//ESP32
#include <esp_system.h>
//#include <rom/rtc.h> // reset reason


//String Formatting
#include <string.h>
#include <stdio.h>

//SD card
#include <SPI.h>
#include <SD.h>

//LED
#include <Arduino.h>
#include <Ticker.h>

// Battery
#define BATT_VOLTAGE_DIVIDER_FACTOR   2       // [N/A]
#define LOW_BATTERY_THRESHOLD         3.1     // in [V]
#define BATT_PIN                      35      // To detect the Lipo battery remaining charge, GPIO35 on Adafruit ESP32 (35 on dev kit)

#define TIME_TO_SLEEP_MS               10 // in ms for the loop
// LOG+SD
#define PIN_CS_SD                   33     // Chip Select (ie CS/SS) for SPI for SD card

const uint8_t     LOG_PWR_PIN_1        = 25;    // To turn the geophone ON and OFF
const uint8_t     LOG_PWR_PIN_2        = 26;    // To turn the geophone ON and OFF

const char FORMAT_SEP               = ','; // Separator between the different files so that the data can be read/parsed by softwares
const uint16_t MAX_LINES_PER_FILES    = 125;  // Maximum number of lines that we want stored in 1 SD card file. It should be about ...min worth
uint16_t    cntLinesInFile        = 0;            // Written at the end of a file for check (36,000 < 65,535)
uint32_t    cntFile               = 0;            // Counter that counts the files written in the SD card this session (we don't include prvious files), included in the name of the file, can handle 0d to 99999d (need 17 bits)
String      fileName              = "";           // Name of the current opened file on the SD card
File        dataFile;                               // Only 1 file can be opened at a certain time, <KEEP GLOBAL>


uint32_t    cntAquisitions               = 0;  

Ticker blinker;
float blinkerPace = 0.05;  //seconds

void blink() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void      testSDCard          (void);
void      createNewFile       (void);
//void      logToSDCard         (void);
void      blinkAnError        (uint8_t errno);


void setup() {
  
#ifdef SERIAL_VERBOSE
  Serial.begin(115200);
  Serial.println("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"); // Indicates a wakeup to the console
#endif


  // LED pins
  //--------------
  pinMode (LED_BUILTIN , OUTPUT);

    // Analog pins
  //--------------
  pinMode (BATT_PIN    , INPUT);

    // Initial pin state
  //-------------------
  digitalWrite(LED_BUILTIN, LOW);



  testSDCard(); // Because of blink an error
  // Create a new file
      createNewFile();

  //LED
  blinker.attach(blinkerPace, blink);


}

void loop() 
{
  uint16_t rawBattValue = analogRead(BATT_PIN); //read the BATT_PIN pin value,   //12 bits (0 – 4095)
  float battVoltage     = rawBattValue * (3.30 / 4095.00) * BATT_VOLTAGE_DIVIDER_FACTOR; //convert the value to a true voltage

  cntAquisitions ++;

  #ifdef SERIAL_VERBOSE
    Serial.println("----------------------------------------");
    Serial.println("Estimating battery level");

    Serial.print("Raw ADC (0-4095): ");
    Serial.print(rawBattValue);
    Serial.println(" LSB");

    Serial.printf("Voltage: %f [V]\r\n", battVoltage);

    if (battVoltage < LOW_BATTERY_THRESHOLD) // check if the battery is low (i.e. below the threshold)
    {
      Serial.println("Warning: Low battery!");
    }
    else
    {
      Serial.println("Battery level OK");
    }
    #endif

  // Create a string for assembling the data to log
  //-----------------------------------------------
  String dataString  = "";

  // Save the results (acceleration is measured in ???)
  
    dataString += String(cntAquisitions);
    dataString += FORMAT_SEP;
    dataString += String(rawBattValue);
    dataString += FORMAT_SEP;
    dataString += String(battVoltage);



#ifdef SERIAL_VERBOSE
  Serial.println("Data going to SD card: ");
  Serial.println(dataString);
  Serial.flush();
#endif

  // Log data in the SD card
  //------------------------

  // Check if the file is available
  if (dataFile)
  {
    // We do it like this because of the "\r\n" not desired at the end of a file
    if (cntLinesInFile >= MAX_LINES_PER_FILES - 1) // Check if we have reached the max. number of lines per file
    {
      // Boost the frequency of the CPU to the MAX so that the writing takes less time
      // setCpuFrequencyMhz(MAX_CPU_FREQUENCY);
      // Write to the file w/out the "\r\n"
      dataFile.print(dataString);
      // Close the file
      dataFile.close();
      // Reset the line counter
      cntLinesInFile = 0;
      #ifdef SERIAL_VERBOSE
      Serial.println("Reached the max number of lines per file, starting a new one");
      #endif
      // Create a new file
      createNewFile();
    }
    else // wE ARE STILL UNDER THE LIMIT OF NUMBER OF LINES PER FILE
    {
      dataFile.println(dataString);
      cntLinesInFile++; // Increment the lines-in-current-file counter

      #ifdef SERIAL_VERBOSE
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
    #ifdef SERIAL_VERBOSE
    Serial.print("Error writting to the file: ");
    Serial.println(fileName);
    #endif
    fileName = "";        // Reset the filename
    cntLinesInFile = 0;   // Reset the lines-in-current-file counter
  }

  delay(TIME_TO_SLEEP_MS);

}



//******************************************************************************************
void blinkAnError(uint8_t errno) {  // Use an on-board LED (the red one close to the micro USB connector, left of the enclosure) to signal errors (RTC/SD)

  //errno argument tells how many blinks per period to do. Must be  strictly less than 10

  while(1) { // Infinite loop: stay here until power cycle
    uint8_t i;

    // This part is executed errno times, quick blink
    for (i=0; i<errno; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }


    // This part is executed (10 - errno) times, led off (waiting to reblink)
    for (i=errno; i<10; i++) {
      delay(200);
    }

    // Total time spent is: errno * (100 + 100) + (10 - errno) * 200 = 2000ms
  }
}

//******************************************************************************************
void testSDCard(void)
{
#ifdef SERIAL_VERBOSE
  Serial.print("Initializing SD card...");
#endif

  // Check if the card is present and can be initialized:
  if (!SD.begin(PIN_CS_SD)) {
    #ifdef SERIAL_VERBOSE
    Serial.println("Card failed, or not present");
    #endif
    blinkAnError(2);
  }

#ifdef SERIAL_VERBOSE
  Serial.println("Card initialized");
#endif

}

//******************************************************************************************
void createNewFile(void) {

  cntFile ++; // Increment the counter of files

  // To name the file we need to know the date : ask the GPS or the RTC
  
  fileName = "";                    // Reset the filename
  fileName += "/L-";                  // To tell it to put in the root folder, absolutely necessary

  char buffer[5];
  sprintf(buffer, "%05d", cntFile); // Making sure the file number is always printed with 5 digits
  //  Serial.println(buffer);
  fileName += String(buffer);

  fileName += ".txt"; // Add the file extension


  if (SD.exists(fileName))
  {
    #ifdef SERIAL_VERBOSE
    Serial.print("Looks like a file already exits on the SD card with that name: ");
    Serial.println(fileName);
    #endif
  }

  // Open the file. Note that only one file can be open at a time,
  // so you have to close this one before opening another.
#ifdef SERIAL_VERBOSE
  Serial.print("Creating the following file on the SD card: ");
  Serial.println(fileName);
#endif
  dataFile = SD.open(fileName, FILE_WRITE);

}
