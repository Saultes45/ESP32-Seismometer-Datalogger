/* ========================================
*
* Copyright University of Auckland Ltd, 2021
* All Rights Reserved
* UNPUBLISHED, LICENSED SOFTWARE.
*
* Metadata
* Written by    : Nathanaël Esnault
* Verified by   : No one yet
* Creation date : 2021-05-26
* Version       : 0.1 (finished on 2021-..-..)
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
* Ressources (Boards + Libraries Manager)
*
*
* SD card:        https://learn.adafruit.com/adafruit-adalogger-featherwing/using-the-sd-card
* RTC:            https://learn.adafruit.com/adafruit-adalogger-featherwing/adafruit2-rtc-with-arduino
* Geophone
*
* Adafruit ESP32 Feather Board (Adafruit HUZZAH32) https://www.adafruit.com/product/3405
*
* TODO
*
* ========================================
*/

// -------------------------- Includes --------------------------
// Our own library
#include "config.h"   // all the parameters

//ESP32
#include <esp_system.h>
#include <rom/rtc.h> // reset reason

//String Formatting
#include <string.h>
#include <stdio.h>

// SD card
#include <SPI.h>
#include <SD.h>

// RTC
#include <RTClib.h> // use the one from Adafruit, not the forks with the same name

// Non volatile memory (NVM) library
#include <Preferences.h> 
Preferences prefs;

// -------------------------- Defines --------------------------
// All moved in "config.h" now

// -------------------------- Structs --------------------------
// All moved in "config.h" now

// -------------------------- Classes -------------------------
// NONE

// -------------------------- Global variables ----------------
// Most are moved in "config.h" now

//RTC
RTC_PCF8523         rtc;
DateTime            time_loop;             // MUST be global!!!!! or it won't update
DateTime            timestampForFileName;  // MUST be global!!!!! or it won't update

// SD
File                dataFile;              // Only 1 file can be opened at a certain time, <KEEP GLOBAL>

hw_timer_t * timerTask = NULL;

// -------------------------- ISR ----------------
// Watchdog
void IRAM_ATTR resetModule() {
  ets_printf("Problem! Watchdog trigger: Rebooting...\r\n");
  esp_restart();
}



// -------------------------- Functions declaration --------------------------
void changeCPUFrequency             (void);                 // Change the CPU frequency and report about it over serial
void print_reset_reason             (RESET_REASON reason);
void verbose_print_reset_reason     (RESET_REASON reason);
void displayWakeUpReason            (void);                 // Display why the module went to sleep
void testSDCard                     (void);                 // Test that the SD card can be accessed
void testRTC                        (void);                 // Test that the RTC can be accessed
void createNewFile                  (void);                 // Create a name a new DATA file on the SD card, file variable is global
//void closeFile                      (void);                 // Close the file currently being used, on the SD card, file variable is global // <Not used>
void blinkAnError                   (uint8_t errno);        // Use an on-board LED (the red one close to the micro USB connector, left of the enclosure) to signal errors (RTC/SD)
//void createNewSeparatorFile         (void);                 // Create a name a new SEPARATOR file on the SD card and close it immediatly (just a beautifier)


// -------------------------- Set up --------------------------

void setup() { //Runs after each deep sleep
  Serial.begin(CONSOLE_BAUD_RATE); //Begin serial communication (USB)
  pinMode(LED_BUILTIN, OUTPUT);   //Set up the on-board LED (RED, close to the uUSB port)

  // Indicate the start of the setup with the red on-board LED
  //------------------------------------------------------------
  digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED ON
  delay(10);                         // Wait a bit

  displayWakeUpReason();
  changeCPUFrequency();

 // External hardaware initialisation and test
 //-------------------------------------------
  // The order of the last 3 tests can be changed
//  testIMU();
//  testSDCard();
//  testRTC();

  // Create the first file
  //----------------------
//  createNewFile();

  // Indicates the end of the setup with the red on-board LED
  //---------------------------------------------------------
  digitalWrite(LED_BUILTIN, LOW);  // Turn the LED OFF
  delay(10);                       // Wait a bit

  Serial.println("This might be the last transmission to the console if you turned verbose OFF");
  
  // Starting the night watch (watchdog)
  //-------------------------------------
  // Remember to hold the door
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
  // Enable all the ISR later
  //timerAlarmEnable(timer);                          //enable interrupt

  // Enable all ISRs
  //----------------
  Serial.println("And here, we, go, ...");
  // Do not go gentle into that good night

  timerAlarmEnable(timer);     // Watchdog

  // Deep sleep
  // ---------- 
  esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM,ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM,ESP_PD_OPTION_OFF);
  //Serial.println("Error = " + String(err));
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); //set up deep sleep wakeup timer
  Serial.println("Going to sleep");
  Serial.flush(); // clear serial
  esp_deep_sleep_start(); //start deep sleep
  
} // END OF SETUP

