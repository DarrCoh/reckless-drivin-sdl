#include "compat.h"
#include "preferences.h"
#include "interface.h"
#include "screen.h"
#include "platform.h"
#include "textrender.h"
#include "error.h"
#include <stdio.h>
#include <string.h>

tPrefs gPrefs;

static const char *GetPrefsPath(void)
{
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/.recklessdrivin_prefs", home);
    return path;
}

static void SetDefaults(void)
{
    memset(&gPrefs, 0, sizeof(tPrefs));
    gPrefs.version = kPrefsVersion;
    gPrefs.volume = 200;
    gPrefs.sound = 1;
    gPrefs.engineSound = 1;
    gPrefs.hqSound = 0;
    gPrefs.hiColor = 1;     /* we only support 16-bit */
    gPrefs.lineSkip = 0;
    gPrefs.motionBlur = 0;

    /* Default key codes (SDL scancodes stored directly) */
    gPrefs.keyCodes[kForward]  = SDL_SCANCODE_UP;
    gPrefs.keyCodes[kBackward] = SDL_SCANCODE_DOWN;
    gPrefs.keyCodes[kLeft]     = SDL_SCANCODE_LEFT;
    gPrefs.keyCodes[kRight]    = SDL_SCANCODE_RIGHT;
    gPrefs.keyCodes[kKickdown] = SDL_SCANCODE_LSHIFT;
    gPrefs.keyCodes[kBrake]    = SDL_SCANCODE_SPACE;
    gPrefs.keyCodes[kFire]     = SDL_SCANCODE_Z;
    gPrefs.keyCodes[kMissile]  = SDL_SCANCODE_X;

    /* Lap records all set to 999.0 */
    {
        int i;
        for (i = 0; i < 10; i++)
            gPrefs.lapRecords[i] = 999.0f;
    }

    /* Registration: Name="Free", Code="B3FB09B1EB" (Pascal strings) */
    gPrefs.name[0] = 4;
    gPrefs.name[1] = 'F';
    gPrefs.name[2] = 'r';
    gPrefs.name[3] = 'e';
    gPrefs.name[4] = 'e';

    gPrefs.code[0] = 10;
    gPrefs.code[1] = 'B';
    gPrefs.code[2] = '3';
    gPrefs.code[3] = 'F';
    gPrefs.code[4] = 'B';
    gPrefs.code[5] = '0';
    gPrefs.code[6] = '9';
    gPrefs.code[7] = 'B';
    gPrefs.code[8] = '1';
    gPrefs.code[9] = 'E';
    gPrefs.code[10] = 'B';
}

void LoadPrefs(void)
{
    FILE *f;
    const char *path = GetPrefsPath();

    f = fopen(path, "rb");
    if (f)
    {
        size_t bytesRead = fread(&gPrefs, 1, sizeof(tPrefs), f);
        fclose(f);
        if (bytesRead == sizeof(tPrefs) && gPrefs.version == kPrefsVersion)
            return; /* loaded successfully */
    }

    /* File doesn't exist or is wrong size/version - use defaults */
    SetDefaults();
    WritePrefs(0);
}

void WritePrefs(int reset)
{
    FILE *f;
    const char *path = GetPrefsPath();

    if (reset)
        SetDefaults();

    f = fopen(path, "wb");
    if (f)
    {
        fwrite(&gPrefs, 1, sizeof(tPrefs), f);
        fclose(f);
    }
}

/* ======================================================================== */
/* Preferences UI                                                            */
/* ======================================================================== */

extern void ConfigureInput(void);

enum {
    kPrefSound = 0,
    kPrefEngineSound,
    kPrefHQSound,
    kPrefVolume,
    kPrefMotionBlur,
    kPrefControls,
    kPrefDone,
    kPrefCount
};

static const char *kPrefLabels[kPrefCount] = {
    "Sound",
    "Engine Sound",
    "HQ Sound",
    "Volume",
    "Motion Blur",
    "Controls",
    "Done"
};

