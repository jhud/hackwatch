// Smartwatch based on the ESP32 + SSD1306 OLED
// (c)2017 James Hudson, released under the MIT license

// NTP based on https://www.arduino.cc/en/Tutorial/UdpNTPClient

// 55mA draw just showing time, changing every second
// Needs a small off switch just to save power
// It should sit flat, even if it means moving the battery to the side in a separate pouch.

// Note on ESP32 sleep modes: the deep and light sleeps both work fine with the RTC,
// but on the "TTGO ESP32 OLED" board, the OLED screen blanks itself on sleep because of the reset pin
// configuration. This makes it not so useful for a watch, discounting any hardware mods to
// the board. So GPIO0 is used as an on/off switch.

// Note on Apple ANCS - I think this needs to have a secure pairing, otherwise the service
// is not advertised over BLE. This is currently under development for the ESP32 Arduino libs
// but is not yet released.
 
#include <TimeLib.h> 
#include <WiFi.h>
#include <WiFiMulti.h>

#include <WiFiUdp.h>

#include "util.h"
#include "states.h"
#include "layout.h"
#include "fonts.h"

#include <driver/adc.h>

// Anything we don't want to commit publicly to git (passwords, keys, etc.)
// You ned to create this file yourself, like:
// const char* ssid[3] = {"yourwifi", "secondwifi", "3rd..."};  //  your network SSID (name)
// const char* pass[3] = {"yourpwd", "secondpwd", "3rdpwd..."};       // your network password
#include "secrets.h"

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

#define UV_SENSOR_ENABLE_GPIO 22

// NTP Servers:
IPAddress timeServer(216,239,35,8); // time.google.com

const int timeZone = 2; // Berlin

WiFiUDP Udp;

static const int MENU_TIME_WIDTH = 36;
static const int STATUS_BAR_HEIGHT = 10;

unsigned long startTime = 0; 
time_t timer = 0;
int lastDay = -1;
int lastMinute = -1;

typedef struct CalendarEvent {
  char name[13];
  char day;
  short int minute;
  short int countdownMinutes;
};

// Sunday = 1
// Saturday = 7
CalendarEvent calendarEvents[] = {
  {"Train OK", 2, 10*60, 60},
  {"Train OK", 3, 10*60, 60},
  {"Train OK", 4, 10*60, 60},
  {"Train OK", 5, 10*60, 60},
  {"H'str 277", 5, 20*60 + 18, 45},
  {"Train OK", 6, 10*60, 60},
  {"FL aerial", 7, 14*60, 99}, 
  {"FL stretch", 1, 12*60, 60}
};

int buttonPins[] = {32, 25, 26, 27, 35};
uint32_t inputMap;

State state = StateTime;

WiFiMulti wifiMulti;
#define INTERUPT_PIN 0
void onboardButtonPressed() {

        detachInterrupt(INTERUPT_PIN);
        pinMode(INTERUPT_PIN, INPUT);
        Layout::enableDisplay(false);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0,0);
    esp_deep_sleep_start();
}

void inputPressed0() { inputMap |=  1; }

void inputPressed1() { inputMap |=  2; }

void inputPressed2() { inputMap |=  4; }

void inputPressed3() { inputMap |=  8; }

void inputPressed4() { inputMap |=  16; }

bool isTimeSet() {
    time_t now = time(nullptr);
 return (now > 1000);
}

