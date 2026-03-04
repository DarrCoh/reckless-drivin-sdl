/*
 * platform_sdl.c - SDL2 platform implementation for Reckless Drivin'
 *
 * Replaces the following Classic Mac OS / Carbon APIs with SDL2 equivalents:
 *   - DrawSprocket (screen management, page flipping, gamma fades)
 *   - InputSprocket / HID Manager (keyboard and gamepad input)
 *   - Sound Manager (multi-channel audio mixing)
 *   - DriverServices (high-resolution timing)
 *
 * The original game renders into a 640x480 16-bit (ARGB1555) framebuffer.
 * We maintain a software framebuffer that the game's rendering code writes
 * into directly, then upload it to an SDL streaming texture each frame.
 *
 * Audio uses a simple software mixer: the original Mac Sound Manager channels
 * are emulated as an array of mixing channels, each with independent volume,
 * pan, and pitch control. The SDL audio callback mixes all active channels
 * into the output buffer on the audio thread.
 */

#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========================================================================= */
/* Forward declarations of game element enums (from input.h)                 */
/* We redefine them here so this file compiles standalone.                    */
/* The actual values must match those in the game's input.h.                  */
/* ========================================================================= */
#ifndef kNumElements
enum {
    kForward = 0,
    kBackward,
    kLeft,
    kRight,
    kKickdown,
    kBrake,
    kFire,
    kMissile,
    kAbort,
    kPause,
    kNumElements
};
#endif

/* ========================================================================= */
/*  Screen subsystem                                                          */
/* ========================================================================= */

static SDL_Window   *sWindow   = NULL;
static SDL_Renderer *sRenderer = NULL;
static SDL_Texture  *sTexture  = NULL;

/*
 * The game writes 16-bit pixels (ARGB1555) directly into this buffer.
 * Platform_Blit2Screen() uploads it to the GPU texture each frame.
 */
/* Extra rows of padding beyond SCREEN_HEIGHT to absorb out-of-bounds writes
 * from the game's RLE sprite renderer (e.g. HUD panel at bottom of screen).
 * Only the first SCREEN_HEIGHT rows are uploaded to the GPU texture. */
static UInt16 sFramebuffer[SCREEN_WIDTH * (SCREEN_HEIGHT + FRAMEBUFFER_GUARD_ROWS)];

