// Smartwatch based on the ESP32 + SSD1306 OLED
// (c)2017 James Hudson, released under the GPL-3.0 license

// NTP based on https://www.arduino.cc/en/Tutorial/UdpNTPClient

// 55mA draw just showing time, changing every second

// Note on ESP32 sleep modes: the deep and light sleeps both work fine with the RTC,
// but on the "TTGO ESP32 OLED" board, the OLED screen blanks itself on sleep because of the reset pin
// configuration. This makes it not so useful for a watch, discounting any hardware mods to
// the board. So GPIO0 is used as an on/off switch.

 
#include <TimeLib.h> 
#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include <WiFiUdp.h>

#include "util.h"
#include "states.h"
#include "layout.h"
#include "fonts.h"
#include "web.h"

#include "esp32notifications.h"

#include <esp_adc/adc_oneshot.h>

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

#define UV_SENSOR_ENABLE_GPIO 22

inline float mWcm2ToUVI(float mWcm2) { return mWcm2*3; }

static unsigned char uviBuffer[10*60*12];
static int bufferPos;

// NTP Servers:
IPAddress timeServer(216,239,35,8); // time.google.com

const int timeZone = 2; // Berlin winter is 1. Summer is 2

WiFiUDP Udp;


BLENotifications notifications;

static const int MENU_TIME_WIDTH = 30;
static const int STATUS_BAR_HEIGHT = 10;
static const int TIME_Y_POS = STATUS_BAR_HEIGHT; 
static const int timeHeight = 28;
static const int appointmentHeight = 28;
static const int APPOINTMENT_Y_POS = TIME_Y_POS+timeHeight-2;

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
CalendarEvent calendarEvents[] PROGMEM = {
  {"H'str U8", 1, 11*50, 20},
 /* {"Train OK", 3, 10*60, 30},
  {"Train OK", 4, 10*60, 30},
  {"Train OK", 5, 10*60, 30},*/
  {"H'str 277", 5, 20*60 + 5, 30},
};



int buttonPins[] = {32, 25, 26, 27, 35};
uint32_t inputMap;

State state = StateTime;

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
    time_t got_time = now();
 return (got_time > 1000);
}

const char btLogo[] PROGMEM = {0b1000100,0, 0b101000, 0, 0b11111111, 1,0b10101010,0,0b1000100,0};

void onBLEStateChanged(BLENotifications::State state) {
  uint16_t color = BLUE;
  switch (state) {
    case BLENotifications::StateConnected: color = GREEN; break;
    case BLENotifications::StateDisconnected:
      color = DARK_GRAY;

      /* We need to startAdvertising on disconnection, otherwise the ESP 32 will now be invisible.
      IMO it would make sense to put this in the library to happen automatically, but some people in the Espressif forums
      were requesting that they would like manual control over re-advertising.*/
      notifications.startAdvertising(); 
      
      break;
  }

  Layout::drawSprite1Bit(60, 1, 5, 9, btLogo, sizeof(btLogo), color);
}


// A notification arrived from the mobile device, ie a social media notification or incoming call.
void onNotificationArrived(const ArduinoNotification * notification, const Notification * rawNotification) {
    int textHeight = 11;

    String body(notification->message);
    //body.remove(20);

    Layout::drawStringInto(0, DISPLAY_HEIGHT-textHeight*2-2, DISPLAY_WIDTH, textHeight, notification->title, AlignLeft, LIGHT_BLUE);
    Layout::drawStringInto(0, DISPLAY_HEIGHT-textHeight-2, DISPLAY_WIDTH, textHeight, body, AlignLeft, LIGHT_BLUE);
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

  memset(uviBuffer, 0, NUM_OF(uviBuffer));

  delay(50);
  if (isTimeSet()) {
    Serial.println("Woo! we woke up");
    timeval time_now; 
    gettimeofday(&time_now, NULL); 
    setTime(time_now.tv_sec); // @todo use a single system
  }
  else {
  
    WiFi.mode(WIFI_STA);

    WiFiManager wm;

    int i=0;
    while (!wm.autoConnect("Hackwatch")) {
      Layout::clear();
      Layout::drawStringInto(0, 0, DISPLAY_WIDTH, 18, "Connecting...", AlignLeft, GREEN);
      Layout::drawStringInto(0, 18, DISPLAY_WIDTH, 18, String(i), AlignLeft, WHITE);
      Layout::drawStringInto(0, 36, DISPLAY_WIDTH, 18, String(WiFi.status()), AlignLeft, GREEN);
      Layout::swapBuffers();

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

  notifications.begin("Hackwatch");
    notifications.setConnectionStateChangedCallback(onBLEStateChanged);
    notifications.setNotificationCallback(onNotificationArrived);


 attachInterrupt(digitalPinToInterrupt(INTERUPT_PIN), onboardButtonPressed, RISING);


  attachInterrupt(digitalPinToInterrupt(buttonPins[0]), inputPressed0, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPins[1]), inputPressed1, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPins[2]), inputPressed2, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPins[3]), inputPressed3, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPins[4]), inputPressed4, FALLING);
}

