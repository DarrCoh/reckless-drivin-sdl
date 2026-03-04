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
	PauseFrameCount();
	SaveFlushEvents();
	InputMode(kInputSuspended);
	BeQuiet();
	ShowPicScreen(1006);
	Platform_ShowCursor();

	while(!end)
	{
		Platform_PollEvents();
		if(Platform_ShouldQuit())
		{
			end=true;
			break;
		}
		if(Platform_ContinuePress()||
		   Platform_IsKeyDown(SDL_SCANCODE_SPACE)||
		   Platform_IsKeyDown(SDL_SCANCODE_ESCAPE)||
		   Platform_GetMouseClick(NULL,NULL))
			end=true;
		SDL_Delay(16);
	}

	Platform_HideCursor();
	InputMode(kInputRunning);
	ScreenClear();
	StartCarChannels();
	ResumeFrameCount();
}