void Platform_InitScreen(void)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "[platform] SDL_INIT_VIDEO failed: %s\n",
                SDL_GetError());
        return;
    }

    sWindow = SDL_CreateWindow(
        "Reckless Drivin'",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!sWindow) {
        fprintf(stderr, "[platform] SDL_CreateWindow failed: %s\n",
                SDL_GetError());
        return;
    }

    sRenderer = SDL_CreateRenderer(
        sWindow, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!sRenderer) {
        fprintf(stderr, "[platform] SDL_CreateRenderer failed: %s\n",
                SDL_GetError());
        return;
    }

    /*
     * Let SDL handle letterboxing automatically when the window is resized
     * to a non-4:3 aspect ratio.
     */
    SDL_RenderSetLogicalSize(sRenderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    /* Use nearest-neighbor scaling for crisp pixel art. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    /*
     * ARGB1555 matches the original game's 16-bit pixel format.
     * The high bit is an alpha/unused bit; the remaining 15 bits encode
     * 5-5-5 RGB (same layout as Mac OS 16-bit color).
     */
    sTexture = SDL_CreateTexture(
        sRenderer,
        SDL_PIXELFORMAT_ARGB1555,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH, SCREEN_HEIGHT
    );
    if (!sTexture) {
        fprintf(stderr, "[platform] SDL_CreateTexture failed: %s\n",
                SDL_GetError());
        return;
    }

    /* Enforce minimum window size (half resolution). */
    SDL_SetWindowMinimumSize(sWindow, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

    memset(sFramebuffer, 0, sizeof(sFramebuffer));
}

void Platform_ShutdownScreen(void)
{
    if (sTexture)  { SDL_DestroyTexture(sTexture);   sTexture  = NULL; }
    if (sRenderer) { SDL_DestroyRenderer(sRenderer);  sRenderer = NULL; }
    if (sWindow)   { SDL_DestroyWindow(sWindow);      sWindow   = NULL; }
}

/*
 * Fade brightness: 0 = full black, 255 = full brightness.
 * Applied via SDL_SetTextureColorMod in Blit2Screen.
 */
static UInt8 sFadeBrightness = 255;

/* Forward declaration — defined in input subsystem section below */
static int sQuitRequested;

void Platform_Blit2Screen(void)
{
    void *texPixels;
    int   texPitch;

    if (!sTexture || !sRenderer)
        return;

    /*
     * Lock the streaming texture, memcpy the framebuffer in, then unlock.
     * This is the fastest path for software-rendered content on modern GPUs.
     */
    if (SDL_LockTexture(sTexture, NULL, &texPixels, &texPitch) == 0) {
        const int srcPitch = SCREEN_WIDTH * sizeof(UInt16);
        if (texPitch == srcPitch) {
            /* Common fast path: pitches match exactly. */
            memcpy(texPixels, sFramebuffer, SCREEN_HEIGHT * srcPitch);
        } else {
            /* Row-by-row copy when the texture pitch differs. */
            const UInt8 *src = (const UInt8 *)sFramebuffer;
            UInt8 *dst = (UInt8 *)texPixels;
            for (int y = 0; y < SCREEN_HEIGHT; y++) {
                memcpy(dst, src, srcPitch);
                src += srcPitch;
                dst += texPitch;
            }
        }
        SDL_UnlockTexture(sTexture);
    }

    SDL_SetTextureColorMod(sTexture, sFadeBrightness, sFadeBrightness, sFadeBrightness);
    SDL_RenderClear(sRenderer);
    SDL_RenderCopy(sRenderer, sTexture, NULL, NULL);
    SDL_RenderPresent(sRenderer);
}

void Platform_FadeScreen(int fade)
{
    /*
     * Original DrawSprocket gamma fading semantics:
     *   fade==1   -> animated fade to black
     *   fade==0   -> animated fade from black (full brightness)
     *   fade>=256 -> instant set: brightness = (fade-256)*255/256
     *   fade>512  -> instant set to full black
     */
    if (fade >= 256) {
        /* Instant brightness set */
        if (fade > 512) {
            sFadeBrightness = 0;
        } else {
            int b = ((fade - 256) * 255) / 256;
            if (b < 0) b = 0;
            if (b > 255) b = 255;
            sFadeBrightness = (UInt8)b;
        }
        return;
    }

    /* Animated fade */
    int target = (fade == 1) ? 0 : 255;
    int start  = (int)sFadeBrightness;
    int steps  = 20; /* ~300ms at 16ms per step */

    for (int i = 1; i <= steps; i++) {
        Platform_PollEvents();
        if (sQuitRequested) break;

        sFadeBrightness = (UInt8)(start + (target - start) * i / steps);
        Platform_Blit2Screen();
        SDL_Delay(16);
    }
    sFadeBrightness = (UInt8)target;
}

void Platform_ScreenClear(void)
{
    memset(sFramebuffer, 0, sizeof(sFramebuffer));
}

UInt16 *Platform_GetFramebuffer(int *rowBytes)
{
    if (rowBytes)
        *rowBytes = SCREEN_WIDTH * (int)sizeof(UInt16); /* 1280 */
    return sFramebuffer;
}

/* ========================================================================= */
/*  Input subsystem                                                           */
/* ========================================================================= */

/*
 * We maintain two parallel input mechanisms:
 *   1. Keyboard: an array mapping each game element to an SDL scancode.
 *   2. Gamepad:  optional SDL_GameController support (first connected pad).
 *
 * Platform_GetElement() checks both and returns true if either source is
 * active for the requested element.
 *
 * Platform_GetEvent() tracks per-element state transitions (up->down and
 * down->up) and returns them one at a time, emulating InputSprocket's
 * event list behavior.
 */

/* Scancode assigned to each game element (keyboard mapping). */
static SDL_Scancode sElementKeys[kNumElements];

/* Previous element state for edge detection in GetEvent(). */
static int sElementPrevState[kNumElements];

/* sQuitRequested is forward-declared above (near screen subsystem) for use by FadeScreen */

/* Mouse click tracking (edge-triggered: set on BUTTONDOWN, cleared after read). */
static int sMouseClicked = 0;
static int sMouseClickX  = 0;
static int sMouseClickY  = 0;

/* Optional gamepad. */
static SDL_GameController *sGameController = NULL;

/* Map game elements to SDL_GameController buttons. */
static const SDL_GameControllerButton sElementPadButton[kNumElements] = {
    SDL_CONTROLLER_BUTTON_DPAD_UP,      /* kForward  */
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,    /* kBackward */
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,    /* kLeft     */
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,   /* kRight    */
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER, /* kKickdown */
    SDL_CONTROLLER_BUTTON_A,            /* kBrake    */
    SDL_CONTROLLER_BUTTON_X,            /* kFire     */
    SDL_CONTROLLER_BUTTON_Y,            /* kMissile  */
    SDL_CONTROLLER_BUTTON_BACK,         /* kAbort    */
    SDL_CONTROLLER_BUTTON_START         /* kPause    */
};

static void TryOpenGameController(void)
{
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
        if (SDL_IsGameController(i)) {
            sGameController = SDL_GameControllerOpen(i);
            if (sGameController) {
                fprintf(stderr, "[platform] Opened game controller: %s\n",
                        SDL_GameControllerName(sGameController));
                break;
            }
        }
    }
}

