#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

// ─── ANSI escape codes ────────────────────────────────────────────────────────
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define ITALIC      "\033[3m"
#define UNDERLINE   "\033[4m"

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
#define CLR_PASS      BOLD FG_BGREEN
#define CLR_FAIL      BOLD FG_BRED
#define CLR_ERROR     BOLD FG_RED
#define CLR_WARN      BOLD FG_BYELLOW
#define CLR_TITLE     BOLD FG_BCYAN
#define CLR_NAME      BOLD FG_BWHITE
#define CLR_DESC      FG_WHITE
#define CLR_HEADER    FG_CYAN
#define CLR_DIVIDER   FG_BLUE
#define CLR_PROMPT    BOLD FG_BYELLOW
#define CLR_MENU_CAT  BOLD FG_BMAGENTA
#define CLR_MENU_OPT  FG_BWHITE
#define CLR_MENU_NUM  FG_BYELLOW
#define CLR_DIM       DIM FG_WHITE
#define CLR_STEP      BOLD FG_BBLUE
#define CLR_INFO      FG_BCYAN

// ─── Unicode box-drawing & symbols ───────────────────────────────────────────
#define BOX_TL  "\u250C"
#define BOX_TR  "\u2510"
#define BOX_BL  "\u2514"
#define BOX_BR  "\u2518"
#define BOX_H   "\u2500"
#define BOX_V   "\u2502"
#define BOX_ML  "\u251C"
#define BOX_MR  "\u2524"
#define BOX_DH  "\u2550"
#define BOX_DTL "\u2554"
#define BOX_DTR "\u2557"
#define BOX_DBL "\u255A"
#define BOX_DBR "\u255D"
#define BOX_DV  "\u2551"

#define SYM_CHECK  "\u2714"
#define SYM_CROSS  "\u2718"
#define SYM_ARROW  "\u25B6"
#define SYM_DOT    "\u2022"
#define SYM_STAR   "\u2605"
#define SYM_WARN   "\u26A0"
#define SYM_GEAR   "\u2699"
#define SYM_RAQUO  "\u00BB"
#define SYM_LIGHT  "\u26A1"

// ─── CLI formatting helpers ──────────────────────────────────────────────────
namespace CLI
{
	inline std::string repeat(const std::string &s, int n)
	{
		std::string result;
		for (int i = 0; i < n; i++) result += s;
		return result;
	}

	// Measure visible character width (skip ANSI escapes, count UTF-8 codepoints)
	inline size_t visibleLen(const std::string &s)
	{
		size_t len = 0;
		bool inEsc = false;
		for (size_t i = 0; i < s.size(); i++)
		{
			unsigned char c = (unsigned char)s[i];
			if (c == '\033') { inEsc = true; continue; }
			if (inEsc) { if (c == 'm') inEsc = false; continue; }
			if (c < 0x80 || c >= 0xC0) len++;
		}
		return len;
	}

	static const int W = 56;

	// ── Line builders ────────────────────────────────────────────────────────
	inline std::string topLine()
	{
		return std::string(CLR_DIVIDER) + BOX_TL + repeat(BOX_H, W) + BOX_TR + RESET;
	}
	inline std::string midLine()
	{
		return std::string(CLR_DIVIDER) + BOX_ML + repeat(BOX_H, W) + BOX_MR + RESET;
	}
	inline std::string botLine()
	{
		return std::string(CLR_DIVIDER) + BOX_BL + repeat(BOX_H, W) + BOX_BR + RESET;
	}
	inline std::string dblTopLine()
	{
		return std::string(CLR_DIVIDER) + BOX_DTL + repeat(BOX_DH, W) + BOX_DTR + RESET;
	}
	inline std::string dblBotLine()
	{
		return std::string(CLR_DIVIDER) + BOX_DBL + repeat(BOX_DH, W) + BOX_DBR + RESET;
	}
	inline std::string dblRow(const std::string &content)
	{
		int avail = W - 1;
		int vl = (int)visibleLen(content);
		std::string pad;
		if (vl < avail)
			pad = std::string(avail - vl, ' ');
		return std::string(CLR_DIVIDER) + BOX_DV + " " + RESET + content + pad
		     + std::string(CLR_DIVIDER) + BOX_DV + RESET;
	}
	inline std::string row(const std::string &content)
	{
		int avail = W - 1;
		int vl = (int)visibleLen(content);
		std::string pad;
		if (vl < avail)
			pad = std::string(avail - vl, ' ');
		return std::string(CLR_DIVIDER) + BOX_V + " " + RESET + content + pad
		     + std::string(CLR_DIVIDER) + BOX_V + RESET;
	}
	inline std::string emptyRow()
	{
		return std::string(CLR_DIVIDER) + BOX_V + repeat(" ", W) + BOX_V + RESET;
	}

