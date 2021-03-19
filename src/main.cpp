#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <FS.h>  // Please read the instructions on http://arduino.esp8266.com/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system
#include <ezTime.h>
#include <HTTPUpdateServer.h>

// Need to change this to LittleFS
#include "SPIFFS.h"

// Declarations
// Set a timezone using the following list
// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
#define MYTIMEZONE "Australia/Brisbane"

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define NUM_LEDS 30                           // Total of 86 LED's     
#define DATA_PIN 13                           // Change this if you are using another type of ESP board than a WeMos D1 Mini
#define MILLI_AMPS 2400 
#define COUNTDOWN_OUTPUT 13

#define WIFIMODE 2                            // 0 = Only Soft Access Point, 1 = Only connect to local WiFi network with UN/PW, 2 = Both

#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2)
  const char* APssid = "CLOCK_AP";        
  const char* APpassword = "1234567890";  
#endif
  
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2)
  #include "Credentials.h"                    // Create this file in the same directory as the .ino file and add your credentials (#define SID YOURSSID and on the second line #define PW YOURPASSWORD)
  const char *ssid = SID;
  const char *password = PW;
#endif

//Rtc<TwoWire> Rtc(Wire);
RTC_DS3231 Rtc;

//AsyncWebServer server(80);
WebServer server(80);
HTTPUpdateServer httpUpdateServer;
CRGB LEDs[NUM_LEDS];

// Settings
Timezone myTZ;
unsigned long prevTime = 0;
byte r_val = 255;
byte g_val = 0;
byte b_val = 0;
bool dotsOn = true;
byte brightness = 255;
float temperatureCorrection = -3.0;
byte temperatureSymbol = 12;                  // 12=Celcius, 13=Fahrenheit check 'numbers'
byte clockMode = 0;                           // Clock modes: 0=Clock, 1=Countdown, 2=Temperature, 3=Scoreboard
unsigned long countdownMilliSeconds;
unsigned long endCountDownMillis;
byte hourFormat = 24;                         // Change this to 12 if you want default 12 hours format instead of 24               
CRGB countdownColor = CRGB::Green;
byte scoreboardLeft = 0;
byte scoreboardRight = 0;
CRGB scoreboardColorLeft = CRGB::Green;
CRGB scoreboardColorRight = CRGB::Red;
CRGB alternateColor = CRGB::Black; 
long numbers[] = {
  //6543210 <- bit - reverse order
  0b1111110,  // [0] 0
  0b0011000,  // [1] 1
  0b1101101,  // [2] 2
  0b0111101,  // [3] 3
  0b0011011,  // [4] 4
  0b0110111,  // [5] 5
  0b1110111,  // [6] 6
  0b0011100,  // [7] 7
  0b1111111,  // [8] 8
  0b0011111,  // [9] 9
  0b0000000,  // [10] off
  0b0000000,  // [10] off
  0b0001111,  // [11] degrees symbol
  0b1100110,  // [12] C(elsius)
  0b1000111,  // [13] F(ahrenheit)
};

//-- Functions
void allBlank() {
  for (int i=0; i<NUM_LEDS; i++) {
    LEDs[i] = CRGB::Black;
  }
  FastLED.show();
}

void displayNumber(byte number, byte segment, CRGB color) {
  /*
   *      
      _        _           _       2   
    _    _   _    _      _   _   1   3
      23       16    14    7       0   
    _   _    _    _  15  _   _   6   4
      _        _           _       5 

    6543210 <- bit - reverse order
  0b1111110,  // [0] 0
  0b0011000,  // [1] 1
  0b1101101,  // [2] 2
  0b0111101,  // [3] 3
  0b0011011,  // [4] 4
  0b0110111,  // [5] 5
  0b1110111,  // [6] 6
  0b0011100,  // [7] 7
  0b1111111,  // [8] 8
  0b0011111,  // [9] 9
  0b0000000,  // [10] off
  0b0001111,  // [11] degrees symbol
  0b1100110,  // [12] C(elsius)
  0b1000111,  // [13] F(ahrenheit)

   */
 
  // segment from left to right: 3, 2, 1, 0
  byte startindex = 0;
  switch (segment) {
    case 0:
      startindex = 0;
      break;
    case 1:
      startindex = 7;
      break;
    case 2:
      startindex = 16;
      break;
    case 3:
      startindex = 23;
      break;    
  }

  for (byte i=0; i<7; i++){
    yield();
    LEDs[i + startindex] = ((numbers[number] & 1 << i) == 1 << i) ? color : alternateColor;
  } 
}

