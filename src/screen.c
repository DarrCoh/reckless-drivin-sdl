/*
 * screen.c - Screen management for Reckless Drivin' SDL port
 *
 * Replaces the original DrawSprocket-based screen management with calls
 * to the SDL2 platform abstraction layer (platform_sdl.c).
 *
 * The original code used DSpContext for page-flipping, gamma fading,
 * CLUT management, and GWorld-based blitting. All of that is now handled
 * by the platform layer which maintains a 640x480 ARGB1555 framebuffer
 * and uploads it to an SDL streaming texture each frame.
 */

#include "compat.h"
#include "screen.h"
#include "platform.h"
#include "preferences.h"
#include "resources.h"

/* ======================================================================== */
/* Globals                                                                   */
/* ======================================================================== */

Ptr    gBaseAddr;
short  gRowBytes;
short  gXSize = 640, gYSize = 480;
int    gOddLines;
Handle gTranslucenceTab = nil, g16BitClut = nil;
UInt8  gLightningTab[kLightValues][256];
int    gScreenMode;
int    gScreenBlitSpecial = false;

/* ======================================================================== */
/* Screen initialisation                                                     */
/* ======================================================================== */

void InitScreen(void)
{
    int rowBytes;

    Platform_InitScreen();

    UInt16 *buffer = Platform_GetFramebuffer(&rowBytes);
    gBaseAddr = (Ptr)buffer;
    gRowBytes = (short)rowBytes;
    gScreenMode = kScreenSuspended;
}

/* ======================================================================== */
/* Screen mode switching                                                     */
/* ======================================================================== */

void ScreenMode(int mode)
{
    fprintf(stderr, "[screen] ScreenMode(%d), current=%d\n", mode, gScreenMode);
    switch (mode) {
    case kScreenRunning: {
        int rowBytes;
        UInt16 *buffer = Platform_GetFramebuffer(&rowBytes);
        gBaseAddr = (Ptr)buffer;
        gRowBytes = (short)rowBytes;
        if (gScreenMode == kScreenSuspended)
            SetScreenClut(8);
        break;
    }
    case kScreenPaused:
        if (gScreenMode == kScreenSuspended)
            SetScreenClut(8);
        break;
    case kScreenSuspended:
        break;
    case kScreenStopped:
        Platform_ShutdownScreen();
        break;
    }
    gScreenMode = mode;
}

/* ======================================================================== */
/* Fade                                                                      */
/* ======================================================================== */

void FadeScreen(int out)
{
    Platform_FadeScreen(out);
}

/* ======================================================================== */
/* Blit to screen                                                            */
/* ======================================================================== */

void Blit2Screen(void)
{
    if (gScreenBlitSpecial) {
        gScreenBlitSpecial = false;
        /* ShiftInPicture() is in screenfx.c -- it will call Blit2Screen
         * again internally, so we just invoke it and return. */
        extern void ShiftInPicture(void);
        ShiftInPicture();
    } else {
        Platform_Blit2Screen();
    }
}

/* ======================================================================== */
/* CLUT / colour table loading                                               */
/* ======================================================================== */

/* Synthetic 16-bit CLUT for when the Cl16 resource is not available.
 * Maps 256 8-bit palette indices to ARGB1555 colors. */
static UInt16 sSyntheticClut16Data[256];
static char *sSyntheticClut16Ptr = (char*)sSyntheticClut16Data;

static void GenerateSyntheticClut16(void)
{
    /* Build a reasonable 256-color palette in ARGB1555 format.
     * Indices 0-215: 6x6x6 color cube (web-safe colors)
     * Indices 216-255: grayscale ramp */
    int i;
    for (i = 0; i < 216; i++) {
        int r = (i / 36) * 51;      /* 0,51,102,153,204,255 */
        int g = ((i / 6) % 6) * 51;
        int b = (i % 6) * 51;
        sSyntheticClut16Data[i] = (UInt16)(0x8000 |
            ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
    }
    for (i = 216; i < 256; i++) {
        int gray = (i - 216) * 255 / 39;
        int g5 = gray >> 3;
        sSyntheticClut16Data[i] = (UInt16)(0x8000 |
            (g5 << 10) | (g5 << 5) | g5);
    }
}

void SetScreenClut(int id)
{
    if (!gPrefs.hiColor) {
        /* 8-bit mode -- not used in SDL port, stub only */
        if (gTranslucenceTab) {
            Resources_Release(gTranslucenceTab);
            gTranslucenceTab = nil;
        }
        gTranslucenceTab = Resources_Get(MakeFourCC("Trtb"), id);
    } else {
        if (g16BitClut) {
            Resources_Release(g16BitClut);
            g16BitClut = nil;
        }
        g16BitClut = Resources_Get(MakeFourCC("Cl16"), id);
        if (!g16BitClut) {
            /* Resource not found - use synthetic palette */
            GenerateSyntheticClut16();
            g16BitClut = (Handle)&sSyntheticClut16Ptr;
        }
    }
}

/* ======================================================================== */
/* Screen clear                                                              */
/* ======================================================================== */

void ScreenClear(void)
{
    Platform_ScreenClear();
}

/* ======================================================================== */
/* Message buffer stubs                                                      */
/* These functions are called from gameframe.c but served a debug overlay    */
/* purpose on the original Mac build.  They are no-ops in the SDL port.      */
/* ======================================================================== */

void MakeDecString(int value, StringPtr str)
{
    (void)value;
    (void)str;
}

void AddFloatToMessageBuffer(StringPtr label, float value)
{
    (void)label;
    (void)value;
}

void FlushMessageBuffer(void)
{
    /* no-op */
}
