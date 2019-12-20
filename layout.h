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

  static void drawLine(int x, int y, int x2, int y2, Color color = WHITE);
  static void fillRect(int x, int y, int w, int h, Color color = WHITE, Color border = WHITE);

  /**
   * Draw a 1 bit sprite.
   * Sprites are drawn in vertical runs.
   * If its height is not a power of 8, then the remainder of the byte is padded with zeros.
   */
  static void drawSprite1Bit(int x, int y, int w, int h, const char * data, unsigned int byteLength, Color color);

  static const char * drawDigitsInto(int x, int y, int w, int h, int m, int s, Color color = WHITE);
  static const char * drawDigitsInto(int x, int y, int w, int h, int hr, int m, char separator, int s, Color color = WHITE);
  
  static const char * drawStringInto(int x, int y, int w, int h, String str, Alignment align = AlignLeft, Color color = WHITE);

	static const char * getFontForHeight(unsigned char height);
  static const char * getFontForSize( String str, unsigned char width, unsigned char height);

  static void scroll(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t X, uint8_t Y);

};
#endif // LAYOUT_H_
