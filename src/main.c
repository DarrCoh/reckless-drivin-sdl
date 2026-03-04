/*
 * main.c - Entry point for Reckless Drivin' SDL port
 *
 * Replaces the original Mac OS Init/Exit and main loop with SDL2 equivalents.
 * The original main was:
 *   void main() {
 *       Init();
 *       while(!gExit)
 *           if(gGameOn) GameFrame();
 *           else Eventloop();
 *       Exit();
 *   }
 */

#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "compat.h"
#include "platform.h"
#include "resources.h"

/* Original game headers */
#include "trig.h"
#include "preferences.h"
#include "register.h"
#include "random.h"
#include "packs.h"
#include "sprites.h"
#include "objects.h"
#include "input.h"
#include "gamesounds.h"
#include "gameinitexit.h"
#include "gameframe.h"
#include "interface.h"
#include "screen.h"
#include "error.h"

/* ---- Global variables expected by the original game code ---- */

/* Sine lookup table (trig.h declares extern, we define here) */
float gSinTab[kSinTabSize];

/* Initialization state */
int gInitSuccessful = 0;

/* OS X flag - set to 0 since we are not on classic Mac OS X */
int gOSX = 0;

/* Exit flag - when set to nonzero the main loop terminates */
int gExit = 0;

/* Resource file references - unused in SDL port but declared by interface.h */
short gAppResFile = 0;
short gLevelResFile = 0;

/* ---- InitTrig: populate the sine table ---- */
/* We must #undef sin/cos since trig.h redefines them as table lookups */
#undef sin
#undef cos

static void InitTrig(void)
{
    int i;
    for (i = 0; i < kSinTabSize; i++)
        gSinTab[i] = (float)sin(2.0 * 3.14159265358979323846 * (double)i / (double)kSinTabSize);
}

/* ---- Find the Data file ---- */
static const char* FindDataFile(const char *argv0)
{
    static char path[4096];

    /* Strategy 1: look next to the executable */
    if (argv0) {
        const char *lastSlash = strrchr(argv0, '/');
        if (lastSlash) {
            size_t dirLen = (size_t)(lastSlash - argv0);
            if (dirLen + 6 < sizeof(path)) {
                memcpy(path, argv0, dirLen);
                path[dirLen] = '/';
                strcpy(path + dirLen + 1, "Data");
                FILE *f = fopen(path, "rb");
                if (f) {
                    fclose(f);
                    return path;
                }
            }
            /* Strategy 1b: macOS .app bundle — look in ../Resources/ relative to executable */
            if (dirLen + 20 < sizeof(path)) {
                memcpy(path, argv0, dirLen);
                strcpy(path + dirLen, "/../Resources/Data");
                FILE *f = fopen(path, "rb");
                if (f) {
                    fclose(f);
                    return path;
                }
            }
        }
    }

    /* Strategy 2: current working directory */
    {
        FILE *f = fopen("Data", "rb");
        if (f) {
            fclose(f);
            return "Data";
        }
    }

    /* Strategy 3: parent directory (common in development) */
    {
        FILE *f = fopen("../RecklessDrivin/Data", "rb");
        if (f) {
            fclose(f);
            return "../RecklessDrivin/Data";
        }
    }

    return NULL;
}

