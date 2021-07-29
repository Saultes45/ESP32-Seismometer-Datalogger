
#define BATT_PIN                      35      // To detect the Lipo battery remaining charge, GPIO35 on Adafruit ESP32 (35 on dev kit)

const uint8_t RS1D_PWR_PIN_1              = 14;      // To turn the geophone ON and OFF

const uint8_t     LOG_PWR_PIN_1        = 25;    // To turn the geophone ON and OFF
const uint8_t     LOG_PWR_PIN_2        = 26;    // To turn the geophone ON and OFF

const uint8_t GPS_BOOST_ENA_PIN           = 21;      // To turn the BOOST converter of the GPS ON and OFF






//******************************************************************************************
void pinSetUp (void)
{

  // Declare which pins of the ESP32 will be used

  // LED pins
  //--------------
  pinMode (LED_BUILTIN , OUTPUT);

  // Analog pins
  //--------------
  pinMode (BATT_PIN    , INPUT);

  // Power pins
  //--------------
  pinMode (RS1D_PWR_PIN_1    , OUTPUT);

  pinMode (LOG_PWR_PIN_1    , OUTPUT);
  pinMode (LOG_PWR_PIN_2    , OUTPUT);

  pinMode (GPS_BOOST_ENA_PIN    , OUTPUT);

  // Initial pin state
  //-------------------
  digitalWrite(LED_BUILTIN, LOW);

//  turnRS1DOFF();
//  turnLogOFF();
//  turnGPSOFF();

}



void setup() {

pinSetUp();
digitalWrite(GPS_BOOST_ENA_PIN, HIGH);
delay(1000 * 50);
digitalWrite(GPS_BOOST_ENA_PIN, LOW);

}

void loop() {
  delay(10000);

}
