#ifndef LAYOUT_H_
#define LAYOUT_H_

class Layout {
public:

  static const char * drawDigitsInto(int x, int y, int w, int h, int m, int s);
  static const char * drawDigitsInto(int x, int y, int w, int h, int hr, int m, char separator, int s);
  
  static const char * drawStringInto(int x, int y, int w, int h, String str);

	static const char * getFontForHeight(unsigned char height);
  static const char * getFontForSize( String str, unsigned char width, unsigned char height);
};
#endif // LAYOUT_H_