void showMenu(const char * item1, const char * item2, const char * item3) {

      Layout::fillRect(0, 0, DISPLAY_WIDTH, 10, BLACK, BLACK);
      
      drawTimeInto(0,0,MENU_TIME_WIDTH, STATUS_BAR_HEIGHT, true, LIGHT_BLUE);

      Layout::drawStringInto(DISPLAY_WIDTH/3+ 4, 0, DISPLAY_WIDTH/3, STATUS_BAR_HEIGHT, item1, AlignCenter, LIGHT_BLUE);
       
      Layout::drawStringInto(DISPLAY_WIDTH*2/3, 0, DISPLAY_WIDTH/3, STATUS_BAR_HEIGHT, item2, AlignCenter, LIGHT_BLUE);
      Layout::drawStringInto(DISPLAY_WIDTH-22, 0, 22, STATUS_BAR_HEIGHT, item3, AlignRight, LIGHT_BLUE);


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
      Layout::clear();
    showDate(0, 0, DISPLAY_WIDTH-2, 12); 
      break;

    case StateEnvironment:
    {
      Layout::setContrast(255);
      Layout::clear();

      drawUVGraph();

    }
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
          Layout::fillRect(0, 0, DISPLAY_WIDTH-1, DISPLAY_HEIGHT-1, WHITE, WHITE);
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
      delay(100);
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

MenuItem menuItems[] PROGMEM = {
  {"Environment", StateEnvironment},
  {"Stopwatch", StateStopwatch},
  //{"Flashlight", StateFlashlight},
  {"Timer", StateTimer},
  {"WiFi Scan", StateWifiScan}
};

void updateMenu() {

  if (inputMap == 0) {
    return;
  }

  if (inputMap&2) {  
      menuSelected+=NUM_OF(menuItems)-1;
      menuSelected %= NUM_OF(menuItems);

      Layout::fillRect(0, 24, DISPLAY_WIDTH, 12, BLACK, BLACK);
      
      inputMap &= ~2;
  }
  
  if (inputMap&4) {  
      menuSelected++;
      menuSelected %= NUM_OF(menuItems);
      
      Layout::fillRect(0, 24, DISPLAY_WIDTH, 12, BLACK, BLACK);
      
      inputMap &= ~4;
  }

  if (inputMap&8) {  
      transitionState(menuItems[menuSelected].state);
      inputMap &= ~8;
      return;
  }

  const int topBarHeight = 12;
  const int height = DISPLAY_HEIGHT - topBarHeight;
  const int selectedHeight = 30;
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
  sprintf(buffer,"%s %02d/%02d", dayShortStr(weekday()), day(), month());
  Layout::drawStringInto(x, y, w, 10, buffer, AlignLeft, BLUE);
  }

void drawTimeInto(int x, int y, int w, int h, bool nighttime, Color color) {
       char buffer[32];
  int hours = hour();
if (!nighttime) {
   Layout::drawDigitsInto(x,y,w,h,hours, minute(), ':', second(), color);  
}
else {
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
  byte numberOfReadings = 4;
  unsigned int runningValue = 0; 


   adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_6, 
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config));



  for(int x = 0 ; x < numberOfReadings ; x++) {
    int adc_raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &adc_raw));
    runningValue += adc_raw;
    runningValue /= numberOfReadings;
  }

    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));

  return(runningValue);  
}

