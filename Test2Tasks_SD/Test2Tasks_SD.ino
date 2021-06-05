

// Array of strings

#define STRINGS_ARRAY_SIZE 50

// Declare a REAL array of Strings, and NOT an array of pointers to Strings
// Careful because of easy memory fragmentation
String dataToWrite[STRINGS_ARRAY_SIZE];

uint32_t globalCounter = 0;

String standardString = "(\"[\"2121\",\"21E5\",\"221F\",\"2186\",\"2111\",\"2171\",\"21E6\",\"21BB\",\"20EF\",\"2010\",\"2010\",\"20CA\",\"21B2\",\"21C3\",\"20E1\",\"204C\",\"2063\",\"212E\",\"21BA\",\"21B4\",\"2108\",\"2032\",\"2055\",\"2182\",\"2262\",\"223B\",\"20FC\",\"1FE5\",\"1F9C\",\"2077\",\"21CD\",\"222D\",\"21A1\",\"20BB\",\"2068\",\"2133\",\"21FA\",\"21F9\",\"20FB\",\"2006\",\"2038\",\"2150\",\"2233\",\"21D6\",\"207C\",\"1FEC\",\"2065\",\"215B\",\"219B\",\"20C0\"]}{\"MSEC\": 746250,\"LS\": 0}{\"MA\": \"RS1D-6-4.5\",\"DF\": \"1.0\",\"CN\": \"SH3\",\"TS\": 0,\"TSM\": 0,\"TQ\": 45,\"SI\":  5000,\"DS\": ";

// 2 Tasks FreeRTOS

void setup() {

  Serial.begin(115200);

  globalCounter = 0; // Init

}

void loop() {

uint32_t cnt_Strings = 0;

// Fill the strings in the array

for (cnt_Strings=0; cnt_Strings<STRINGS_ARRAY_SIZE - 1; cnt_Strings++)
{
    dataToWrite[cnt_Strings] = "";
     int generated=0;
      for (generated=0; generated<random(200, 600) - 1; generated++)
      {
         byte randomValue = random(0, 26);
         char letter = randomValue + 'a';

         dataToWrite[cnt_Strings] = dataToWrite[cnt_Strings] + String(char(letter)) ;

      }
     dataToWrite[cnt_Strings] =  String(globalCounter) + String("  ") + dataToWrite[cnt_Strings];//+ standardString;
     globalCounter ++;
}

  delay(500);

  // Display the strings in the array

for (cnt_Strings=0; cnt_Strings<STRINGS_ARRAY_SIZE - 1; cnt_Strings++)
{
     Serial.println(dataToWrite[cnt_Strings]);

}

  delay(500);

}