// -------------------------- Loop --------------------------
void loop() {

}


/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/


void setup() { //Runs after each deep sleep
  
  Serial.begin(115200); //begin serial for logging

  Serial.println(getCpuFrequencyMhz());
  setCpuFrequencyMhz(80);//sets CPU to 80Mhz
  Serial.println(getCpuFrequencyMhz());
  
  Serial.print("Gateway ID is: ");
  Serial.println(WiFi.macAddress());
  gatewayID = WiFi.macAddress();
  Serial.println("");
  
  Serial.println("Setting up whitelist...");
  //beacon whitelist list setup
  prefs.begin("whitelist"); //use "whitelist" namespace
  String beaconString = prefs.getString("whitelist"); //Get the String stored in the whitelist namespace
  Serial.println("Extracted from NVM:");
  Serial.println(beaconString);
  char newWhitelist[ARBITRARY_NUMBER];
  beaconString.toCharArray(newWhitelist, ARBITRARY_NUMBER);

  //Loop through the string, seperating on , to create mac address list
  int i = 0; 
    beacons[i] = strtok(newWhitelist, ",");
    while( beacons[i] != NULL ) {
      Serial.println(beacons[i]);
      i++;
      beacons[i] = strtok(NULL, ",");
   }
   NUMBER_OF_BEACONS = i; //Set number of beacons
  
  prefs.end(); //Close namespace
  
  
  //Create checksum and setup beacon whitelist set.
  uint64_t checksum = 0;
  for(int i = 0; i < NUMBER_OF_BEACONS; i++){
    beaconsSet.insert(beacons[i]); //Place each item from the beacon whitelist into the beacon whitelist set (For faster access)
    uint32_t const crc32_res = crc32.calc((uint8_t const *)beacons[i], strlen(beacons[i])); //Create checksum of the mac address
    checksum += (uint64_t(crc32_res)); //Add that checksum to the overall checksum
  }
  //Print checksum (uint64_t works by having a 32 bit uint, then another which counts the number of overflows).
  Serial.print("Checksum: ");
  uint32_t checksumNum = (checksum >> 0) & ~(~0 << (31-0+1));
  uint32_t checksumOverflow = (checksum >> 32) & ~(~0 << (63-32+1));
  Serial.println(String(checksumNum) + " + 4,294,967,296 * " + String(checksumOverflow));
  Serial.println("Whitelist setup!");

  //BLE setup
  Serial.println("Initialising BLE...");
  BLEDevice::init(""); //Start the bluetooth with no name
  //esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN,ESP_PWR_LVL_N12 ); //Sets the scan power lower
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); //Setup callbacks
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(100);  // less or equal setInterval value, for constant scan doesn't matter what they are as long as they are the same
  Serial.println("BLE intitialised!");

  
  resultCount = 0; //reset number of results
  Serial.println("Start BLE scan");
  //scan
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false); //scan based on parameters in setup
  pBLEScan->stop(); //clean up
  btStop(); //turn off bt
  Serial.println("BLE scan finished");

  
  Serial.println("Creating JSON string");
  //create JSON
  //Adding beacon data found in scan
  String payloadString = "{\"data\":[";
  for (uint8_t i = 0; i < resultCount; i++) {
    payloadString += "{\"mac\":\"";
    payloadString += String(buffer[i].address);
    payloadString += "\",\"rssi\":\"";
    payloadString += String(buffer[i].rssi);
    payloadString += "\",\"timestamp\":\"";
    payloadString += String(buffer[i].timestamp);
    payloadString += "\"}";
    if (i < resultCount - 1) {
      payloadString += ',';
    }
  }
  //Add "empty" values if no beacons are detected
  if(resultCount < 1){ 
    payloadString += "{\"mac\":\"";
    payloadString += "11";
    payloadString += "\",\"rssi\":\"";
    payloadString += "11";
    payloadString += "\",\"timestamp\":\"";
    payloadString += "11";
    payloadString += "\"}";
  }
  //adding ID and checksum
  payloadString += "],\"gateway\":\"";
  payloadString += gatewayID;
  payloadString += "\",\"Cksum\":\"";
  payloadString += String(checksumNum) + " " + String(checksumOverflow);
  payloadString += "\"}";

  Serial.println("JSON string created");
  Serial.println(payloadString);

  //WiFi setup (Done after scan and JSON creation to save power)
  //delay(2000);   // delay needed before calling the WiFi.begin
  WiFi.begin(ssid, password); //Begin attempting connection to wifi
  byte connectionAttempt = 0;
  while (WiFi.status() != WL_CONNECTED) { // check for the connection
    delay(1000);
    Serial.print("Connecting to WiFi.. Attempt ");
    Serial.println(connectionAttempt);
    connectionAttempt += 1;
    if(connectionAttempt >= 5){
      Serial.println("Connection limit exceeded. Restarting.");
      ESP.restart();
    }
  }
  Serial.println("Connection to WiFi successful!");

  //Send JSON string
  Serial.println("Sending HTTP message");
  sendHTTP(payloadString);
  Serial.println("Sent HTTP message");

  //Stop Wifi
  WiFi.mode(WIFI_OFF);
  
  
  Serial.print("Reult count: ");
  Serial.println(resultCount);
  Serial.println("Scan done!");
  pBLEScan->clearResults();   // delete results from BLEScan buffer to release memory


  //deep sleep 
  esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM,ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM,ESP_PD_OPTION_OFF);
  //Serial.println("Error = " + String(err));
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); //set up deep sleep wakeup timer
  Serial.println("Going to sleep");
  delay(1000);
  Serial.flush(); //clear serial
  esp_deep_sleep_start(); //start deep sleep
}


