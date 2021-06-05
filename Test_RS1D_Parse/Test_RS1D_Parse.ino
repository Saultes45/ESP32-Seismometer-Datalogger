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

#define SEPARATOR_CHAR  0x2C    // ',' Separate fields in messages
#define EOM_CHAR_SR     0x7D    // '}'
#define SOM_CHAR_SR     0x7B    // '{'
#define EMPTY_CHAR      0x00    // '\0'
#define OutBufferSize (26u)     //

bool goodMessageReceived_flag  = false; // Set to "bad message" first
int32_t seismometerAcc[50];
int32_t thresholdAcc[50];
uint8_t nbr_bumpDetectedLast = 0u; //  max of 50; // This should be the output of a function and should be local 


void parse_SR(void);
void readRS1DBuffer(void);
void parseGeophoneData(void);
uint32_t  hex2dec                     (char * a);


//--------------------------------------------------
void setup() {

  pinMode (RS1D_PWR_PIN_1    , OUTPUT);
  pinMode (RS1D_PWR_PIN_2    , OUTPUT);

  for (uint8_t cnt_fill=0; cnt_fill < 50; cnt_fill++)
  {
    seismometerAcc[cnt_fill] = (int32_t)0;
    thresholdAcc[cnt_fill] = (int32_t)0;
  }

  Serial.begin(115200);

//      for (uint8_t cnt_Acc = 0; cnt_Acc < 50; cnt_Acc++)
//      {
//        Serial.printf("%d",seismometerAcc[cnt_Acc]);
//        Serial.print(",");
//      }
//      Serial.println();


//Serial.println(hex2dec("FFFFFFFE"));
//int32_t testAcc = (int32_t)hex2dec("FFFFFFFE");
//Serial.println(testAcc);

      
  // Start the HW serial for the Geophone/STM
  // ----------------------------------------
  GeophoneSerial.begin(GEOPHONE_BAUD_RATE);
  GeophoneSerial.setTimeout(100);// Set the timeout to 10 milliseconds (for findUntil)

  // DEBUG
  //GeophoneSerial.println("This is a test");

  digitalWrite(RS1D_PWR_PIN_1, HIGH);
  digitalWrite(RS1D_PWR_PIN_2, HIGH);

}

void loop() {

    //Serial.println("-----------------------");
    readRS1DBuffer();
    if (goodMessageReceived_flag)
    {
      goodMessageReceived_flag = false; // Reset the flag
//      Serial.printf("1 good message was received with %d bumps\r\n", nbr_bumpDetectedLast);
      nbr_bumpDetectedLast = (uint8_t)0; //Reset
      for (uint8_t cnt_Acc = 0; cnt_Acc < 50; cnt_Acc++)
      {
        Serial.print(thresholdAcc[cnt_Acc]);
        Serial.print("   ");
        Serial.println(seismometerAcc[cnt_Acc]);
//        delay(10);
//        Serial.print(",");
Serial.flush();
      }
      
      
//      for (uint8_t cnt_fill=0; cnt_fill < 50; cnt_fill++)
//      {
//        seismometerAcc[cnt_fill] = (int32_t)0;
//      }
//      Serial.println("Done");
//      Serial.flush();
    } 
    //delay(10);

      // If we are in a state that requires reading the Geophone then read the GEOPHONE serial buffer until no more character available
//    if (GeophoneSerial.available()) 
//    {
//        char c = GeophoneSerial.read();
//        Serial.write(c);
//    }

}








void readRS1DBuffer(void)
{
  uint16_t  max_nbr_depiledChar = 500;
  uint16_t  cnt_depiledChar = 0;
  uint16_t  cnt_savedMessage = 0;
  char      temp             = 0; // Init must be different from SOM_CHAR_SR

  if (GeophoneSerial.available()) 
  {
//    Serial.println("-----------------------");
    
    if(not(GeophoneSerial.find("[\"")))// test the received buffer for SOM_CHAR_SR
    { 
//      Serial.println("Couldn't find the start of message");
    }
    else
    {
      
//      Serial.println("Found start of message");

      cnt_savedMessage = 0;
      RS1Dmessage[cnt_savedMessage] = '[';
      cnt_savedMessage ++;
      RS1Dmessage[cnt_savedMessage] = '\"';
      cnt_savedMessage ++;
      
//      Serial.println(RS1Dmessage);

      cnt_depiledChar = 0; // reset

      //Depile and save a char from buff until '}' found
//      Serial.println("Depile and save a char from buff until ']' found");
//      Serial.println(cnt_savedMessage);
      //        
      //        while((cnt_depiledChar < max_nbr_depiledChar) & cnt_savedMessage < RX_BUFFER_SIZE) // 4 conditions (GeophoneSerial.available()) &  (RS1Dmessage[cnt_savedMessage-1] != EOM_CHAR_SR) & (cnt_depiledChar < max_nbr_depiledChar) & 
      //        {
      //          while (GeophoneSerial.available())
      //          {
      //////          RS1Dmessage[cnt_savedMessage] = GeophoneSerial.read();
      //            Serial.print(GeophoneSerial.read());
      ////          
      ////            cnt_depiledChar++;
      //          }
      //          cnt_savedMessage++;
      //        }

      unsigned long startedWaiting = millis();
      unsigned long howLongToWait = 100; // in [ms]
      while((RS1Dmessage[cnt_savedMessage-1] != ']') && (millis() - startedWaiting <= howLongToWait) && (cnt_savedMessage < RX_BUFFER_SIZE))
      {
        if (GeophoneSerial.available())
        {
          RS1Dmessage[cnt_savedMessage] = GeophoneSerial.read();
          cnt_savedMessage++;
        }
      }

//      Serial.printf("We waited %d ms\r\n", millis() - startedWaiting);

//      Serial.println("Content of the buffer");
//      Serial.println(RS1Dmessage);

//      Serial.print("cnt_savedMessage: ");
//      Serial.println(cnt_savedMessage);

      if ((RS1Dmessage[cnt_savedMessage-1] == ']')) // if any SOM found then we continue
      {
        //Serial.println("Time to parse the RS1D Message");
        delay(1);
        //parse_SR(); // <DEBUG>
        parseGeophoneData();
        // goodMessageReceived_flag = true; // <DEBUG>
      }
      else
      {
//        Serial.println("Tried to depile from RS1D serial buffer but timeout");
      }
      //    }
      //    else
      //    {
      //      Serial.println("Tried to depile from RS1D serial buffer but no SOM_CHAR_SR found");
      //    }
    }
    memset(RS1Dmessage, 0, RX_BUFFER_SIZE); // clean the message field anyway
//    Serial.print("Reset of RS1Dmessage: ");
//    Serial.println(RS1Dmessage);
  }
  else
  {
    //Serial.println("No data available in the buffer");
  }
}



//******************************************************************************************
void parse_SR(void)
{
  //Place holder
  Serial.println(RS1Dmessage); // Display 1 retrieved message
  Serial.println("Parsing done");
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
        thresholdAcc[cnt_fill] = (int32_t)0;
        seismometerAcc[cnt_fill] = (int32_t)0;
      }

    // Parse the aquited message to get 1st acceleration (1st is different)
    //----------------------------------------------------------------------
//    Serial.println("Parsing the 1st");
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
      thresholdAcc[0] = seismometerAcc[0];
      nbr_bumpDetectedLast++;
    }

    // Parse the rest of the accelerations
    //-------------------------------------
//    Serial.println("Parsing the rest");
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
        thresholdAcc[cnt_accvalues] = seismometerAcc[cnt_accvalues];
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
