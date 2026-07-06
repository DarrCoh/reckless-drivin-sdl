#include "compat.h"
#include "gameframe.h"
#include "error.h"
#include "screen.h"
#include "input.h"
#include "gamesounds.h"
#include "platform.h"
#include "textrender.h"
#include <string.h>

/*
 * GameEndSequence - Scrolling credits screen shown after completing all levels.
 *
 * Original used a compressed PICT (PPic 1009) containing QuickDraw text
 * commands, which was rasterized into a GWorld and scrolled upward.
 * Since we can't render QuickDraw text PICTs, we use the bitmap font
 * to create a text-based credits scroll.
 */

typedef struct {
	const char *text;
	int scale;
	UInt16 color;
} CreditLine;

static const CreditLine kCredits[] = {
	{"RECKLESS DRIVIN'",  3, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"CONGRATULATIONS!",  3, COL_YELLOW},
	{"",                  2, COL_WHITE},
	{"You beat all the levels!", 2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"- Original Game -", 2, COL_CYAN},
	{"",                  1, COL_WHITE},
	{"Jonas Echterhoff",  2, COL_WHITE},
	{"(c) 2000",          2, COL_GRAY},
	{"",                  2, COL_WHITE},
	{"Released under MIT License, 2019", 2, COL_GRAY},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"- SDL2 Port -",     2, COL_CYAN},
	{"",                  1, COL_WHITE},
	{"Darren Cohen",      2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"- Reverse Engineering -", 2, COL_CYAN},
	{"",                  1, COL_WHITE},
	{"Nate Craddock",     2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"- LZRW3-A Compression -", 2, COL_CYAN},
	{"",                  1, COL_WHITE},
	{"Ross Williams",     2, COL_WHITE},
	{"(Public Domain)",   2, COL_GRAY},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"- Built with -",    2, COL_CYAN},
	{"",                  1, COL_WHITE},
	{"SDL2",              2, COL_WHITE},
	{"libsdl.org",        2, COL_GRAY},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"Thanks for playing!", 3, COL_YELLOW},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
	{"",                  2, COL_WHITE},
};

#define kNumCreditLines (sizeof(kCredits) / sizeof(kCredits[0]))
#define kScrollSpeed 35  /* pixels per second, matching original */

void GameEndSequence()
{
	PauseFrameCount();
	BeQuiet();
	FadeScreen(1);

	int rowBytes;
	UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
	int fbStride = rowBytes / 2;

	/* Compute total height of credits */
	int totalHeight = 0;
	for (int i = 0; i < (int)kNumCreditLines; i++)
		totalHeight += 8 * kCredits[i].scale + 4; /* line height + spacing */

	/* Add screen height so credits scroll fully off the top */
	int scrollMax = totalHeight + SCREEN_HEIGHT;

	UInt64 startTime = GetMSTime();
	int done = 0;

	FadeScreen(0); /* fade from black */

	while (!done) {
		Platform_PollEvents();
		if (Platform_ShouldQuit()) { done = 1; break; }

		/* Check for skip */
		if (ContinuePress() || Platform_GetMouseClick(NULL, NULL)) {
			done = 1;
			break;
		}

		UInt64 now = GetMSTime();
		float elapsed = (float)(now - startTime) / 1000000.0f;
		int scrollY = (int)(elapsed * kScrollSpeed);

		if (scrollY >= scrollMax) {
			done = 1;
			break;
		}

		/* Clear to black */
		memset(fb, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(UInt16));

		/* Draw each credit line at its scrolled position */
		int lineY = SCREEN_HEIGHT - scrollY; /* start below screen, scroll up */
		for (int i = 0; i < (int)kNumCreditLines; i++) {
			int lineH = 8 * kCredits[i].scale + 4;

			/* Only draw if visible */
			if (lineY + lineH > 0 && lineY < SCREEN_HEIGHT) {
				if (kCredits[i].text[0] != '\0') {
					TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2,
					                      lineY, kCredits[i].text,
					                      kCredits[i].color, kCredits[i].scale);
				}
			}
			lineY += lineH;
		}

		Platform_Blit2Screen();
		SDL_Delay(16);
	}

	FadeScreen(1);
	Platform_ScreenClear();
	FadeScreen(512);
	FlushInput();
	ResumeFrameCount();
}