/* ---- Init: replaces the original Init() ---- */
static void Init(const char *argv0)
{
    const char *dataPath;

    /* Initialize SDL subsystems */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[init] SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    /* Find and load the resource Data file */
    dataPath = FindDataFile(argv0);
    if (!dataPath) {
        fprintf(stderr, "[init] Error: Could not find 'Data' file.\n");
        fprintf(stderr, "[init] Place it next to the executable or in the current directory.\n");
        return;
    }
    if (!Resources_Init(dataPath)) {
        fprintf(stderr, "[init] Error: Failed to load resources from '%s'.\n", dataPath);
        return;
    }
    fprintf(stderr, "[init] Loaded resources from: %s\n", dataPath);

    /* Initialize the SDL screen/window and set up framebuffer globals */
    Platform_InitScreen();
    {
        int rowBytes;
        UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
        extern Ptr gBaseAddr;
        extern short gRowBytes;
        gBaseAddr = (Ptr)fb;
        gRowBytes = (short)rowBytes;
    }

    /* We are not on classic Mac OS X */
    gOSX = 0;

    /* Load preferences from disk (or initialize with defaults if no file) */
    LoadPrefs();
    gPrefs.hiColor = 1;  /* Force 16-bit — the only mode we support */

    /* The game is now free/open source - set registered unconditionally */
    gRegistered = 1;
    gKey = 0x1E42A71F;

    /* Initialize game subsystems */
    Randomize();

    /* Show loading screen */
    {
        extern void ShowPicScreen(int id);
        ShowPicScreen(1003);
    }

    /* Load resource packs - sounds, object types, object groups, road data */
    fprintf(stderr, "[init] Loading packs...\n");
    LoadPack(kPackSnds);  fprintf(stderr, "[init]   kPackSnds OK\n");
    LoadPack(kPackObTy);  fprintf(stderr, "[init]   kPackObTy OK\n");
    SwapAllObjectTypes(); fprintf(stderr, "[init]   SwapAllObjectTypes OK\n");
    LoadPack(kPackOgrp);  fprintf(stderr, "[init]   kPackOgrp OK\n");
    SwapAllObjectGroups();fprintf(stderr, "[init]   SwapAllObjectGroups OK\n");
    LoadPack(kPackRoad);  fprintf(stderr, "[init]   kPackRoad OK\n");
    SwapAllRoadInfo();    fprintf(stderr, "[init]   SwapAllRoadInfo OK\n");

    /* Load 16-bit color packs (we only support hiColor mode) */
    LoadPack(kPacksR16);  fprintf(stderr, "[init]   kPacksR16 OK\n");
    LoadPack(kPackcR16);  fprintf(stderr, "[init]   kPackcR16 OK\n");
    LoadPack(kPackTx16);  fprintf(stderr, "[init]   kPackTx16 OK\n");

    /* Load sprites and initialize trig tables */
    fprintf(stderr, "[init] Loading sprites...\n");
    LoadSprites();        fprintf(stderr, "[init]   LoadSprites OK\n");
    InitTrig();           fprintf(stderr, "[init]   InitTrig OK\n");

    /* Initialize input and audio */
    fprintf(stderr, "[init] Initializing input/audio...\n");
    Platform_InitInput();
    Platform_LoadKeyBindings(gPrefs.keyCodes);
    Platform_InitAudio();

    /* Initialize the game's sound channels and volume */
    fprintf(stderr, "[init] Initializing sound channels...\n");
    InitChannels();
    SetGameVolume(-1);

    /* Set up the menu interface */
    fprintf(stderr, "[init] Initializing interface...\n");
    InitInterface();

    gInitSuccessful = true;
    fprintf(stderr, "[init] Reckless Drivin' initialized successfully.\n");
}

/* ---- Exit: replaces the original Exit() ---- */
static void GameExit(void)
{
    if (gInitSuccessful) {
        /* Save preferences */
        WritePrefs(false);

        /* Clean up game resources */
        DisposeInterface();
        UnloadSprites();

        /* Shut down platform subsystems */
        Platform_ShutdownAudio();
        Platform_ShutdownInput();
        Platform_ShutdownScreen();
    }

    /* Clean up resource system */
    Resources_Shutdown();

    /* Quit SDL */
    SDL_Quit();

    fprintf(stderr, "[init] Reckless Drivin' shut down.\n");
}

/* ---- Main entry point ---- */
int main(int argc, char *argv[])
{
    (void)argc; /* unused */

    Init(argv[0]);

    if (!gInitSuccessful) {
        fprintf(stderr, "[init] Initialization failed. Exiting.\n");
        GameExit();
        return 1;
    }

    /* Main game loop - mirrors the original:
     *   while(!gExit)
     *       if(gGameOn) GameFrame();
     *       else Eventloop();
     */
    fprintf(stderr, "[init] Entering main loop...\n");

    while (!gExit) {
        /* Poll SDL events (keyboard, window close, etc.) */
        Platform_PollEvents();

        /* Check if the user closed the window */
        if (Platform_ShouldQuit()) {
            gExit = 1;
            break;
        }

        if (gGameOn)
            GameFrame();
        else {
            Eventloop();
            SDL_Delay(16); /* ~60fps polling when in menu */
        }
    }

    GameExit();
    return 0;
}
