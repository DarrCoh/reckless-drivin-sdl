#ifndef __TEXTRENDER_H
#define __TEXTRENDER_H

/*
 * textrender.h - Shared 8x8 bitmap font rendering for Reckless Drivin' SDL port
 *
 * Provides text drawing directly to the ARGB1555 framebuffer.
 * Used by: preferences, high scores, credits, key config, level select.
 */

#include "compat.h"
#include "platform.h"

/* ARGB1555 color constants */
#define COL_BLACK   0x0000
#define COL_WHITE   0x7FFF
#define COL_YELLOW  0x7FE0
#define COL_CYAN    0x03FF
#define COL_RED     0x7C00
#define COL_GREEN   0x03E0
#define COL_GRAY    0x294A
#define COL_DKGRAY  0x18C6
#define COL_BAR_BG  0x2108
#define COL_BAR_FG  0x2D6B

/* Draw a single character at (x,y) with given color and pixel scale (1=8x8, 2=16x16, etc.) */
void TR_DrawChar(UInt16 *fb, int stride, int x, int y,
                 char ch, UInt16 color, int scale);

/* Draw a null-terminated C string left-aligned at (x,y) */
void TR_DrawString(UInt16 *fb, int stride, int x, int y,
                   const char *str, UInt16 color, int scale);

/* Draw a string right-aligned so its right edge is at rightX */
void TR_DrawStringRight(UInt16 *fb, int stride, int rightX, int y,
                        const char *str, UInt16 color, int scale);

/* Draw a string centered horizontally around centerX */
void TR_DrawStringCentered(UInt16 *fb, int stride, int centerX, int y,
                           const char *str, UInt16 color, int scale);

/* Compute the pixel width of a string at the given scale */
int TR_StringWidth(const char *str, int scale);

/* Fill a rectangle with a solid color */
void TR_FillRect(UInt16 *fb, int stride, int x, int y,
                 int w, int h, UInt16 color);

/* Convert a Pascal string (length-prefixed) to a C string (null-terminated) */
void TR_PStrToC(const unsigned char *pstr, char *cstr, int maxLen);

#endif /* __TEXTRENDER_H */