void Platform_InitInput(void)
{
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "[platform] SDL_INIT_GAMECONTROLLER failed: %s\n",
                SDL_GetError());
        /* Non-fatal; keyboard still works. */
    }

    /* Set default keyboard mapping. */
    sElementKeys[kForward]   = SDL_SCANCODE_UP;
    sElementKeys[kBackward]  = SDL_SCANCODE_DOWN;
    sElementKeys[kLeft]      = SDL_SCANCODE_LEFT;
    sElementKeys[kRight]     = SDL_SCANCODE_RIGHT;
    sElementKeys[kKickdown]  = SDL_SCANCODE_LSHIFT;
    sElementKeys[kBrake]     = SDL_SCANCODE_SPACE;
    sElementKeys[kFire]      = SDL_SCANCODE_Z;
    sElementKeys[kMissile]   = SDL_SCANCODE_X;
    sElementKeys[kAbort]     = SDL_SCANCODE_ESCAPE;
    sElementKeys[kPause]     = SDL_SCANCODE_P;

    memset(sElementPrevState, 0, sizeof(sElementPrevState));
    sQuitRequested = 0;

    /* Try to open a game controller if one is already connected. */
    TryOpenGameController();
}

void Platform_LoadKeyBindings(const UInt8 *keyCodes)
{
    /*
     * Load saved SDL scancodes from preferences.
     * Called after LoadPrefs() in main.c Init().
     * Only apply if the values look like valid SDL scancodes (> 0 and < 512).
     */
    for (int i = 0; i < kNumElements - 2; i++) { /* skip kAbort and kPause */
        SDL_Scancode sc = (SDL_Scancode)keyCodes[i];
        if (sc > 0 && sc < SDL_NUM_SCANCODES)
            sElementKeys[i] = sc;
    }
}

void Platform_ShutdownInput(void)
{
    if (sGameController) {
        SDL_GameControllerClose(sGameController);
        sGameController = NULL;
    }
}

/*
 * Convert window-space mouse coordinates to 640x480 logical coordinates.
 * Accounts for letterboxing from SDL_RenderSetLogicalSize and window resizing.
 */