void setup() 
{
  Serial.begin(115200);

  pinMode(UV_SENSOR_ENABLE_GPIO,OUTPUT);

  pinMode(34, INPUT);

  pinMode(16,OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high

  pinMode(0, INPUT); // Let us use flash button on dev board as a function button

for (int i=0; i<NUM_OF(buttonPins); i++) {
pinMode(buttonPins[i], INPUT_PULLUP);  
}

  Layout::init(false); // left handed

  delay(50);
  if (isTimeSet()) {
    Serial.println("Woo! we woke up");
    timeval time_now; 
    gettimeofday(&time_now, NULL); 
    setTime(time_now.tv_sec); // @todo use a single system
  }
  else {
  
  WiFi.mode(WIFI_STA);

  for (int i=0; i<NUM_OF(ssid); i++) {
      wifiMulti.addAP(ssid[i], pass[i]);
  }


  int status = WL_IDLE_STATUS; 
  int i=0;
  while (status != WL_CONNECTED) {
    Layout::clear();
    Layout::drawStringInto(0, 0, DISPLAY_WIDTH, 18, "Connecting...", AlignLeft, GREEN);
    Layout::drawStringInto(0, 18, DISPLAY_WIDTH, 18, String(i), AlignLeft, WHITE);
    Layout::drawStringInto(0, 36, DISPLAY_WIDTH, 18, String(WiFi.status()), AlignLeft, GREEN);
    Layout::swapBuffers();
     status = wifiMulti.run();

                    i++;

      if (i==5) {
    Layout::clear();
    Layout::drawStringInto(0, 0, DISPLAY_WIDTH, 14, "Timed out. Sleeping.", AlignLeft, RED);  
    Layout::swapBuffers();
         WiFi.disconnect();
         WiFi.mode(WIFI_OFF);
         delay(500000);
           WiFi.mode(WIFI_STA);
         i=0;  
      }
          delay(10000);
  }

  

  Udp.begin(8888);

    Layout::clear();
    Layout::drawStringInto(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, "Getting time...");
    Layout::swapBuffers();

auto ntpTime = getNtpTime();
timeval time_now; 
time_now.tv_sec = ntpTime;
time_now.tv_usec = 0;
  


    settimeofday(&time_now, NULL); // as used by the onboard RTC which can survive deep sleeps
    setTime(ntpTime); // @todo use a single system
  }


 attachInterrupt(digitalPinToInterrupt(INTERUPT_PIN), onboardButtonPressed, RISING);


  attachInterrupt(digitalPinToInterrupt(buttonPins[0]), inputPressed0, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPins[1]), inputPressed1, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPins[2]), inputPressed2, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPins[3]), inputPressed3, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPins[4]), inputPressed4, FALLING);
}

void showMenu(const char * item1, const char * item2, const char * item3) {

      Layout::fillRect(0, 0, DISPLAY_WIDTH, 10, BLACK);
      drawTimeInto(0,0,MENU_TIME_WIDTH, 10, true, LIGHT_BLUE);
      Layout::drawStringInto(DISPLAY_WIDTH/3+ 4, 0, DISPLAY_WIDTH/3, STATUS_BAR_HEIGHT, item1, AlignCenter, LIGHT_BLUE);
      Layout::drawStringInto(DISPLAY_WIDTH*2/3, 0, DISPLAY_WIDTH/3, STATUS_BAR_HEIGHT, item2, AlignCenter, LIGHT_BLUE);
      Layout::drawStringInto(DISPLAY_WIDTH, 0, DISPLAY_WIDTH/3, STATUS_BAR_HEIGHT, item3, AlignRight, LIGHT_BLUE);

    Layout::drawLine(1, 0, 2, 0);
    Layout::drawLine(DISPLAY_WIDTH/3, 0, DISPLAY_WIDTH/3+2, 0);
    Layout::drawLine(DISPLAY_WIDTH*2/3, 0, DISPLAY_WIDTH*2/3+2, 0);
    Layout::drawLine(DISPLAY_WIDTH-2, 0, DISPLAY_WIDTH-1, 0);
}

void transitionState(State st) {
  switch (state) {
    case StateWifiScan:
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      break;
  }
  
  switch(st) {
    case StateTime:
      Layout::setContrast(127);
      Layout::clear();
    showDate(0, 29, DISPLAY_WIDTH-2, DISPLAY_HEIGHT-28); 
      break;

    case StateEnvironment:
      Layout::setContrast(255);
      Layout::clear();
      break;

    case StateMenu:
      Layout::setContrast(127);
      Layout::clear();
      showMenu(" PRV", "NXT", "OK");
      break;

    case StateStopwatch:
      startTime = millis();
      Layout::setContrast(127);
      Layout::clear();
      showMenu("", "RST", "LAP");
      break;

    case StateTimer:
      Layout::setContrast(127);
      Layout::clear();
      showMenu("+10", "+1", "GO");
      break;

    case StateFlashlight:
          Layout::setContrast(255);
          Layout::fillRect(0, 0, DISPLAY_WIDTH-1, DISPLAY_HEIGHT-1, WHITE);
        Layout::swapBuffers();
      break;

     case StateWifiScan:
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
  }

  state = st;

}
 
