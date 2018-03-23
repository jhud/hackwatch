#ifndef LAYOUT_H_
#define LAYOUT_H_

enum Alignment {
  AlignLeft = 0,
  AlignCenter,
  AlignRight
};

#define COLOR_SCREEN

#ifdef COLOR_SCREEN
#include "SSD1331Extended.h"
#else
#include "SSD1306.h" 
#endif


#define Color uint32_t

class Layout {
public:

  static void init(bool leftHanded);

  static void swapBuffers();
  static void clear();

  static void enableDisplay(bool on);

  static void setContrast(int val);

  static void drawLine(int x, int y, int x2, int y2, Color color = 255);
  static void fillRect(int x, int y, int w, int h, Color color);

  static const char * drawDigitsInto(int x, int y, int w, int h, int m, int s);
  static const char * drawDigitsInto(int x, int y, int w, int h, int hr, int m, char separator, int s);
  
  static const char * drawStringInto(int x, int y, int w, int h, String str, Alignment align = AlignLeft);

	static const char * getFontForHeight(unsigned char height);
  static const char * getFontForSize( String str, unsigned char width, unsigned char height);

};
#endif // LAYOUT_H_