static void WindowToLogical(int winX, int winY, int *logX, int *logY)
{
    int winW, winH;
    if (!sWindow) { *logX = winX; *logY = winY; return; }

    SDL_GetWindowSize(sWindow, &winW, &winH);
    if (winW <= 0 || winH <= 0) { *logX = winX; *logY = winY; return; }

    float scaleX = (float)winW / SCREEN_WIDTH;
    float scaleY = (float)winH / SCREEN_HEIGHT;
    float scale  = (scaleX < scaleY) ? scaleX : scaleY;

    int viewW   = (int)(SCREEN_WIDTH  * scale);
    int viewH   = (int)(SCREEN_HEIGHT * scale);
    int offsetX = (winW - viewW) / 2;
    int offsetY = (winH - viewH) / 2;

    *logX = (int)((winX - offsetX) / scale);
    *logY = (int)((winY - offsetY) / scale);
}

void Platform_PollEvents(void)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            sQuitRequested = 1;
            break;

        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_RESIZED && sWindow) {
                /* Enforce 4:3 aspect ratio on resize. */
                int w = ev.window.data1;
                int h = ev.window.data2;
                int newH = (w * SCREEN_HEIGHT) / SCREEN_WIDTH;
                if (newH != h)
                    SDL_SetWindowSize(sWindow, w, newH);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_LEFT) {
                int lx, ly;
                WindowToLogical(ev.button.x, ev.button.y, &lx, &ly);
                sMouseClicked = 1;
                sMouseClickX  = lx;
                sMouseClickY  = ly;
            }
            break;

        case SDL_CONTROLLERDEVICEADDED:
            if (!sGameController) {
                sGameController = SDL_GameControllerOpen(ev.cdevice.which);
                if (sGameController) {
                    fprintf(stderr, "[platform] Controller added: %s\n",
                            SDL_GameControllerName(sGameController));
                }
            }
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            if (sGameController &&
                ev.cdevice.which == SDL_JoystickInstanceID(
                    SDL_GameControllerGetJoystick(sGameController)))
            {
                SDL_GameControllerClose(sGameController);
                sGameController = NULL;
                fprintf(stderr, "[platform] Controller removed\n");
            }
            break;

        default:
            break;
        }
    }
}

int Platform_IsKeyDown(int scancode)
{
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    if (scancode >= 0 && scancode < SDL_NUM_SCANCODES)
        return state[scancode] ? 1 : 0;
    return 0;
}