void handleGlobalButtons() {
if (inputMap&1) {
    if (state == StateTime) {
      transitionState(StateMenu);
    }
    else {
       transitionState(StateTime);     
    }
    inputMap &= ~1;
}
}


void loop()
{  
  //Layout::clear();
 //       Layout::drawStringInto(0,0,DISPLAY_WIDTH,16,"| | . X O");   
//delay(10000);
//return;

  int hours = hour();
  const bool nighttime = (hours > 21) || (hours < 7);

  handleGlobalButtons();

  switch(state) {
    case StateTime:
      showTime(nighttime);  
      delay(1000);
      break;

    case StateEnvironment:
    {
      updateEnvironment(10);  
      delay(1000);
      break;
    }

    case StateStopwatch:
    {
      updateStopwatch(11);
      break;
    }

    case StateMenu:
      updateMenu();
      delay(250);
      break;

    case StateTimer:
      updateTimer();
      delay(100);
      break;

    case StateFlashlight:
      delay(1000);
      break;

    case StateWifiScan:
      updateWifiScan(11, DISPLAY_HEIGHT-11);
      delay(5000);
      break;
  }

}

  static int menuSelected = 0;

struct MenuItem {
  char * name;
  State state;
};

MenuItem menuItems[] = {
  {"Stopwatch", StateStopwatch},
  {"Flashlight", StateFlashlight},
  {"Timer", StateTimer},
  {"Environment", StateEnvironment},
  {"WiFi Scan", StateWifiScan}
};

void updateMenu() {

  if (inputMap&2) {  
      menuSelected+=NUM_OF(menuItems)-1;
      menuSelected %= NUM_OF(menuItems);

      Layout::fillRect(0, 24, DISPLAY_WIDTH, 36, BLACK);
      
      inputMap &= ~2;
  }
  
  if (inputMap&4) {  
      menuSelected++;
      menuSelected %= NUM_OF(menuItems);
      
      Layout::fillRect(0, 24, DISPLAY_WIDTH, 36, BLACK);
      
      inputMap &= ~4;
  }

  if (inputMap&8) {  
      transitionState(menuItems[menuSelected].state);
      inputMap &= ~8;
      return;
  }

  const int topBarHeight = 12;
  const int height = DISPLAY_HEIGHT - topBarHeight;
  const int selectedHeight = 32;
  const int unselectedHeight = (height-selectedHeight) / 2;
  
  
  Layout::drawStringInto(0,topBarHeight, DISPLAY_WIDTH, unselectedHeight, menuItems[(menuSelected+NUM_OF(menuItems)-1)%NUM_OF(menuItems)].name, AlignLeft, 0x9999);
  Layout::drawStringInto(0,topBarHeight+unselectedHeight, DISPLAY_WIDTH, selectedHeight, menuItems[menuSelected].name, AlignLeft);
  Layout::drawStringInto(0,topBarHeight+unselectedHeight+selectedHeight, DISPLAY_WIDTH, unselectedHeight, menuItems[(menuSelected+1)%NUM_OF(menuItems)].name, AlignLeft, 0x9999);
  Layout::swapBuffers();
}

