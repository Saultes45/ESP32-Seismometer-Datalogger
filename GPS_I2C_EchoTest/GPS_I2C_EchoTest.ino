// Test code for Adafruit GPS That Support Using I2C
//
// This code shows how to test a passthru between USB and I2C
//
// Pick one up today at the Adafruit electronics shop
// and help support open source hardware & software! -ada

#include <Adafruit_GPS.h>



// Relay Pins (for turning external components ON and OFF)
#define GPS_PWR_PIN_1                      14      // To turn the GPS ON and OFF
#define GPS_PWR_PIN_2                      32      // To turn the GPS ON and OFF
#define GPS_PWR_PIN_3                      15      // To turn the GPS ON and OFF
#define GPS_PWR_PIN_4                      12      // To turn the GPS ON and OFF

#define GPS_PPS_PIN                      27      // To get an interrupt each time there is a PPS from GPS

// Connect to the GPS on the hardware I2C port
Adafruit_GPS GPS(&Wire);

// -------------------------- ISR ----------------
volatile bool ppsDetected             = false;    //false= no rising edge, true= rising edge

void IRAM_ATTR ISR_GPS_PPS() {
    ppsDetected = true; //set the flag
}


void setup() {
  // wait for hardware serial to appear
  while (!Serial);

 // Relay pins (GPS+RS1D)   
pinMode (GPS_PWR_PIN_1    , OUTPUT);
pinMode (GPS_PWR_PIN_2    , OUTPUT);
pinMode (GPS_PWR_PIN_3    , OUTPUT);
pinMode (GPS_PWR_PIN_4    , OUTPUT);
digitalWrite(GPS_PWR_PIN_1, HIGH);
digitalWrite(GPS_PWR_PIN_2, HIGH);
digitalWrite(GPS_PWR_PIN_3, HIGH);
digitalWrite(GPS_PWR_PIN_4, HIGH);

pinMode (GPS_PPS_PIN    , INPUT);
//pinMode(GPS_PPS_PIN, INPUT_PULLDOWN);
attachInterrupt(GPS_PPS_PIN, ISR_GPS_PPS, RISING);
//detachInterrupt(GPS_PPS_PIN);
// LOW, HIGH, CHANGE, FALLING, RISING

  // make this baud rate fast enough to we aren't waiting on it
  Serial.begin(115200);

  Serial.println("Adafruit GPS library basic I2C test!");
  GPS.begin(0x10);  // The I2C address to use is 0x10
}


void loop() 
{
//  if (Serial.available()) {
//    char c = Serial.read();
//    GPS.write(c);
//  }

if (ppsDetected) {
      Serial.println("PPS pulse received");
      ppsDetected = false;
  }

//  if (GPS.available()) {
//    char c = GPS.read();
//    Serial.write(c);
//  }
}