int Platform_GetElement(int element)
{
    if (element < 0 || element >= kNumElements)
        return 0;

    /* Check keyboard. */
    const Uint8 *kbState = SDL_GetKeyboardState(NULL);
    if (kbState[sElementKeys[element]])
        return 1;

    /* Check gamepad. */
    if (sGameController) {
        if (SDL_GameControllerGetButton(sGameController,
                                        sElementPadButton[element]))
            return 1;

        /*
         * Also support analog stick for steering/throttle:
         *   Left stick Y axis: forward (up) / backward (down)
         *   Left stick X axis: left / right
         * Use a dead zone of ~30% (roughly 9830 out of 32767).
         */
        const int kDeadZone = 9830;
        switch (element) {
        case kForward: {
            int val = SDL_GameControllerGetAxis(sGameController,
                          SDL_CONTROLLER_AXIS_LEFTY);
            if (val < -kDeadZone) return 1;
            break;
        }
        case kBackward: {
            int val = SDL_GameControllerGetAxis(sGameController,
                          SDL_CONTROLLER_AXIS_LEFTY);
            if (val > kDeadZone) return 1;
            break;
        }
        case kLeft: {
            int val = SDL_GameControllerGetAxis(sGameController,
                          SDL_CONTROLLER_AXIS_LEFTX);
            if (val < -kDeadZone) return 1;
            break;
        }
        case kRight: {
            int val = SDL_GameControllerGetAxis(sGameController,
                          SDL_CONTROLLER_AXIS_LEFTX);
            if (val > kDeadZone) return 1;
            break;
        }
        case kKickdown: {
            /* Right trigger as kickdown alternative. */
            int val = SDL_GameControllerGetAxis(sGameController,
                          SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            if (val > kDeadZone) return 1;
            break;
        }
        case kBrake: {
            /* Left trigger as brake alternative. */
            int val = SDL_GameControllerGetAxis(sGameController,
                          SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            if (val > kDeadZone) return 1;
            break;
        }
        default:
            break;
        }
    }

    return 0;
}

int Platform_GetEvent(int *element, int *data)
{
    /*
     * Scan all elements looking for a state change since the last call.
     * This mimics InputSprocket's ISpElementList_GetNextEvent():
     *   - element: which game element changed
     *   - data: 1 = pressed (down), 0 = released (up)
     *
     * We return one event per call, cycling through elements in order.
     * The caller loops until we return 0 (no more events).
     */
    for (int i = 0; i < kNumElements; i++) {
        int current = Platform_GetElement(i);
        if (current != sElementPrevState[i]) {
            *element = i;
            *data = current;
            sElementPrevState[i] = current;
            return 1;
        }
    }
    return 0;
}

void Platform_FlushInput(void)
{
    /* Drain the SDL event queue. */
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

    /* Reset edge-detection state so no spurious events fire next frame. */
    for (int i = 0; i < kNumElements; i++)
        sElementPrevState[i] = Platform_GetElement(i);

    /* Clear any pending mouse click. */
    sMouseClicked = 0;
}

int Platform_ShouldQuit(void)
{
    return sQuitRequested;
}

/* ========================================================================= */
/*  Key mapping API                                                           */
/* ========================================================================= */

void Platform_SetElementKey(int element, SDL_Scancode key)
{
    if (element >= 0 && element < kNumElements)
        sElementKeys[element] = key;
}

SDL_Scancode Platform_GetElementKey(int element)
{
    if (element >= 0 && element < kNumElements)
        return sElementKeys[element];
    return SDL_SCANCODE_UNKNOWN;
}

SDL_Scancode Platform_WaitForKey(void)
{
    /* Block until a non-modifier key is pressed. Return 0 if ESC pressed. */
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    for (;;) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                sQuitRequested = 1;
                return SDL_SCANCODE_UNKNOWN;
            }
            if (ev.type == SDL_KEYDOWN) {
                SDL_Scancode sc = ev.key.keysym.scancode;
                /* Ignore pure modifier keys */
                if (sc == SDL_SCANCODE_LSHIFT || sc == SDL_SCANCODE_RSHIFT ||
                    sc == SDL_SCANCODE_LCTRL  || sc == SDL_SCANCODE_RCTRL  ||
                    sc == SDL_SCANCODE_LALT   || sc == SDL_SCANCODE_RALT   ||
                    sc == SDL_SCANCODE_LGUI   || sc == SDL_SCANCODE_RGUI)
                    continue;
                if (sc == SDL_SCANCODE_ESCAPE)
                    return SDL_SCANCODE_UNKNOWN; /* cancelled */
                return sc;
            }
        }
        SDL_Delay(16);
    }
}

/* ========================================================================= */
/*  Audio subsystem                                                           */
/* ========================================================================= */

/*
 * Software mixer for the SDL port.  The original game uses Mac Sound Manager
 * with various sample formats (8-bit unsigned and 16-bit signed big-endian)
 * at various sample rates (11 kHz, 22 kHz, 44.1 kHz).
 *
 * We output signed 16-bit stereo at 44100 Hz.  Each channel stores its
 * source format (8-bit or 16-bit) and a base pitch that encodes the ratio
 * of the source sample rate to 44100 Hz.  The game's pitch multiplier is
 * applied on top of that base.
 *
 * Thread safety: sMixMutex protects all shared channel state.
 */

#define AUDIO_BUFFER_FRAMES 1024

#define FRAC_BITS 16
#define FRAC_ONE  (1 << FRAC_BITS)