float readUVSensor() {

  digitalWrite(UV_SENSOR_ENABLE_GPIO, HIGH);
  delay(1);
  

    float analog_value = averageAnalogRead();
  

  
  float uvIntensity = mapfloat(analog_value/2000.0f, 1.009, 2.9, 0.0, 15.0);

  /*Serial.print(" UV Intensity (mW/cm^2): ");
  Serial.print(uvIntensity);
  
  Serial.println();*/

        digitalWrite(UV_SENSOR_ENABLE_GPIO, LOW);


      inputMap &= ~2; // stop this triggering, ADC interferes with this GPIO for some reason

return uvIntensity;
}

Color colorForUVI(float uvi) {
  Color color;
        if (uvi < 3) {
        color = GREEN;
      }
      else if (uvi < 6) {
        color = YELLOW;
      }
      else if (uvi < 8) {
        color = ORANGE;
      }
      else if (uvi < 11) {
        color = RED;
      }
      else {
        color = VIOLET;
      }
      return color;
}

void updateUVIDisplay(float last_uv) {
      char buffer[32];

    int brightness = 255;
      Color color;

      float uvi = mWcm2ToUVI(last_uv); 

      // use UV to control global display dimming
      if (uvi < 0.2) {
        brightness = 0;
      }
      else if (uvi < 0.8) {
        brightness = (uvi-0.2)*600;
      }
      
      color = colorForUVI(uvi);
      sprintf(buffer,"%.01f", uvi); 

      Layout::drawStringInto(76, 0, DISPLAY_WIDTH-77, 12, buffer, AlignLeft, color);
      Layout::fillRect(71, 3, 3, STATUS_BAR_HEIGHT-4, color, color);

      Layout::setContrast(brightness);
}

void showTime(bool noSeconds){
    char buffer[32];

if (inputMap&16) {
      inputMap &= ~16;

  WiFi.mode(WIFI_STA);
    int status = WL_IDLE_STATUS; 
    int i=0;
    while (status != WL_CONNECTED) {
            Layout::fillRect(0, TIME_Y_POS, DISPLAY_WIDTH, timeHeight, BLACK, BLACK);
      Layout::drawStringInto(0, 18, DISPLAY_WIDTH, 18, "Toggling light...", AlignLeft, GREEN);
      Layout::swapBuffers();

      i++;

      if (i==5) {
        Layout::clear();
        Layout::drawStringInto(0, 18, DISPLAY_WIDTH, 14, "Timed out.", AlignLeft, RED);  
        Layout::swapBuffers();
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        return; 
      }
      delay(2000);
    }
      
  toggleRoomLights();

WiFi.disconnect();
WiFi.mode(WIFI_OFF);
      
 /*     transitionState(StateFlashlight);
      return;
      */
}

  if (inputMap&2) {  
      inputMap &= ~2;
    transitionState(menuItems[menuSelected].state);
    return;
  }

  

if (lastMinute == minute()) {
if (noSeconds) {
  return;
}

}

  lastMinute = minute();

    bool sensorUpdated = false;
    float last_uv = -1;
  if ((second()%6) == 0) {
    last_uv = readUVSensor();
    uviBuffer[bufferPos] = mWcm2ToUVI(last_uv*10);
    bufferPos++;
    bufferPos %= NUM_OF(uviBuffer);
    sensorUpdated = true;
  }
  
    if (day() != lastDay) {
     Layout::clear();
     showDate(0, 0, DISPLAY_WIDTH, 12); 
    lastDay = day(); 
    }

drawTimeInto(0, TIME_Y_POS, DISPLAY_WIDTH, timeHeight, noSeconds, WHITE);

for (int i=0; i<sizeof(calendarEvents) / sizeof(calendarEvents[0]); i++) {
  CalendarEvent event = calendarEvents[i];
  if (event.day == weekday()) {
    int currentSeconds = hour() * 3600 + minute()*60 + second();  
    int secondsRemaining = event.minute*60 - currentSeconds;
    if (secondsRemaining >= 0 && secondsRemaining <= event.countdownMinutes*60) {
          Layout::drawDigitsInto(0, APPOINTMENT_Y_POS, DISPLAY_WIDTH,appointmentHeight, secondsRemaining/60, secondsRemaining%60, RED);  
    Layout::drawStringInto(0, APPOINTMENT_Y_POS+appointmentHeight-2, 66, DISPLAY_HEIGHT-(timeHeight+appointmentHeight)+3, event.name, AlignRight, RED);

        const int top = APPOINTMENT_Y_POS+4;
        const int bot = APPOINTMENT_Y_POS+23;
        const int mid = (top+bot)/2;
    Layout::drawLine(72, top, 78, mid, RED);
    Layout::drawLine(72, bot, 78, mid, RED);
    Layout::drawLine(72, top, 72, bot, RED);
    sensorUpdated = false;
    }
    else if (secondsRemaining == -1) {
      Layout::clear();
      showDate(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT-timeHeight); 
    }
  }
}
    if (sensorUpdated) {
      updateUVIDisplay(last_uv);
    }

  Layout::swapBuffers();
}

