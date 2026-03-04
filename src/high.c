#include "compat.h"
#include <stdio.h>
#include <string.h>
#include "preferences.h"
#include "interface.h"
#include "screen.h"
#include "error.h"
#include "gamesounds.h"
#include "platform.h"
#include "textrender.h"

void ShowHighScores(int hilite)
{
	int i, rowBytes;
	UInt16 *fb;
	int fbStride;

	/* Show the high scores background picture */
	ShowPicScreen(1004);

	fb = Platform_GetFramebuffer(&rowBytes);
	fbStride = rowBytes / 2;

	/* Title */
	TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 120, "HIGH SCORES", COL_WHITE, 3);

	/* Draw each score entry */
	for (i = 0; i < kNumHighScoreEntrys; i++)
	{
		int y = 175 + i * 28;
		UInt16 color = (i == hilite) ? COL_YELLOW : COL_CYAN;
		char name[17];
		char scoreBuf[16];
		char rankBuf[8];

		/* Rank number */
		snprintf(rankBuf, sizeof(rankBuf), "%2d.", i + 1);
		TR_DrawString(fb, fbStride, 130, y, rankBuf, color, 2);

		/* Name (convert Pascal string to C string) */
		TR_PStrToC(gPrefs.high[i].name, name, sizeof(name));
		if (name[0] == '\0')
			strcpy(name, "---");
		TR_DrawString(fb, fbStride, 185, y, name, color, 2);

		/* Score (right-aligned) */
		snprintf(scoreBuf, sizeof(scoreBuf), "%u", (unsigned)gPrefs.high[i].score);
		TR_DrawStringRight(fb, fbStride, 510, y, scoreBuf, color, 2);

		/* Faux bold for highlighted entry: draw again offset by 1px */
		if (i == hilite) {
			TR_DrawString(fb, fbStride, 131, y, rankBuf, color, 2);
			TR_DrawString(fb, fbStride, 186, y, name, color, 2);
			TR_DrawStringRight(fb, fbStride, 511, y, scoreBuf, color, 2);
		}
	}

	Platform_Blit2Screen();
	WaitForPress();
	FadeScreen(1);
	ScreenUpdate();
	FadeScreen(0);
}

extern int gOSX;

void SetHighScoreEntry(int index, UInt32 score)
{
	int rowBytes;
	UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
	int fbStride = rowBytes / 2;
	char inputBuf[16];
	int inputLen = 0;
	int done = 0;

	/* Pre-fill with last name if available */
	TR_PStrToC(gPrefs.lastName, inputBuf, sizeof(inputBuf));
	inputLen = (int)strlen(inputBuf);
	if (inputLen == 0) {
		strcpy(inputBuf, "Player");
		inputLen = 6;
	}

	Platform_ShowCursor();
	Platform_FlushInput();
	SDL_StartTextInput();

	while (!done) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
			case SDL_QUIT:
				done = 1;
				break;
			case SDL_KEYDOWN:
				if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) {
					done = 1;
				} else if (ev.key.keysym.sym == SDLK_ESCAPE) {
					done = 1;
				} else if (ev.key.keysym.sym == SDLK_BACKSPACE && inputLen > 0) {
					inputLen--;
					inputBuf[inputLen] = '\0';
				}
				break;
			case SDL_TEXTINPUT:
				/* Append printable characters (max 15 for Str15) */
				for (int ci = 0; ev.text.text[ci] && inputLen < 15; ci++) {
					char c = ev.text.text[ci];
					if (c >= 32 && c <= 126) {
						inputBuf[inputLen++] = c;
						inputBuf[inputLen] = '\0';
					}
				}
				break;
			}
		}

		/* Draw the name entry screen */
		ShowPicScreen(1004);
		fb = Platform_GetFramebuffer(&rowBytes);
		fbStride = rowBytes / 2;

		TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 140, "NEW HIGH SCORE!", COL_YELLOW, 3);

		char scoreBuf[32];
		snprintf(scoreBuf, sizeof(scoreBuf), "Score: %u", (unsigned)score);
		TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 190, scoreBuf, COL_WHITE, 2);

		TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 240, "Enter your name:", COL_WHITE, 2);

		/* Input field background */
		int fieldX = 180;
		int fieldW = 280;
		int fieldY = 275;
		TR_FillRect(fb, fbStride, fieldX, fieldY, fieldW, 28, COL_GRAY);

		/* Draw the input text */
		TR_DrawString(fb, fbStride, fieldX + 8, fieldY + 6, inputBuf, COL_WHITE, 2);

		/* Blinking cursor */
		{
			UInt64 ms = Platform_GetMicroseconds() / 1000;
			if ((ms / 500) % 2 == 0) {
				int cx = fieldX + 8 + TR_StringWidth(inputBuf, 2);
				TR_FillRect(fb, fbStride, cx, fieldY + 6, 2, 16, COL_WHITE);
			}
		}

		TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 330, "Press ENTER to confirm", COL_GRAY, 2);

		Platform_Blit2Screen();
		SDL_Delay(16);
	}

	SDL_StopTextInput();
	Platform_HideCursor();

	/* Ensure we have a valid name */
	if (inputLen == 0) {
		strcpy(inputBuf, "Player");
		inputLen = 6;
	}

	/* Store as Pascal string */
	gPrefs.high[index].name[0] = (unsigned char)inputLen;
	memcpy(gPrefs.high[index].name + 1, inputBuf, inputLen);
	gPrefs.high[index].score = score;
	gPrefs.high[index].time = 0;

	/* Save the name for next time */
	gPrefs.lastName[0] = (unsigned char)inputLen;
	memcpy(gPrefs.lastName + 1, inputBuf, inputLen);
}

void CheckHighScore(UInt32 score)
{
	int i;
	if(gLevelResFile)return;
	for(i=kNumHighScoreEntrys;score>gPrefs.high[i-1].score&&i>0;i--);
	if(i<kNumHighScoreEntrys)
	{
		BlockMoveData(gPrefs.high+i,gPrefs.high+i+1,sizeof(tScoreRecord)*(kNumHighScoreEntrys-i-1));
		SimplePlaySound(153);
		SetHighScoreEntry(i,score);
		WritePrefs(false);
		ShowHighScores(i);
	}
}

void ClearHighScores()
{
	/* Original code loaded default high scores from a resource.
	   For the SDL port, just zero out the high score table. */
	int i;
	for(i=0;i<kNumHighScoreEntrys;i++)
	{
		gPrefs.high[i].name[0] = 0;
		gPrefs.high[i].score = 0;
		gPrefs.high[i].time = 0;
	}
	WritePrefs(false);
}
