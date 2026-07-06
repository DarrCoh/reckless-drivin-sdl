#include "compat.h"
#include "gameframe.h"
#include "interface.h"
#include "input.h"
#include "error.h"
#include "gamesounds.h"
#include "screenfx.h"
#include "screen.h"
#include "platform.h"

void PauseGame()
{
	int end=false;
	int prevContinue;
	PauseFrameCount();
	SaveFlushEvents();
	InputMode(kInputSuspended);
	BeQuiet();
	ShowPicScreen(1006);
	Platform_ShowCursor();
	prevContinue=Platform_ContinuePress();

	while(!end)
	{
		Platform_PollEvents();
		if(Platform_ShouldQuit())
		{
			end=true;
			break;
		}
		/* Resume on a fresh press only, so keys or buttons still held
		 * from gameplay cannot dismiss the pause screen instantly. */
		if(Platform_WasKeyPressed(SDL_SCANCODE_SPACE)||
		   Platform_WasKeyPressed(SDL_SCANCODE_ESCAPE)||
		   Platform_GetMouseClick(NULL,NULL))
			end=true;
		{
			int cont=Platform_ContinuePress();
			if(cont&&!prevContinue)
				end=true;
			prevContinue=cont;
		}
		Platform_Blit2Screen();
		SDL_Delay(16);
	}

	Platform_HideCursor();
	/* Drop whatever dismissed the pause screen (e.g. a held Escape) so it
	 * does not surface as an abort event once input resumes. */
	Platform_FlushInput();
	InputMode(kInputRunning);
	ScreenClear();
	StartCarChannels();
	ResumeFrameCount();
}