typedef struct {
    const UInt8 *sampleData;  /* pointer to PCM data (8-bit or 16-bit BE)  */
    int          sampleLen;   /* length in sample frames                    */
    UInt32       position;    /* playback position in fixed-point           */
    float        volumeL;     /* left channel volume  [0..1]               */
    float        volumeR;     /* right channel volume [0..1]               */
    float        basePitch;   /* native rate / mixer rate (set at play)    */
    float        gamePitch;   /* game-controlled pitch multiplier          */
    int          bitsPerSample; /* 8 or 16                                 */
    int          active;      /* non-zero if channel is playing            */
    int          looping;     /* non-zero to loop the sample               */
} AudioChannel;

static AudioChannel      sChannels[kNumAudioChannels];
static SDL_AudioDeviceID  sAudioDevice = 0;
static SDL_mutex         *sMixMutex    = NULL;

/*
 * Read one sample from a channel and return it as a signed 16-bit value.
 * Handles both unsigned 8-bit and signed 16-bit big-endian source formats.
 */
static inline int ReadSample(const AudioChannel *c, UInt32 idx)
{
    if (c->bitsPerSample == 16) {
        /* Signed 16-bit big-endian */
        const UInt8 *p = c->sampleData + idx * 2;
        int16_t s = (int16_t)((p[0] << 8) | p[1]);
        return (int)s;
    } else {
        /* Unsigned 8-bit → signed 16-bit */
        return ((int)c->sampleData[idx] - 128) << 8;
    }
}

/*
 * SDL audio callback.  Mixes all active channels into a signed 16-bit
 * stereo interleaved output buffer.
 */
static void AudioCallback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;

    /* len is in bytes; each stereo frame is 4 bytes (2 × int16) */
    int numFrames = len / 4;
    int16_t *out = (int16_t *)stream;

    /* Silence */
    memset(stream, 0, len);

    if (SDL_LockMutex(sMixMutex) != 0)
        return;

    for (int ch = 0; ch < kNumAudioChannels; ch++) {
        AudioChannel *c = &sChannels[ch];
        if (!c->active || !c->sampleData)
            continue;

        float volL = c->volumeL;
        float volR = c->volumeR;

        if (volL <= 0.0f && volR <= 0.0f) {
            /* Muted -- advance position silently */
            float pitch = c->basePitch * c->gamePitch;
            if (pitch <= 0.0f) pitch = 1.0f;
            UInt32 pitchFP = (UInt32)(pitch * FRAC_ONE);
            c->position += pitchFP * (UInt32)numFrames;
            if ((c->position >> FRAC_BITS) >= (UInt32)c->sampleLen) {
                if (c->looping)
                    c->position = 0;
                else
                    c->active = 0;
            }
            continue;
        }

        float pitch = c->basePitch * c->gamePitch;
        if (pitch <= 0.0f) pitch = 1.0f;
        UInt32 pitchFP = (UInt32)(pitch * FRAC_ONE);

        /* Scale volumes to 16-bit range (×32767) as fixed-point for speed */
        int iVolL = (int)(volL * 32767.0f);
        int iVolR = (int)(volR * 32767.0f);

        for (int i = 0; i < numFrames; i++) {
            UInt32 sampleIndex = c->position >> FRAC_BITS;
            if (sampleIndex >= (UInt32)c->sampleLen) {
                if (c->looping) {
                    c->position = 0;
                    sampleIndex = 0;
                } else {
                    c->active = 0;
                    break;
                }
            }

            int sample = ReadSample(c, sampleIndex);

            /* Mix into stereo output with volume scaling */
            int outIdx = i * 2;
            int mixL = (int)out[outIdx]     + ((sample * iVolL) >> 15);
            int mixR = (int)out[outIdx + 1] + ((sample * iVolR) >> 15);

            /* Clamp to int16 range */
            if (mixL >  32767) mixL =  32767;
            if (mixL < -32768) mixL = -32768;
            if (mixR >  32767) mixR =  32767;
            if (mixR < -32768) mixR = -32768;

            out[outIdx]     = (int16_t)mixL;
            out[outIdx + 1] = (int16_t)mixR;

            c->position += pitchFP;
        }
    }

    SDL_UnlockMutex(sMixMutex);
}

