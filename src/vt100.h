#ifndef _vt100_h
#define _vt100_h

// http://www.termsys.demon.co.uk/vtansi.htm 

#define VT_BRIGHT     "1"
#define VT_DIM        "2"
#define VT_UNDERSCORE "4"
#define VT_BLINK      "5"
#define VT_REVERSE    "7"
#define VT_HIDDEN     "8"

#define VT_BLACK      "30"
#define VT_RED        "31"
#define VT_GREEN      "32"
#define VT_YELLOW     "33"
#define VT_BLUE       "34"
#define VT_MAGENTA    "35"
#define VT_CYAN       "36"
#define VT_WHITE      "37"

#define VT_BG_BLACK   "40"
#define VT_BG_RED     "41"
#define VT_BG_GREEN   "42"
#define VT_BG_YELLOW  "43"
#define VT_BG_BLUE    "44"
#define VT_BG_MAGENTA "45"
#define VT_BG_CYAN    "46"
#define VT_BG_WHITE   "47"

#define VT_CURSOR_UP     "\e[A"
#define VT_CURSOR_DOWN   "\e[B"
#define VT_CURSOR_RIGHT  "\e[C"
#define VT_CURSOR_LEFT   "\e[D"
#define VT_CURSOR_SAVE   "\e[s"
#define VT_CURSOR_UNSAVE "\e[u"

#define VT_ERASE_RIGHT  "\e[K"
#define VT_ERASE_LEFT   "\e[1K"
#define VT_ERASE_LINE   "\e[2K"
#define VT_ERASE_BELOW  "\e[J"
#define VT_ERASE_ABOVE  "\e[1J"
#define VT_ERASE_SCREEN "\e[2J"

#define VT_RESET        "\e[0m"
#define VT_SET_COLOR    "\e[%dm"
#define VT_SET(s)       "\e[" s "m"
#define VT_SET2(s0, s1) "\e[" s0 ";" s1 "m"


#endif