void displayDots(CRGB color) {
  if (dotsOn) {
    LEDs[14] = color;
    LEDs[15] = color;
  } else {
    LEDs[14] = CRGB::Black;
    LEDs[15] = CRGB::Black;
  }

  dotsOn = !dotsOn;  
}


void hideDots() {
  LEDs[42] = CRGB::Black;
  LEDs[43] = CRGB::Black;
}

void updateClock() {  
  DateTime now = Rtc.now();
  //Serial.println(now);    

  //Serial.println(now.hour());
  int hrs = now.hour();

  //Serial.println(now.minute());
  int mins = now.minute();

  //Serial.println(now.second());
  int secs = now.second();


  if (hourFormat == 12 && hrs > 12)
    hrs = hrs- 12;
  
  byte h1 = hrs / 10;
  byte h2 = hrs % 10;
  byte m1 = mins / 10;
  byte m2 = mins % 10;  
  byte s1 = secs / 10;
  byte s2 = secs % 10;
  
  CRGB color = CRGB(r_val, g_val, b_val);

  if (h1 > 0)
    displayNumber(h1,3,color);
  else 
    displayNumber(10,3,color);  // Blank
    
  displayNumber(h2,2,color);
  displayNumber(m1,1,color);
  displayNumber(m2,0,color); 

  displayDots(color);  
}

void updateCountdown() {

  if (countdownMilliSeconds == 0 && endCountDownMillis == 0) 
    return;
    
  unsigned long restMillis = endCountDownMillis - millis();
  unsigned long hours   = ((restMillis / 1000) / 60) / 60;
  unsigned long minutes = (restMillis / 1000) / 60;
  unsigned long seconds = restMillis / 1000;
  int remSeconds = seconds - (minutes * 60);
  int remMinutes = minutes - (hours * 60); 
  
  Serial.print(restMillis);
  Serial.print(" ");
  Serial.print(hours);
  Serial.print(" ");
  Serial.print(minutes);
  Serial.print(" ");
  Serial.print(seconds);
  Serial.print(" | ");
  Serial.print(remMinutes);
  Serial.print(" ");
  Serial.println(remSeconds);

  byte h1 = hours / 10;
  byte h2 = hours % 10;
  byte m1 = remMinutes / 10;
  byte m2 = remMinutes % 10;  
  byte s1 = remSeconds / 10;
  byte s2 = remSeconds % 10;

  CRGB color = countdownColor;
  if (restMillis <= 60000) {
    color = CRGB::Red;
  }

  if (hours > 0) {
    // hh:mm
    displayNumber(h1,3,color); 
    displayNumber(h2,2,color);
    displayNumber(m1,1,color);
    displayNumber(m2,0,color);  
  } else {
    // mm:ss   
    displayNumber(m1,3,color);
    displayNumber(m2,2,color);
    displayNumber(s1,1,color);
    displayNumber(s2,0,color);  
  }

  displayDots(color);  

  if (hours <= 0 && remMinutes <= 0 && remSeconds <= 0) {
    Serial.println("Countdown timer ended.");
    //endCountdown();
    countdownMilliSeconds = 0;
    endCountDownMillis = 0;
    digitalWrite(COUNTDOWN_OUTPUT, HIGH);
    return;
  }  
}

void endCountdown() {
  allBlank();
  for (int i=0; i<NUM_LEDS; i++) {
    if (i>0)
      LEDs[i-1] = CRGB::Black;
    
    LEDs[i] = CRGB::Red;
    FastLED.show();
    delay(25);
  }  
}

void updateTemperature() {
  float temp = Rtc.getTemperature();
  //float ftemp = temp.AsFloatDegC();
  float ctemp = temp + temperatureCorrection;
  Serial.print("Sensor temp: ");
  Serial.print(temp);
  Serial.print(" Corrected: ");
  Serial.println(ctemp);
 
  // Offset for the correct byte
  int offSetSymbol;
  if (temperatureSymbol == 13) {
    ctemp = (ctemp * 1.8000) + 32;
    offSetSymbol = 14;
  }
  else { offSetSymbol = 13; }

  byte t1 = int(ctemp) / 10;
  byte t2 = int(ctemp) % 10;
  CRGB color = CRGB(r_val, g_val, b_val);
  displayNumber(t1,3,color);
  displayNumber(t2,2,color);
  displayNumber(12,1,color);
  displayNumber(offSetSymbol,0,color);
  hideDots();
}