	// ── Badges ───────────────────────────────────────────────────────────────
	inline std::string passBadge()
	{
		return std::string(BOLD) + BG_GREEN + FG_BLACK + " " + SYM_CHECK + " PASS " + RESET;
	}
	inline std::string failBadge()
	{
		return std::string(BOLD) + BG_RED + FG_BWHITE + " " + SYM_CROSS + " FAIL " + RESET;
	}
	inline std::string errorBadge()
	{
		return std::string(BOLD) + BG_RED + FG_BWHITE + " " + SYM_WARN + " ERROR " + RESET;
	}
	inline std::string runBadge()
	{
		return std::string(BOLD) + BG_BLUE + FG_BWHITE + " " + SYM_GEAR + " RUN " + RESET;
	}
	inline std::string infoBadge()
	{
		return std::string(BOLD) + BG_CYAN + FG_BLACK + " " + SYM_ARROW + " INFO " + RESET;
	}

	// ── Progress bar ─────────────────────────────────────────────────────────
	inline std::string progressBar(int passed, int total, int width = 30)
	{
		if (total == 0) return "";
		int filled = (passed * width) / total;
		int empty  = width - filled;
		std::string bar;
		bar += std::string(CLR_PASS) + repeat("\u2588", filled) + RESET;
		bar += std::string(CLR_FAIL) + repeat("\u2588", empty) + RESET;
		return bar;
	}

	// ── Separator ────────────────────────────────────────────────────────────
	inline std::string separator()
	{
		return std::string(CLR_DIVIDER) + repeat(BOX_H, W + 2) + RESET;
	}
	inline std::string thickSeparator()
	{
		return std::string(CLR_DIVIDER) + repeat(BOX_DH, W + 2) + RESET;
	}

	// ── Banner ───────────────────────────────────────────────────────────────
	inline void printBanner()
	{
		std::cout << "\n";
		std::cout << dblTopLine() << std::endl;
		std::cout << dblRow(std::string(CLR_TITLE) + "   " + SYM_LIGHT + "  enginX " + RESET + CLR_DIM + "- Web Server Tester  " + SYM_LIGHT + RESET) << std::endl;
		std::cout << dblBotLine() << std::endl;
	}

	// ── Word-wrap plain text into box rows ────────────────────────────────────
	inline void printWrapped(std::ostream &out, const std::string &text,
	                         const std::string &color)
	{
		int avail = W - 2;
		std::istringstream iss(text);
		std::string word;
		std::string line;
		int lineLen = 0;

		while (iss >> word)
		{
			int wlen = (int)word.size();
			if (lineLen > 0 && lineLen + 1 + wlen > avail)
			{
				out << row(color + line + RESET) << std::endl;
				line = word;
				lineLen = wlen;
			}
			else
			{
				if (lineLen > 0) { line += " "; lineLen++; }
				line += word;
				lineLen += wlen;
			}
		}
		if (!line.empty())
			out << row(color + line + RESET) << std::endl;
	}

	// ── Print multi-line text (\r\n separated) in box rows ───────────────────
	inline void printLines(std::ostream &out, const std::string &text,
	                       const std::string &color)
	{
		std::string line;
		for (size_t i = 0; i < text.size(); i++)
		{
			if (text[i] == '\r') continue;
			if (text[i] == '\n')
			{
				if (!line.empty())
				{
					if ((int)line.size() > W - 2)
						printWrapped(out, line, color);
					else
						out << row(color + line + RESET) << std::endl;
				}
				line.clear();
			}
			else
				line += text[i];
		}
		if (!line.empty())
		{
			if ((int)line.size() > W - 2)
				printWrapped(out, line, color);
			else
				out << row(color + line + RESET) << std::endl;
		}
	}

	// ── Error line ───────────────────────────────────────────────────────────
	inline void printError(const std::string &msg)
	{
		std::cerr << "  " << errorBadge() << "  " << CLR_ERROR << msg << RESET << std::endl;
	}

	// ── Info line ────────────────────────────────────────────────────────────
	inline void printInfo(const std::string &msg)
	{
		std::cout << "  " << infoBadge() << "  " << CLR_INFO << msg << RESET << std::endl;
	}

	// ── Diagnosis / hint line ────────────────────────────────────────────────
	inline void printHint(const std::string &msg)
	{
		std::cerr << "  " << CLR_WARN << SYM_ARROW << " " << RESET << CLR_DIM << msg << RESET << std::endl;
	}
}
