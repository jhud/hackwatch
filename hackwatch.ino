// Smartwatch based on the ESP32 + SSD1306 OLED
// (c)2017 James Hudson, released under the MIT license

// NTP based on https://www.arduino.cc/en/Tutorial/UdpNTPClient

// 55mA draw just showing time, changing every second
// Needs a small off switch just to save power
// It should sit flat, even if it means moving the battery to the side in a separate pouch.
 
#include <TimeLib.h> 
#include <WiFi.h>
#include <WiFiMulti.h>

#include <WiFiUdp.h>
#include "SSD1306.h" 

#define NUM_OF(x) (sizeof(x)/sizeof(x[0])) // Define because it's useful

// Anything we don't want to commit publicly to git (passwords, keys, etc.)
// You ned to create this file yourself, like:
// const char* ssid[3] = {"yourwifi", "secondwifi", "3rd..."};  //  your network SSID (name)
// const char* pass[3] = {"yourpwd", "secondpwd", "3rdpwd..."};       // your network password
#include "secrets.h"

#include "fonts.h"

//#ifdef LEFT_HAND // if the watch will be worn on the left hand

//OLED pins to ESP32 GPIOs via this connecthin:
//OLED_SDA -- GPIO4
//OLED_SCL -- GPIO15
//OLED_RST -- GPIO16

SSD1306  display(0x3c, 4, 15); // addr, SDA, SCL (reset = GPIO16)

// NTP Servers:
IPAddress timeServer(216,239,35,8); // time.google.com

const int timeZone = 1; // Berlin

WiFiUDP Udp;

time_t startTime = 0; 
time_t lastTime = 0; // Last time time was displayed
int lastDay = -1;

typedef struct CalendarEvent {
  char name[11];
  char day;
  short int minute;
  short int countdownMinutes;
};

CalendarEvent calendarEvents[] = {
  {"Train OK", 2, 10*60, 60},
    {"Train OK", 3, 10*60, 60},
      {"Train OK", 4, 10*60, 60},
        {"Train OK", 5, 10*60, 60},
  {"U1 Schlesi", 5, 20*60 + 02, 45},
    {"Train OK", 6, 10*60, 60},
};


WiFiMulti wifiMulti;

void setup() 
{
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high

  pinMode(0, INPUT); // Let us use flash button on dev board as a function button

  display.init();

#ifdef LEFT_HAND
  display.flipScreenVertically();
#endif

    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);

  WiFi.mode(WIFI_STA);

  for (int i=0; i<NUM_OF(ssid); i++) {
      wifiMulti.addAP(ssid[i], pass[i]);
  }

  int status = WL_IDLE_STATUS; 
  int i=0;
  while (status != WL_CONNECTED) {
    display.clear();
        display.drawString(0, 0, "Connecting...");
    display.drawString(0, 14, String(i));
       display.drawString(0, 32, String(WiFi.status()));
      display.display();
     status = wifiMulti.run();

                    i++;

      if (i==5) {
          display.clear();
         display.drawString(0, 0, "Timed out. Sleeping.");  
         display.display(); 
         WiFi.disconnect();
         WiFi.mode(WIFI_OFF);
         delay(500000);
           WiFi.mode(WIFI_STA);
         i=0;  
      }
          delay(10000);
  }

  

  Udp.begin(8888);

    display.clear();
    display.drawString(0, 0, "Getting time...");
    display.display();

  setSyncProvider(getNtpTime);

  startTime = now();
}

void loop()
{  

  int hours = hour();
  const bool nighttime = (hours > 21) || (hours < 7);
  
  if (timeStatus() != timeNotSet) {
    if (now() != lastTime) { 
      lastTime = now();
      showTime(nighttime);  
    }
  }


  if (digitalRead(0) != 1) {
        display.setFont(ArialMT_Plain_10);
        display.clear();
            char buffer[32];
        time_t secElapsed = now() - startTime;
        display.drawString(0, 0, "DEBUG INFO");
         sprintf(buffer,"Wifi SSID: %s", WiFi.localIP());
         display.drawString(0, 20, buffer);
    sprintf(buffer,"Uptime: %d:%02d:%02d", secElapsed/3600, (secElapsed/60)%60, secElapsed%60);
        display.drawString(0, 39, buffer);
        display.display();
  }

  if (nighttime && second() == 0) {
    delay(60000);
  }
  else {
    delay(1000);
  }
}


void showDate(int x, int y) {
     char buffer[32];
    display.setFont(Lato_Hairline_10);

    display.drawString(x, y, dayStr(weekday()));

   display.setFont(ArialMT_Plain_16);
    sprintf(buffer,"%02d/%02d/%04d", day(), month(), year());
    display.drawString(x, y+13, buffer);
  }

void showTime(bool nighttime){
    char buffer[32];
  
    if (day() != lastDay) {
        display.clear();
      showDate(2, 29);
    lastDay = day(); 
    }


  const char *timeFont = nighttime ? Lato_Hairline_24 : Lato_Semibold_26;

display.setColor(BLACK);
display.fillRect(0, 0, DISPLAY_WIDTH-10, 26);
display.setColor(WHITE);

int hours = hour();
if (!nighttime) {
    sprintf(buffer,"%02d:%02d:%02d", hours, minute(), second());
}
else {
    sprintf(buffer,"%02d:%02d", hours, minute());  
}

    display.setFont(timeFont);
    display.drawString(0, 0, buffer);

for (int i=0; i<sizeof(calendarEvents) / sizeof(calendarEvents[0]); i++) {
  CalendarEvent event = calendarEvents[i];
  if (event.day == weekday()) {
    int currentSeconds = hour() * 3600 + minute()*60 + second();  
    int secondsRemaining = event.minute*60 - currentSeconds;
    if (secondsRemaining >= 0 && secondsRemaining <= event.countdownMinutes*60) {
    sprintf(buffer,"%02d:%02d",secondsRemaining/60, secondsRemaining%60);

display.setColor(BLACK);
display.fillRect(0, 28, DISPLAY_WIDTH-10, DISPLAY_HEIGHT-28);
display.setColor(WHITE);
    
    display.setFont(timeFont);

    display.drawString(0, 28, buffer);
    
    display.setFont(ArialMT_Plain_10);
        display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(59, 50, event.name);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        const int top = 32;
        const int bot = 59;
        const int mid = (top+bot)/2;
    display.drawLine(66, top, 72, mid);
    display.drawLine(66, bot, 72, mid);
    display.drawVerticalLine(62, top, bot-top);
    }
    else if (secondsRemaining == -1) {
             display.clear();
      showDate(2, 29); 
    }

  }
}



if (!nighttime) {
  time_t secElapsed = now();
  for (int i=0; i<32; i++) {
    display.setColor((secElapsed&1) == 0 ? BLACK : WHITE);

      display.fillRect(124-i*4, DISPLAY_HEIGHT-4, 4, 4);
  secElapsed >>= 1;
   
  }
}
 
  display.display();
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