void setup() {
  pinMode(COUNTDOWN_OUTPUT, OUTPUT);
  Serial.begin(115200); 
  delay(200);

  // RTC DS3231 Setup
  if (! Rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (Rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    Rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  //WiFi.setSleepMode(WIFI_NONE_SLEEP);  

  delay(200);
  //Serial.setDebugOutput(true);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS);  
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
  fill_solid(LEDs, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // WiFi - AP Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 0 || WIFIMODE == 2) 
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(APssid, APpassword);    // IP is usually 192.168.4.1
  Serial.println();
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
#endif

  // WiFi - Local network Mode or both
#if defined(WIFIMODE) && (WIFIMODE == 1 || WIFIMODE == 2) 
  byte count = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status()   != WL_CONNECTED) {
    // Stop if cannot connect
    if (count >= 60) {
      Serial.println("Could not connect to local WiFi.");      
      return;
    }
       
    delay(500);
    Serial.print(".");
    LEDs[count] = CRGB::Green;
    FastLED.show();
    count++;
  }
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  IPAddress ip = WiFi.localIP();
  Serial.println(ip[3]);
#endif  

  // Setup EZ Time
  setDebug(INFO);
  waitForSync();

  Serial.println();
  Serial.println("UTC:             " + UTC.dateTime());

  myTZ.setLocation(F(MYTIMEZONE));
  Serial.print(F("Time in your set timezone:         "));
  Serial.println(myTZ.dateTime());

  httpUpdateServer.setup(&server);

  // Handlers
  server.on("/color", HTTP_POST, []() {    
    r_val = server.arg("r").toInt();
    r_val = server.arg("g").toInt();
    b_val = server.arg("b").toInt();
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/setdate", HTTP_POST, []() { 
    // Sample input: date = "Dec 06 2009", time = "12:34:56"
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    // This use EZTime to get the time from the NET
    String timeString = "";
    timeString = myTZ.dateTime("g:i:s");

    //If the length is only 7, pad it with
    // a space at the beginning
    if (timeString.length() == 7) {
      timeString = " " + timeString;
    }

    Serial.println(timeString);

    String hrsString = myTZ.dateTime("g");
    int setHrs = hrsString.toInt();

    String minString = myTZ.dateTime("i");
    int setMin = minString.toInt();

    String secString = myTZ.dateTime("s");
    int setSec = secString.toInt();
 
    Rtc.adjust(DateTime(2021, 1, 10, setHrs, setMin, setSec));
    clockMode = 0;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/brightness", HTTP_POST, []() {    
    brightness = server.arg("brightness").toInt();    
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });
  
  server.on("/countdown", HTTP_POST, []() {    
    countdownMilliSeconds = server.arg("ms").toInt();     
    byte cd_r_val = server.arg("r").toInt();
    byte cd_g_val = server.arg("g").toInt();
    byte cd_b_val = server.arg("b").toInt();
    digitalWrite(COUNTDOWN_OUTPUT, LOW);
    countdownColor = CRGB(cd_r_val, cd_g_val, cd_b_val); 
    endCountDownMillis = millis() + countdownMilliSeconds;
    allBlank(); 
    clockMode = 1;     
    //server.send(200, "text/json", "{\"result\":\"ok\"}");
  });

  server.on("/temperature", HTTP_POST, []() {   
    temperatureCorrection = server.arg("correction").toInt();
    temperatureSymbol = server.arg("symbol").toInt();
    Serial.println(temperatureSymbol);
    clockMode = 2;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  });  

  server.on("/hourformat", HTTP_POST, []() {   
    hourFormat = server.arg("hourformat").toInt();
    clockMode = 0;     
    server.send(200, "text/json", "{\"result\":\"ok\"}");
  }); 

  server.on("/clock", HTTP_POST, []() {       
    clockMode = 0;     
    //server.send(200, "text/json", "{\"result\":\"ok\"}");
  });  
 
  // Before uploading the files with the "ESP32 Sketch Data Upload" tool, zip the files with the command "gzip -r ./data/" (on Windows I do this with a Git Bash)
  // *.gz files are automatically unpacked and served from your ESP (so you don't need to create a handler for each file).
  server.serveStatic("/", SPIFFS, "/", "max-age=86400");
  server.begin();     

  SPIFFS.begin();
  Serial.println("");
  Serial.println("SPIFFS contents:");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while ( file ) {
    String fileName = file.name();
    size_t fileSize = file.size();
    Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
    file = root.openNextFile();
  }
  Serial.println(); 
  
  digitalWrite(COUNTDOWN_OUTPUT, LOW);
}

//-- The Main Loop
void loop(){

  server.handleClient(); 
  
  unsigned long currentMillis = millis();  
  if (currentMillis - prevTime >= 1000) {
    prevTime = currentMillis;

    if (clockMode == 0) {
      updateClock();
    } else if (clockMode == 1) {
      updateCountdown();
    } else if (clockMode == 2) {
      updateTemperature();      
    }

    FastLED.setBrightness(brightness);
    FastLED.show();
  }   
}



 
