# hackwatch
Want to run your Arduino code anywhere, any time? How about running a webserver from your wrist? This is a smartwatch running on the ESP32 + Arduino + 0.96' OLED. Designed to run on cheap boards that have inbuilt LiPo battery circuitry and an OLED screen.

Currently, the code doesn't do more than show the time and date, and has some basic hardcoded calendar functionality, which will remind you when you need to leave to catch buses, etc. I'll add to it as I need features and find time.

The "flash" button on the GPIO0 of the development board is repurposed as a function button for the watch.

It makes for quite a chunky watch, but the "always on" glow looks very cool, and a 500mAH battery lasts 10+ hours. Further power optimisations (i.e. deep sleep) should be easily implemented.
 
To do:
- Reduce power consumption
- Notifications from phone
- Slow down at night, dim if no program is running. 
- Piezo chime or vibrate