void Platform_InitAudio(void)
{
    SDL_AudioSpec desired, obtained;

    /* Guard against double initialization */
    if (sAudioDevice != 0)
        return;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[platform] SDL_INIT_AUDIO failed: %s\n",
                SDL_GetError());
        return;
    }

    sMixMutex = SDL_CreateMutex();
    if (!sMixMutex) {
        fprintf(stderr, "[platform] SDL_CreateMutex failed: %s\n",
                SDL_GetError());
        return;
    }

    memset(sChannels, 0, sizeof(sChannels));
    for (int i = 0; i < kNumAudioChannels; i++) {
        sChannels[i].basePitch = 1.0f;
        sChannels[i].gamePitch = 1.0f;
    }

    SDL_memset(&desired, 0, sizeof(desired));
    desired.freq     = AUDIO_MIX_RATE;
    desired.format   = AUDIO_S16SYS;  /* signed 16-bit native endian */
    desired.channels = 2;             /* stereo output */
    desired.samples  = AUDIO_BUFFER_FRAMES;
    desired.callback = AudioCallback;
    desired.userdata = NULL;

    sAudioDevice = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (sAudioDevice == 0) {
        fprintf(stderr, "[platform] SDL_OpenAudioDevice failed: %s\n",
                SDL_GetError());
        return;
    }

    fprintf(stderr, "[platform] Audio: %d Hz, %d-bit, %d ch, %d samples\n",
            obtained.freq, SDL_AUDIO_BITSIZE(obtained.format),
            obtained.channels, obtained.samples);

    SDL_PauseAudioDevice(sAudioDevice, 0);
}

void Platform_ShutdownAudio(void)
{
    if (sAudioDevice) {
        SDL_PauseAudioDevice(sAudioDevice, 1);
        SDL_CloseAudioDevice(sAudioDevice);
        sAudioDevice = 0;
    }
    if (sMixMutex) {
        SDL_DestroyMutex(sMixMutex);
        sMixMutex = NULL;
    }
    memset(sChannels, 0, sizeof(sChannels));
}

void Platform_PlaySound(int channelId, const void *sampleData, int sampleLen,
                        float volume, float pan, float pitch,
                        int bitsPerSample)
{
    if (channelId < 0 || channelId >= kNumAudioChannels)
        return;
    if (!sampleData || sampleLen <= 0)
        return;

    float clampedPan = pan;
    if (clampedPan < -1.0f) clampedPan = -1.0f;
    if (clampedPan >  1.0f) clampedPan =  1.0f;

    float volL = volume * (1.0f - clampedPan) * 0.5f;
    float volR = volume * (1.0f + clampedPan) * 0.5f;

    if (volL < 0.0f) volL = 0.0f; if (volL > 1.0f) volL = 1.0f;
    if (volR < 0.0f) volR = 0.0f; if (volR > 1.0f) volR = 1.0f;

    if (pitch <= 0.0f) pitch = 1.0f;
    if (bitsPerSample != 8 && bitsPerSample != 16)
        bitsPerSample = 8;

    SDL_LockMutex(sMixMutex);
    {
        AudioChannel *c = &sChannels[channelId];
        c->sampleData   = (const UInt8 *)sampleData;
        c->sampleLen    = sampleLen;
        c->position     = 0;
        c->volumeL      = volL;
        c->volumeR      = volR;
        c->basePitch    = pitch;
        c->gamePitch    = 1.0f;
        c->bitsPerSample = bitsPerSample;
        c->active       = 1;
        c->looping      = (channelId == kAudioChannelEngine ||
                           channelId == kAudioChannelSkid) ? 1 : 0;
    }
    SDL_UnlockMutex(sMixMutex);
}

void Platform_StopChannel(int channelId)
{
    if (channelId < 0 || channelId >= kNumAudioChannels)
        return;

    SDL_LockMutex(sMixMutex);
    sChannels[channelId].active = 0;
    SDL_UnlockMutex(sMixMutex);
}

