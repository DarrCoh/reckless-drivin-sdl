#include "compat.h"
#include "screen.h"
#include "trig.h"
#include "input.h"
#include "sprites.h"
#include "gameframe.h"
#include "preferences.h"
#include "random.h"
#include "platform.h"
#include <string.h>
#include <math.h>

#define kGameOverSprite 265
#define kFadeStart		1.4
#define kAnimDuration	2.1
#define kShiftInDuration  0.8

enum{
	kSpinIn,
	kFlyIn,
	//kBlur,
	//kWobble,
	kNumGameOverTypes
};

extern UInt64 GetMSTime(void);

/*
 * ShiftInPicture - Venetian-blind transition effect.
 *
 * Even rows slide in from the right, odd rows from the left,
 * creating an interlaced sweep effect over kShiftInDuration seconds.
 * Faithfully reproduces the original Mac code's mathematical formula.
 */
void ShiftInPicture()
{
	PauseFrameCount();

	int rowBytes;
	UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
	int fbStride = rowBytes / 2;

	/* Save the incoming frame (the game has already rendered into the fb) */
	static UInt16 incoming[SCREEN_WIDTH * SCREEN_HEIGHT];
	memcpy(incoming, fb, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(UInt16));

	UInt64 startTime = GetMSTime();
	float elapsed;

	do {
		UInt64 now = GetMSTime();
		elapsed = (float)(now - startTime) / 1000000.0f;
		float per = elapsed / kShiftInDuration;
		if (per > 1.0f) per = 1.0f;

		/* Clear framebuffer to black */
		memset(fb, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(UInt16));

		for (int y = 0; y < SCREEN_HEIGHT; y++) {
			float relY = (float)y / (float)SCREEN_HEIGHT;
			float denom1 = relY + 1.0f;
			float denom2 = relY * relY + 2.0f * relY + 1.0f; /* (relY+1)^2 */
			float f = 4.0f / denom2 - 4.0f / denom1 - 1.0f;

			int offset;
			if (y & 1) {
				/* Odd rows: slide from left */
				offset = (int)(-f * (1.0f - per) * SCREEN_WIDTH);
			} else {
				/* Even rows: slide from right */
				offset = (int)(f * (1.0f - per) * SCREEN_WIDTH);
			}

			/* Copy the row from the incoming buffer with the offset */
			UInt16 *dstRow = fb + y * fbStride;
			UInt16 *srcRow = incoming + y * SCREEN_WIDTH;

			for (int x = 0; x < SCREEN_WIDTH; x++) {
				int sx = x - offset;
				if (sx >= 0 && sx < SCREEN_WIDTH)
					dstRow[x] = srcRow[sx];
			}
		}

		Platform_Blit2Screen();
		Platform_PollEvents();

		/* Allow skipping with any key */
		if (ContinuePress()) break;

	} while (elapsed < kShiftInDuration);

	/* Ensure the final frame is clean */
	memcpy(fb, incoming, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(UInt16));
	Platform_Blit2Screen();

	ResumeFrameCount();
}

void GameOverAnim()
{
	/*
	 * GameOverAnim - Shows "GAME OVER" sprite with animation.
	 * Originally rendered to the screen GWorld directly.
	 * For the SDL port, render to our offscreen buffer and blit.
	 */
	int type=RanInt(0,kNumGameOverTypes);
	int side=RanProb(0.5);
	UInt64 animStart;
	float t;

	animStart=GetMSTime();
	do{
		UInt64 msTime=GetMSTime();
		float size,xPos,yPos,dir;
		msTime-=animStart;
		t=msTime/1000000.0;
		switch(type)
		{
			case kSpinIn:
				size=t*t;
				xPos=gXSize/2;
				yPos=gYSize/2;
				dir=(side?1:-1)*2*PI*t;
				break;
			case kFlyIn:
				size=t*t;
				xPos=gXSize/2;
				if(side)
					yPos=(gYSize/2)*(4.0/3.0)*(1.0/(-t-1.0)+1.0);
				else
					yPos=gYSize-(gYSize/2)*(4.0/3.0)*(1.0/(-t-1.0)+1.0);
				dir=0;
				break;
			default:
				size=t*t;
				xPos=gXSize/2;
				yPos=gYSize/2;
				dir=0;
				break;
		}
		DrawSprite(kGameOverSprite,xPos,yPos,dir,size);
		if(t>kFadeStart)
			FadeScreen(256-(t-kFadeStart)/(kAnimDuration-kFadeStart)*256+256);
		Blit2Screen();
	}while(t<kAnimDuration);
	ScreenClear();
	FadeScreen(512);
}