void sendHTTP(String message) {
  if (WiFi.status() == WL_CONNECTED) { // check WiFi connection status

    HTTPClient http; //client object

    http.begin(endpoint);  //Specify destination for HTTP request
    http.addHeader("Content-Type", "text/plain"); //Specify content-type header
    http.addHeader("Authorization", authToken); //JWT token header

    int httpResponseCode = http.POST(message);   //Send the actual POST request
    
    if (httpResponseCode > 0) {
      String response = http.getString(); //Get the response to the request
      Serial.print("Success sending POST: ");
      Serial.println(httpResponseCode);   //Print return code
      Serial.println(response);           //Print request answer
  
      if (String(response) != String("OK") && String(httpResponseCode) == String("200")) //Check if we where sent a new whitelist (response code will be 200, request answer will be the new whitelist)
      {
        Serial.println("Performing Whitelist Update...");
        updateWhitelist(response);
        Serial.println("Whitelist Update Complete!");
      }
    }
    else { //codes under 0 are used by the ESP32 board to say that the POST request sending has failed
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }
    http.end();  //Free resources
  }
  else {
    Serial.println("Error in WiFi connection");
  }
}

void updateWhitelist(String newWhitelistStr){
  //Store new whitelist of form ["c2:47:d6:6e:7a:63","fd:51:ea:c7:6e:da","d8:46:9a:4b:46:29","cb:10:0e:56:a8:c7","ec:c7:3c:e6:f4:1f"]
  newWhitelistStr.remove(0,1); //remove leading "["
  newWhitelistStr.replace("\"",""); //remove "s
  newWhitelistStr.remove(newWhitelistStr.length()-1,newWhitelistStr.length()); //remove trailing "}"

  prefs.begin("whitelist"); //use "whitelist" namespace
  prefs.putString("whitelist", newWhitelistStr); //Place the whitelist string into the "whitelist" NVM
  prefs.end(); //Close the namespace

}

void loop() {
  //not called/used
}
