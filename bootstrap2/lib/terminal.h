#ifndef LIB_TERMINAL_H
#define LIB_TERMINAL_H

#define ANSI_PREFIX "\x1b["
#define ANSI_END "m"

#define ANSI_RESET "0"
#define ANSI_RESET_SEQUENCE ANSI_PREFIX ANSI_RESET ANSI_END

#define ANSI_BOLD      "1"
#define ANSI_FAINT     "2"
#define ANSI_ITALIC    "3"
#define ANSI_UNDERLINE "4"

#define ANSI_BOLD_RESET      "22"
#define ANSI_FAINT_RESET     "22"
#define ANSI_ITALIC_RESET    "23"
#define ANSI_UNDERLINE_RESET "24"

#define ANSI_BLACK_FG   "30"
#define ANSI_RED_FG     "31"
#define ANSI_GREEN_FG   "32"
#define ANSI_YELLO_FG   "33"
#define ANSI_BLUE_FG    "34"
#define ANSI_MAGENTA_FG "35"
#define ANSI_CYAN_FG    "36"
#define ANSI_WHITE_FG   "37"
#define ANSI_DEFAULT_FG "39"

#define ANSI_BLACK_BG   "40"
#define ANSI_RED_BG     "41"
#define ANSI_GREEN_BG   "42"
#define ANSI_YELLO_BG   "43"
#define ANSI_BLUE_BG    "44"
#define ANSI_MAGENTA_BG "45"
#define ANSI_CYAN_BG    "46"
#define ANSI_WHITE_BG   "47"
#define ANSI_DEFAULT_BG "49"

#define ANSI1(X, ...) X
#define ANSI2(X, ...) X __VA_OPT__(";" ANSI1(__VA_ARGS__))
#define ANSI3(X, ...) X __VA_OPT__(";" ANSI2(__VA_ARGS__))
#define ANSI4(X, ...) X __VA_OPT__(";" ANSI3(__VA_ARGS__))
#define ANSI5(X, ...) X __VA_OPT__(";" ANSI4(__VA_ARGS__))
#define ANSI6(X, ...) X __VA_OPT__(";" ANSI5(__VA_ARGS__))
#define ANSI7(X, ...) X __VA_OPT__(";" ANSI6(__VA_ARGS__))
#define ANSI8(X, ...) X __VA_OPT__(";" ANSI7(__VA_ARGS__))
#define ANSI(...) ANSI_PREFIX ANSI8(__VA_ARGS__) ANSI_END

#endif