# hackwatch
Want to run your Arduino code anywhere, any time? How about running a webserver from your wrist? This is a smartwatch running on the ESP32 + Arduino + 0.96' OLED. Designed to run on cheap boards that have inbuilt LiPo battery circuitry and an OLED screen. It makes for quite a chunky watch, but the "always on" glow looks very cool, and a 500mAH battery currently lasts 10+ hours without any optimisation. The whole prototype was built for $12.

![Picture of hackwatch prototype](hackwatch.jpg?raw=true "Hackwatch prototype")

Currently, the code uses your WiFi to sync with an NTP server, then show the time and date. It also has some basic hardcoded calendar functionality, which will remind you when you need to catch buses, etc. I'll improve this project as I need features and find time.

The "flash" button on the GPIO0 of the development board is repurposed as a function button for the watch.


# Setup
You need to create a file called "secrets.h" which has your WiFi SSID and password:

const char ssid[] = "my access point";  //  your network SSID (name)
const char pass[] = "MyPassword12345";       // your network password



# Major tasks yet to do:
- Reduce power consumption (i.e. deep sleep)
- Notifications from phone
- Take calendar from a server
- Slow down at night, dim if no program is running. 
- Piezo chime or vibrate
