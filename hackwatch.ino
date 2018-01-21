// Smartwatch based on the ESP32 + SSD1306 OLED
// (c)2017 James Hudson, released under the MIT license

// 55mA draw just showing time, changing every second
// Needs a small off switch just to save power
// It should sit flat, even if it means moving the battery to the side in a separate pouch.
 
#include <TimeLib.h> 
#include <WiFi.h>
#include <WiFiUdp.h>
#include "SSD1306.h" 

// Anything we don't want to commit publicly to git (passwords, keys, etc.)
#include "secrets.h"

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
  char name[9];
  char day;
  short int minute;
  short int countdownMinutes;
};

CalendarEvent calendarEvents[] = {
  {"Train OK", 2, 10*60, 60},
    {"Train OK", 3, 10*60, 60},
      {"Train OK", 4, 10*60, 60},
        {"Train OK", 5, 10*60, 60},
  {"Bus M46", 5, 20*60 + 11, 45},
    {"Train OK", 6, 10*60, 60},
};

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

  /*Serial.begin(9600);
  delay(250);
  Serial.println(ssid);*/
  WiFi.begin(ssid, pass);

  int i=0;
  while (WiFi.status() != WL_CONNECTED) {
    display.clear();
        display.drawString(0, 0, "Connecting...");
    display.drawString(0, 10, i&1 ? "." : " ");
       display.drawString(0, 30, String(WiFi.status()));
      display.display();
          delay(500);
          i++;
  }

  
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());

  Udp.begin(8888);

    display.clear();
    display.drawString(0, 0, "Getting time...");
    display.display();

  setSyncProvider(getNtpTime);

  startTime = now();
}

void loop()
{  
  
  if (timeStatus() != timeNotSet) {
    if (now() != lastTime) { 
      lastTime = now();
      showTime();  
    }
  }


  if (digitalRead(0) != 1) {
        display.setFont(ArialMT_Plain_10);
        display.clear();
            char buffer[32];
        time_t secElapsed = now() - startTime;
        display.drawString(0, 0, "DEBUG INFO");
         sprintf(buffer,"Wifi SSID: %s", ssid);
         display.drawString(0, 20, buffer);
    sprintf(buffer,"Uptime: %d:%02d:%02d", secElapsed/3600, (secElapsed/60)%60, secElapsed%60);
        display.drawString(0, 39, buffer);
        display.display();
  }
  
  delay(1000);
}


void showDate() {
     char buffer[32];
    display.setFont(ArialMT_Plain_16);

    display.drawString(00, 28, dayStr(weekday()));

    sprintf(buffer,"%02d/%02d/%04d", day(), month(), year());
    display.drawString(2, 49, buffer);
  }

void showTime(){
    char buffer[32];
  
    if (day() != lastDay) {
        display.clear();
      showDate();
    lastDay = day(); 
    }


display.setColor(BLACK);
display.fillRect(0, 0, DISPLAY_WIDTH-10, 24);
display.setColor(WHITE);

    sprintf(buffer,"%02d:%02d:%02d", hour(), minute(), second());
    display.setFont(ArialMT_Plain_24);
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
    
    display.setFont(ArialMT_Plain_24);

    display.drawString(0, 28, buffer);
    
    display.setFont(ArialMT_Plain_10);
        display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(59, 50, event.name);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        const int top = 32;
        const int bot = 59;
        const int mid = (top+bot)/2;
    display.drawLine(62, top, 72, mid);
    display.drawLine(62, bot, 72, mid);
    display.drawVerticalLine(62, top, bot-top);
    }
    else if (secondsRemaining == -1) {
             display.clear();
      showDate(); 
    }

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
