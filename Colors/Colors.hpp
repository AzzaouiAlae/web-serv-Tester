#pragma once
#include <string>

// ─── ANSI escape codes ────────────────────────────────────────────────────────
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"

// Foreground colours
#define FG_BLACK    "\033[30m"
#define FG_RED      "\033[31m"
#define FG_GREEN    "\033[32m"
#define FG_YELLOW   "\033[33m"
#define FG_BLUE     "\033[34m"
#define FG_MAGENTA  "\033[35m"
#define FG_CYAN     "\033[36m"
#define FG_WHITE    "\033[37m"

// Bright foreground colours
#define FG_BRED     "\033[91m"
#define FG_BGREEN   "\033[92m"
#define FG_BYELLOW  "\033[93m"
#define FG_BBLUE    "\033[94m"
#define FG_BMAGENTA "\033[95m"
#define FG_BCYAN    "\033[96m"
#define FG_BWHITE   "\033[97m"

// Background colours
#define BG_RED      "\033[41m"
#define BG_GREEN    "\033[42m"
#define BG_YELLOW   "\033[43m"
#define BG_BLUE     "\033[44m"
#define BG_MAGENTA  "\033[45m"
#define BG_CYAN     "\033[46m"

// ─── Semantic aliases (use these in test code) ────────────────────────────────
#define CLR_PASS      BOLD FG_BGREEN    // bright green  – test passed
#define CLR_FAIL      BOLD FG_BRED      // bright red    – test failed
#define CLR_ERROR     BOLD FG_RED       // red           – runtime error
#define CLR_WARN      BOLD FG_BYELLOW   // yellow        – warning / note
#define CLR_TITLE     BOLD FG_BCYAN     // cyan          – section titles
#define CLR_NAME      BOLD FG_BWHITE    // white bold    – test name
#define CLR_DESC      FG_WHITE          // white         – description text
#define CLR_HEADER    FG_CYAN           // cyan          – HTTP response header
#define CLR_DIVIDER   FG_BLUE           // blue          – separator lines
#define CLR_PROMPT    BOLD FG_BYELLOW   // yellow bold   – user prompts
#define CLR_MENU_CAT  BOLD FG_BMAGENTA  // magenta bold  – menu category labels
#define CLR_MENU_OPT  FG_BWHITE        // white bright  – menu option text
#define CLR_MENU_NUM  FG_BYELLOW        // yellow        – menu numbers