void updateTimer() {
       char buffer[32];
      static int timerSeconds = 0;
      static bool running = false;

              drawTimeInto(0,0,MENU_TIME_WIDTH, 10, true, WHITE);

  if (inputMap&2) {  
      timerSeconds += 600;
      inputMap &= ~2;
            if (running == true) {
        timerSeconds = 0;
        running = false;
      }
       showMenu("+10", "+1", "GO");
  }

  if (inputMap&4) {  
      timerSeconds += 60;
      inputMap &= ~4;
            if (running == true) {
        timerSeconds = 0;
        running = false;
      }
      showMenu("+10", "+1", "GO");
  }

  
  if (inputMap&8) {  
          inputMap &= ~8;
      if (running == true) {
        timerSeconds = 0;
        running = false;
        return;
      }
      timer = now() + timerSeconds;
      running = true;
      timerSeconds = 0;
      showMenu("X", "X", "X");
  }
  
       if (running == true) {
      time_t currentSeconds = now();  
    time_t secondsRemaining = timer-currentSeconds;
    if (secondsRemaining >= 0) {
       Layout::drawDigitsInto(0,25,DISPLAY_WIDTH,DISPLAY_HEIGHT-25,secondsRemaining/60, secondsRemaining%60); 
    }
    else {
       Layout::drawStringInto(0,25,DISPLAY_WIDTH,32,"Done!"); 
    }
       }
       else {
        sprintf(buffer,"SET %02d min",timerSeconds/60); 
        Layout::drawStringInto(0,25,DISPLAY_WIDTH,DISPLAY_HEIGHT-25,buffer);    
       }

    Layout::swapBuffers();
}


void showDate(int x, int y, int w, int h) {
     char buffer[32];
     

      sprintf(buffer,"%02d/%02d/%04d", day(), month(), year());
  Layout::drawStringInto(x, y+10, w, h-10, buffer, AlignLeft, BLUE);

    Layout::drawStringInto(x, y, w, 10, dayStr(weekday()), AlignLeft, BLUE);

  }

void drawTimeInto(int x, int y, int w, int h, bool nighttime, Color color) {
       char buffer[32];
  int hours = hour();
if (!nighttime) {
    Layout::setContrast(127);
   Layout::drawDigitsInto(x,y,w,h,hours, minute(), ':', second(), color);  
}
else {
    Layout::setContrast(0);
    Layout::drawDigitsInto(x,y,w,h,hours, minute(), color);  
}
}

//The Arduino Map function but for floats
//From: http://forum.arduino.cc/index.php?topic=3922.0
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}



int averageAnalogRead()
{
  byte numberOfReadings = 8;
  unsigned int runningValue = 0; 

    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_CHANNEL_6,ADC_ATTEN_6db);

  for(int x = 0 ; x < numberOfReadings ; x++)
    runningValue += adc1_get_raw(ADC1_CHANNEL_6);
  runningValue /= numberOfReadings;

  return(runningValue);  
}

float readUVSensor() {

  digitalWrite(UV_SENSOR_ENABLE_GPIO, HIGH);
  delay(1);
  

    float analog_value = averageAnalogRead();
  

  
  float uvIntensity = mapfloat(analog_value/2000.0f, 1.0, 2.9, 0.0, 15.0);

  /*Serial.print(" UV Intensity (mW/cm^2): ");
  Serial.print(uvIntensity);
  
  Serial.println();*/

        digitalWrite(UV_SENSOR_ENABLE_GPIO, LOW);


      inputMap &= ~2; // stop this triggering, ADC interferes with this GPIO

return uvIntensity;
}