void Preferences(void)
{
    int rowBytes;
    UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
    int fbStride = rowBytes / 2;
    int selected = 0;
    int done = 0;

    /* Work on a copy of settings so we can cancel */
    UInt8 snd        = gPrefs.sound;
    UInt8 engSnd     = gPrefs.engineSound;
    UInt8 hqSnd      = gPrefs.hqSound;
    UInt16 volume    = gPrefs.volume;
    UInt8 motionBlur = gPrefs.motionBlur;

    Platform_ShowCursor();
    SaveFlushEvents();

    while (!done) {
        Platform_PollEvents();
        if (Platform_ShouldQuit()) { done = 1; break; }

        /* --- Draw the preferences screen --- */
        TR_FillRect(fb, fbStride, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COL_DKGRAY);

        /* Title */
        TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 50, "PREFERENCES", COL_WHITE, 3);

        /* Menu items */
        int baseY = 120;
        int itemH = 36;
        int labelX = 140;
        int valueX = 400;

        for (int i = 0; i < kPrefCount; i++) {
            int y = baseY + i * itemH;
            UInt16 labelCol = (i == selected) ? COL_YELLOW : COL_WHITE;

            /* Highlight bar for selected item */
            if (i == selected)
                TR_FillRect(fb, fbStride, 120, y - 4, 400, 28, COL_GRAY);

            if (i == kPrefDone) {
                /* Center the "Done" label */
                TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, y, "[ Done ]", labelCol, 2);
            } else if (i == kPrefControls) {
                TR_DrawString(fb, fbStride, labelX, y, kPrefLabels[i], labelCol, 2);
                TR_DrawString(fb, fbStride, valueX, y, "Configure...",
                              (i == selected) ? COL_GREEN : COL_GRAY, 2);
            } else {
                TR_DrawString(fb, fbStride, labelX, y, kPrefLabels[i], labelCol, 2);

                /* Draw value */
                const char *val = NULL;
                switch (i) {
                    case kPrefSound:       val = snd ? "ON" : "OFF"; break;
                    case kPrefEngineSound: val = engSnd ? "ON" : "OFF"; break;
                    case kPrefHQSound:     val = hqSnd ? "ON" : "OFF"; break;
                    case kPrefMotionBlur:  val = motionBlur ? "ON" : "OFF"; break;
                    case kPrefVolume: {
                        /* Volume bar */
                        int barX = valueX;
                        int barW = 120;
                        int barH = 16;
                        int filled = (volume * barW) / 256;
                        TR_FillRect(fb, fbStride, barX, y + 4, barW, barH, COL_BAR_BG);
                        TR_FillRect(fb, fbStride, barX, y + 4, filled, barH, COL_BAR_FG);
                        /* Numeric value */
                        char numBuf[8];
                        snprintf(numBuf, sizeof(numBuf), "%d", volume);
                        TR_DrawString(fb, fbStride, barX + barW + 16, y, numBuf, labelCol, 2);
                        break;
                    }
                }
                if (val)
                    TR_DrawString(fb, fbStride, valueX, y, val,
                                  (i == selected) ? COL_GREEN : COL_WHITE, 2);
            }
        }

        /* Instructions */
        TR_DrawString(fb, fbStride, 130, 390, "Up/Down: Select", COL_GRAY, 2);
        TR_DrawString(fb, fbStride, 130, 416, "Left/Right: Change   Enter: Done", COL_GRAY, 2);

        Platform_Blit2Screen();

        /* --- Handle input --- */
        if (Platform_IsKeyDown(SDL_SCANCODE_UP)) {
            selected = (selected + kPrefCount - 1) % kPrefCount;
            SDL_Delay(150);
        }
        if (Platform_IsKeyDown(SDL_SCANCODE_DOWN)) {
            selected = (selected + 1) % kPrefCount;
            SDL_Delay(150);
        }

        int adjust = 0;
        if (Platform_IsKeyDown(SDL_SCANCODE_LEFT))  { adjust = -1; SDL_Delay(80); }
        if (Platform_IsKeyDown(SDL_SCANCODE_RIGHT)) { adjust =  1; SDL_Delay(80); }
        if (Platform_IsKeyDown(SDL_SCANCODE_RETURN) ||
            Platform_IsKeyDown(SDL_SCANCODE_SPACE)) {
            if (selected == kPrefDone) { done = 1; break; }
            if (selected == kPrefControls) {
                /* Save current settings before entering controls */
                gPrefs.sound = snd;
                gPrefs.engineSound = engSnd;
                gPrefs.hqSound = hqSnd;
                gPrefs.volume = volume;
                gPrefs.motionBlur = motionBlur;
                ConfigureInput();
                SaveFlushEvents();
                SDL_Delay(200);
                continue;
            }
            adjust = 1;
            SDL_Delay(150);
        }
        if (Platform_IsKeyDown(SDL_SCANCODE_ESCAPE)) {
            done = 1; break;
        }

        /* Mouse click */
        {
            int mx, my;
            if (Platform_GetMouseClick(&mx, &my)) {
                /* Check if click is on a menu item */
                for (int i = 0; i < kPrefCount; i++) {
                    int iy = baseY + i * itemH - 4;
                    if (mx >= 120 && mx < 520 && my >= iy && my < iy + 28) {
                        selected = i;
                        if (i == kPrefDone) { done = 1; break; }
                        if (i == kPrefControls) {
                            gPrefs.sound = snd;
                            gPrefs.engineSound = engSnd;
                            gPrefs.hqSound = hqSnd;
                            gPrefs.volume = volume;
                            gPrefs.motionBlur = motionBlur;
                            ConfigureInput();
                            SaveFlushEvents();
                            continue;
                        }
                        adjust = 1;
                        break;
                    }
                }
            }
        }

        if (adjust && !done) {
            switch (selected) {
                case kPrefSound:       snd = !snd; break;
                case kPrefEngineSound: engSnd = !engSnd; break;
                case kPrefHQSound:     hqSnd = !hqSnd; break;
                case kPrefMotionBlur:  motionBlur = !motionBlur; break;
                case kPrefVolume: {
                    int v = (int)volume + adjust * 10;
                    if (v < 0) v = 0;
                    if (v > 255) v = 255;
                    volume = (UInt16)v;
                    break;
                }
            }
        }

        SDL_Delay(16);
    }

    /* Save settings */
    gPrefs.sound = snd;
    gPrefs.engineSound = engSnd;
    gPrefs.hqSound = hqSnd;
    gPrefs.volume = volume;
    gPrefs.motionBlur = motionBlur;
    WritePrefs(0);

    /* Return to menu */
    ShowPicScreen(1000);
    SaveFlushEvents();
}