void updateEnvironment(int yOffset) {

  float last_uv = readUVSensor();

     char buffer[32];
     
  int y= yOffset;
  int h = 10;
  const int bucketSize = 20;
      sprintf(buffer,"UV: %.02f", last_uv); 
        Layout::drawStringInto(0, y, 50, h, buffer, AlignLeft, BLUE); y += h;

  Layout::scroll(1, yOffset, DISPLAY_WIDTH-1, DISPLAY_HEIGHT-1, 0, yOffset);
  static int anim;
    int start = (NUM_OF(uviBuffer)+bufferPos+anim) % NUM_OF(uviBuffer);
  int readFrom = start;
  anim+=bucketSize;

  Layout::drawLine(DISPLAY_WIDTH-1, yOffset, DISPLAY_WIDTH-1,  DISPLAY_HEIGHT - 1, BLACK);

  float uvi = 0;
  for (int j=0; j<bucketSize; j++) {
    readFrom %= NUM_OF(uviBuffer);
    auto val = uviBuffer[readFrom];

    if((readFrom%600) == 0) {
      Layout::drawLine(DISPLAY_WIDTH-1, yOffset, DISPLAY_WIDTH-1, DISPLAY_HEIGHT - 1, BLUE);  
      sprintf(buffer,"%d", (bufferPos-start)/600-1);  
      Layout::drawStringInto(DISPLAY_WIDTH-16, y, 10, h, buffer, AlignLeft, BLUE); y += h;   
    }
    
    readFrom++;
    uvi = _max(uvi, float(val));
  }

  y = DISPLAY_HEIGHT - uvi/2 - 1;
  Layout::drawLine(DISPLAY_WIDTH-1, y, DISPLAY_WIDTH-1, DISPLAY_HEIGHT - 1, colorForUVI(uvi/10));
  

  Layout::swapBuffers();
}

void drawUVGraph() {
  int pixelWidth = NUM_OF(uviBuffer)/DISPLAY_WIDTH;
  auto w = NUM_OF(uviBuffer)/pixelWidth;
int readFrom = NUM_OF(uviBuffer)+bufferPos;
for (int i=0; i< w; i++) {
  float uvi = 0;
  for (int j=0; j<pixelWidth; j++) {
    readFrom %= NUM_OF(uviBuffer);
    auto val = uviBuffer[readFrom];
    readFrom++;
    uvi = _max(uvi, float(val));
  }

  Layout::drawLine(i, DISPLAY_HEIGHT - uvi/2 - 1, i, DISPLAY_HEIGHT - 1, colorForUVI(uvi/10));
}
}

void updateStopwatch(int yOffset) {
    auto diff = millis() - startTime;
    auto sec = diff/1000;


   /* uviBuffer[bufferPos] = (millis()/1000) % 120;
    bufferPos++;
    bufferPos %= NUM_OF(uviBuffer);*/


     static auto lastTimePrint = 0;
     lastTimePrint += diff;
     if (lastTimePrint > 10000) {
      drawTimeInto(0,0,MENU_TIME_WIDTH, 11, true, WHITE); 
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
      Serial.println(F("Receive NTP Response"));
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
  const uint8_t lineSpacing = 13;

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