void Platform_SetChannelVolume(int channelId, float left, float right)
{
    if (channelId < 0 || channelId >= kNumAudioChannels)
        return;

    if (left  < 0.0f) left  = 0.0f; if (left  > 1.0f) left  = 1.0f;
    if (right < 0.0f) right = 0.0f; if (right > 1.0f) right = 1.0f;

    SDL_LockMutex(sMixMutex);
    sChannels[channelId].volumeL = left;
    sChannels[channelId].volumeR = right;
    SDL_UnlockMutex(sMixMutex);
}

void Platform_SetChannelPitch(int channelId, float pitch)
{
    if (channelId < 0 || channelId >= kNumAudioChannels)
        return;

    if (pitch <= 0.0f) pitch = 1.0f;

    SDL_LockMutex(sMixMutex);
    sChannels[channelId].gamePitch = pitch;
    SDL_UnlockMutex(sMixMutex);
}

/* ========================================================================= */
/*  Timing                                                                    */
/* ========================================================================= */

UInt64 Platform_GetMicroseconds(void)
{
    /*
     * SDL_GetPerformanceCounter / SDL_GetPerformanceFrequency gives us
     * the highest resolution monotonic timer available on the platform.
     * We convert to microseconds, being careful to avoid overflow by
     * dividing before multiplying when the frequency is large enough.
     *
     * This replaces the original game's use of DriverServices UpTime()
     * and InputSprocket ISpUptime()/ISpTimeToMicroseconds().
     */
    UInt64 counter = (UInt64)SDL_GetPerformanceCounter();
    UInt64 freq    = (UInt64)SDL_GetPerformanceFrequency();

    /*
     * If the frequency is a multiple of 1,000,000 we can divide it down
     * cleanly to avoid intermediate overflow.  Otherwise, use the general
     * formula which is safe as long as the counter value hasn't been running
     * for centuries (which it hasn't -- SDL counters reset on boot).
     */
    if (freq >= 1000000ULL && (freq % 1000000ULL) == 0) {
        return counter / (freq / 1000000ULL);
    }
    return (counter * 1000000ULL) / freq;
}

/* ========================================================================= */
/*  Mouse                                                                     */
/* ========================================================================= */

void Platform_GetMouseState(int *x, int *y, int *down)
{
    int winX, winY;
    Uint32 buttons = SDL_GetMouseState(&winX, &winY);
    int lx, ly;
    WindowToLogical(winX, winY, &lx, &ly);
    if (x) *x = lx;
    if (y) *y = ly;
    if (down) *down = (buttons & SDL_BUTTON_LMASK) ? 1 : 0;
}

int Platform_GetMouseClick(int *x, int *y)
{
    if (sMouseClicked) {
        if (x) *x = sMouseClickX;
        if (y) *y = sMouseClickY;
        sMouseClicked = 0;
        return 1;
    }
    return 0;
}

/* ========================================================================= */
/*  Cursor                                                                    */
/* ========================================================================= */

void Platform_ShowCursor(void)
{
    SDL_ShowCursor(SDL_ENABLE);
}

void Platform_HideCursor(void)
{
    SDL_ShowCursor(SDL_DISABLE);
}

/* ========================================================================= */
/*  Misc                                                                      */
/* ========================================================================= */

int Platform_ContinuePress(void)
{
    /*
     * Replaces the original ContinuePress(), which checked if any of the
     * "action" buttons were held on a gamepad (kForward, kKickdown, kFire,
     * kMissile). Used to advance past "press any button to continue" screens.
     */
    if (Platform_GetElement(kForward))  return 1;
    if (Platform_GetElement(kKickdown)) return 1;
    if (Platform_GetElement(kFire))     return 1;
    if (Platform_GetElement(kMissile))  return 1;

    /* Also accept Enter / Return from keyboard as a "continue" key. */
    const Uint8 *kbState = SDL_GetKeyboardState(NULL);
    if (kbState[SDL_SCANCODE_RETURN] || kbState[SDL_SCANCODE_KP_ENTER])
        return 1;

    return 0;
}
