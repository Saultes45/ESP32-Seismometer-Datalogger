
//const uint8_t     RS1D_PWR_PIN_1        = 25u;    // To turn the geophone ON and OFF
//const uint8_t     RS1D_PWR_PIN_2        = 26u;    // To turn the geophone ON and OFF

//Possible: 25 26 *34* *39* *36* *04* 21 12 27 15 32 14
// RTC GPS RS1D

#define BATT_PIN                      35      // To detect the Lipo battery remaining charge, GPIO35 on Adafruit ESP32 (35 on dev kit)


void setup() {
    
    
    // Declare which pins of the ESP32 will be used
  pinMode (LED_BUILTIN , OUTPUT);
  pinMode (BATT_PIN    , INPUT);

//  pinMode (RS1D_PWR_PIN_1    , OUTPUT);
//  pinMode (RS1D_PWR_PIN_2    , OUTPUT);
pinMode   (25 , OUTPUT);
digitalWrite(25, HIGH);

pinMode   (26 , OUTPUT);
digitalWrite(26, HIGH);

pinMode   (34 , OUTPUT);
digitalWrite(34, HIGH);

pinMode   (39 , OUTPUT);
digitalWrite(39, HIGH);

pinMode   (36 , OUTPUT);
digitalWrite(36, HIGH);

pinMode   (04 , OUTPUT);
digitalWrite(04, HIGH);

pinMode   (21 , OUTPUT);
digitalWrite(21, HIGH);

pinMode   (12 , OUTPUT);
digitalWrite(12, HIGH);

pinMode   (27 , OUTPUT);
digitalWrite(27, HIGH);

pinMode   (15 , OUTPUT);
digitalWrite(15, HIGH);

pinMode   (32 , OUTPUT);
digitalWrite(32, HIGH);

pinMode   (14 , OUTPUT);
digitalWrite(14, HIGH);


}

void loop() {
  // put your main code here, to run repeatedly:

}
