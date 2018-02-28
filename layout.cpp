#include <Arduino.h>

#include "fonts.h"
#include "layout.h"
#include "util.h"

#include "SSD1306.h" 

static const int HEIGHT_INDEX = 1; // From the font format at http://oleddisplay.squix.ch/

static const char * fonts[] = {Lato_Regular_32, Lato_Semibold_26, Lato_Light_24, Lato_Hairline_16, Lato_Regular_12, ArialMT_Plain_10, Lato_Hairline_10};

extern SSD1306 display;


const char * Layout::drawStringInto(int x, int y, int w, int h, String str) {
	display.setColor(BLACK);
	display.fillRect(x, y, w, h);
	display.setColor(WHITE);
	
	
	    display.setFont(Layout::getFontForSize(str, w,h));
        display.drawString(x, y, str);
  }
  
  const char * Layout::drawDigitsInto(int x, int y, int w, int h, int hr, int m, char separator, int s) {
	         char buffer[20];
			 sprintf(buffer, "%02d:%02d:%02d", hr, m, s);
			 drawStringInto(x, y, w, h, buffer);

    }
	
    const char * Layout::drawDigitsInto(int x, int y, int w, int h, int m, int s) {
  	         char buffer[20];
  			 sprintf(buffer, "%02d:%02d", m, s);
  			 drawStringInto(x, y, w, h, buffer);

      }
  
  const char * Layout::getFontForSize( String str, unsigned char width, unsigned char height) {
  	for (int i = 0; i < NUM_OF(fonts); i++ )
  	{
  		if (height >= fonts[i][HEIGHT_INDEX]) {
			display.setFont(fonts[i]);
			int w = display.getStringWidth(str);
			if (w <= width) {
  				return fonts[i];
			}
  		}
  	}
  	return fonts[NUM_OF(fonts)-1];
  }
  
const char * Layout::getFontForHeight(unsigned char height) {
	for (int i = 0; i < NUM_OF(fonts); i++ )
	{
		if (height >= fonts[i][HEIGHT_INDEX]) {
			return fonts[i];
		}
	}
	return fonts[NUM_OF(fonts)-1];
}