void showTime(bool noSeconds){
    char buffer[32];
    const int timeHeight = 28;
    const int appointmentHeight = 28;

if (inputMap&16) {
      inputMap &= ~16;
}

  if (inputMap&2) {  
      inputMap &= ~2;
    transitionState(menuItems[menuSelected].state);
    return;
  }


if (noSeconds && lastMinute == minute()) {
  return;
}
  lastMinute = minute();
  
    if (day() != lastDay) {
     Layout::clear();
     showDate(0, 29, DISPLAY_WIDTH, DISPLAY_HEIGHT-28); 
    lastDay = day(); 
    }

drawTimeInto(0,0,DISPLAY_WIDTH, timeHeight, noSeconds, WHITE);

for (int i=0; i<sizeof(calendarEvents) / sizeof(calendarEvents[0]); i++) {
  CalendarEvent event = calendarEvents[i];
  if (event.day == weekday()) {
    int currentSeconds = hour() * 3600 + minute()*60 + second();  
    int secondsRemaining = event.minute*60 - currentSeconds;
    if (secondsRemaining >= 0 && secondsRemaining <= event.countdownMinutes*60) {
          Layout::drawDigitsInto(0, timeHeight-2, DISPLAY_WIDTH,appointmentHeight,secondsRemaining/60, secondsRemaining%60, AlignLeft, RED);  
    Layout::drawStringInto(66, timeHeight+appointmentHeight-4, DISPLAY_WIDTH, DISPLAY_HEIGHT-(timeHeight+appointmentHeight)+3, event.name, AlignRight, RED);

        const int top = 34;
        const int bot = 53;
        const int mid = (top+bot)/2;
    Layout::drawLine(72, top, 78, mid, RED);
    Layout::drawLine(72, bot, 78, mid, RED);
    Layout::drawLine(72, top, 72, bot, RED);
    }
    else if (secondsRemaining == -1) {
      Layout::clear();
      showDate(0, timeHeight-2, DISPLAY_WIDTH, DISPLAY_HEIGHT-timeHeight); 
    }

  }
}
  Layout::swapBuffers();
}

void updateEnvironment(int yOffset) {

int w = DISPLAY_HEIGHT;
  float uv = readUVSensor();

     char buffer[32];
     
  int y= yOffset;
  int h = 14;
      sprintf(buffer,"UV: mW/cm^2"); 
        Layout::drawStringInto(0, y, w, h, buffer, AlignLeft, BLUE); y += h;

      sprintf(buffer,"%.02f", uv); 
        Layout::drawStringInto(0, y, w, h, buffer, AlignLeft, WHITE); y += h;

        
      sprintf(buffer,"UV Index"); 
        Layout::drawStringInto(0, y, w, h, buffer, AlignLeft, BLUE); y += h;

      sprintf(buffer,"%.01f", uv*10000/25); 
        Layout::drawStringInto(0, y, w, h, buffer, AlignLeft, WHITE); y += h;

  Layout::swapBuffers();
}

void updateStopwatch(int yOffset) {
    auto diff = millis() - startTime;
    auto sec = diff/1000;

     static auto lastTimePrint = 0;
     lastTimePrint += diff;
     if (lastTimePrint > 10000) {
      drawTimeInto(0,0,MENU_TIME_WIDTH, 10, true, WHITE); 
      lastTimePrint = 0;
     }

      Layout::drawDigitsInto(0,yOffset,DISPLAY_WIDTH,26,sec/60, sec%60, '.', ((diff/100)%10)*10);  
      Layout::swapBuffers();
      delay(100);

      if (inputMap&4) {
        startTime = millis();
            inputMap &= ~4;
      }

      if (inputMap&8) {
      Layout::drawDigitsInto(0, yOffset+26, DISPLAY_WIDTH,26, sec/60, sec%60, '.', ((diff/100)%10)*10);
            inputMap &= ~8;
      }
}



const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

WiFi.disconnect();
WiFi.mode(WIFI_OFF);
      
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void updateWifiScan(int yOffset, int h) {
  const uint8_t lineSpacing = 9;

  Layout::drawStringInto(DISPLAY_WIDTH-20, 0, DISPLAY_WIDTH, 10, "Scanning...", AlignRight);
  Layout::swapBuffers();


    int n = WiFi.scanNetworks(false, false);

  Layout::clear();
    
    if (n == 0) {
          Layout::drawStringInto(0, yOffset, DISPLAY_WIDTH, DISPLAY_WIDTH-yOffset, "no networks found");
    } else {

        const int num = _min(h/lineSpacing, n-1);
      
        for (int i = 0; i <= num; ++i) {
         char buffer[64];
         sprintf(buffer, "%ddBm %c%s", WiFi.RSSI(i), (WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?' ':'*', WiFi.SSID(i).c_str() );
          Layout::drawStringInto(0, i*lineSpacing+yOffset,DISPLAY_WIDTH, lineSpacing, buffer);
        }
    }
  Layout::swapBuffers();  
}

