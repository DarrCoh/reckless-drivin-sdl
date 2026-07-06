#include "compat.h"
#include "error.h"
#include "screen.h"
#include "input.h"
#include "gameinitexit.h"
#include "preferences.h"
#include "high.h"
#include "gamesounds.h"
#include "lzrw.h"
#include "resources.h"
#include "register.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>

enum{
	kNoButton=-1,
	kStartGameButton,
	kPrefsButton,
	kScoreButton,
	kHelpButton,
	kQuitButton,
	kRegisterButton,
	kAboutButton
};

/* These globals are defined in main.c; we just reference them here */
extern int gExit;
extern short gLevelResFile,gAppResFile;
unsigned char gLevelFileName[64];
static int gInterfaceInited=false;

/* Menu image buffers for hover/press visual feedback */
static UInt16 sMenuNormal[SCREEN_WIDTH * SCREEN_HEIGHT];
static UInt16 sMenuHover[SCREEN_WIDTH * SCREEN_HEIGHT];
static UInt16 sMenuPressed[SCREEN_WIDTH * SCREEN_HEIGHT];
static int sMenuImagesLoaded = 0;
static int sHoverButton = -1; /* which button the mouse is currently over */

/* Button hit rectangles from the original 'Recs' resource id=1000.
 * Each entry is {top, left, bottom, right} in 640x480 coordinates.
 * Order matches the button enum: Start, Prefs, Scores, Help, Quit, Register. */
typedef struct { int top, left, bottom, right; } ButtonRect;
static const ButtonRect kButtonRects[] = {
	{105, 389, 202, 588},  /* kStartGameButton */
	{125, 114, 193, 250},  /* kPrefsButton     */
	{267,  69, 334, 199},  /* kScoreButton     */
	{284, 433, 352, 563},  /* kHelpButton      */
	{392, 470, 458, 601},  /* kQuitButton      */
	{231, 257, 297, 389},  /* kRegisterButton  */
};
#define kNumButtons (sizeof(kButtonRects) / sizeof(kButtonRects[0]))

static int HitTestButtons(int x, int y)
{
	for (int i = 0; i < (int)kNumButtons; i++) {
		if (x >= kButtonRects[i].left && x < kButtonRects[i].right &&
		    y >= kButtonRects[i].top  && y < kButtonRects[i].bottom)
			return i;
	}
	return kNoButton;
}

/* Forward declaration — ShowPicScreen is defined later in this file */
void ShowPicScreen(int id);

/* ======================================================================== */
/* Menu image helpers for hover/press states                                 */
/* ======================================================================== */

/* Load a PPic into a 640x480 pixel buffer by rendering to the framebuffer
 * and copying. Returns 1 on success. */
static int LoadPPicToBuffer(int ppicId, UInt16 *buf)
{
	int rowBytes;
	UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
	ShowPicScreen(ppicId);
	memcpy(buf, fb, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(UInt16));
	return 1;
}

/* Copy just one button's rectangle from a source buffer to the framebuffer */
static void BlitButtonRect(int button, UInt16 *source)
{
	if (button < 0 || button >= (int)kNumButtons) return;
	int rowBytes;
	UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
	int fbStride = rowBytes / 2;
	const ButtonRect *r = &kButtonRects[button];
	for (int y = r->top; y < r->bottom && y < SCREEN_HEIGHT; y++) {
		for (int x = r->left; x < r->right && x < SCREEN_WIDTH; x++) {
			fb[y * fbStride + x] = source[y * SCREEN_WIDTH + x];
		}
	}
}

/* Map a keyboard shortcut to the button index it corresponds to.
 * Returns kNoButton if no match. */
static int KeyToButton(void)
{
	if (Platform_IsKeyDown(SDL_SCANCODE_RETURN) ||
	    Platform_IsKeyDown(SDL_SCANCODE_KP_ENTER) ||
	    Platform_IsKeyDown(SDL_SCANCODE_S) ||
	    Platform_IsKeyDown(SDL_SCANCODE_N))
		return kStartGameButton;
	if (Platform_IsKeyDown(SDL_SCANCODE_P))
		return kPrefsButton;
	if (Platform_IsKeyDown(SDL_SCANCODE_C) ||
	    Platform_IsKeyDown(SDL_SCANCODE_O))
		return kScoreButton;
	if (Platform_IsKeyDown(SDL_SCANCODE_H) ||
	    Platform_IsKeyDown(SDL_SCANCODE_SLASH))
		return kHelpButton;
	if (Platform_IsKeyDown(SDL_SCANCODE_Q))
		return kQuitButton;
	return kNoButton;
}

/* ======================================================================== */
/* Mac PICT v2 renderer (minimal — handles DirectBitsRect for 16-bit)       */
/* ======================================================================== */

/* Read a big-endian UInt16 from unaligned memory */
static inline UInt16 RdBE16(const UInt8 *p)
{
	return (UInt16)((p[0] << 8) | p[1]);
}

/*
 * PackBits decompression for 16-bit pixel data (word-level).
 * Each unit is a 2-byte word.  Flag byte n:
 *   n >= 0:    copy (n+1) words literally  (2*(n+1) bytes)
 *   n < 0, != -128: repeat next word (1-n) times (2*(1-n) bytes)
 *   n == -128: no-op
 */
static void UnpackBitsRow16(const UInt8 *src, int srcLen, UInt8 *dst, int dstLen)
{
	const UInt8 *srcEnd = src + srcLen;
	UInt8 *dstEnd = dst + dstLen;

	while (src < srcEnd && dst + 1 < dstEnd) {
		int8_t n = (int8_t)*src++;
		if (n >= 0) {
			/* Literal run: copy (n+1) 16-bit words */
			int count = n + 1;
			int bytes = count * 2;
			for (int i = 0; i < bytes && src < srcEnd && dst < dstEnd; i++)
				*dst++ = *src++;
		} else if (n != (int8_t)-128) {
			/* Repeat run: repeat next 2 bytes (1-n) times */
			int count = 1 - (int)n;
			UInt8 b0 = (src < srcEnd) ? *src++ : 0;
			UInt8 b1 = (src < srcEnd) ? *src++ : 0;
			for (int i = 0; i < count && dst + 1 < dstEnd; i++) {
				*dst++ = b0;
				*dst++ = b1;
			}
		}
		/* n == -128: no-op */
	}
}

/*
 * Standard PackBits decompression (byte-level, for 8-bit data).
 */
static void UnpackBitsRow8(const UInt8 *src, int srcLen, UInt8 *dst, int dstLen)
{
	const UInt8 *srcEnd = src + srcLen;
	UInt8 *dstEnd = dst + dstLen;

	while (src < srcEnd && dst < dstEnd) {
		int8_t n = (int8_t)*src++;
		if (n >= 0) {
			int count = n + 1;
			while (count-- > 0 && src < srcEnd && dst < dstEnd)
				*dst++ = *src++;
		} else if (n != (int8_t)-128) {
			int count = 1 - (int)n;
			UInt8 val = (src < srcEnd) ? *src++ : 0;
			while (count-- > 0 && dst < dstEnd)
				*dst++ = val;
		}
	}
}

/*
 * Parse a Mac PICT v2 and render pixel data into the game's framebuffer.
 * Handles two opcodes:
 *   0x009A  DirectBitsRect — 16-bit 5-5-5 RGB pixels (most game screens)
 *   0x0098  PackBitsRect   — 8-bit indexed color with embedded CLUT (pause screen)
 */
static int RenderPICTToFramebuffer(const UInt8 *data, long dataSize)
{
	int rowBytes;
	UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
	if (!fb) return 0;

	/* Scan for DirectBitsRect (0x009A) or PackBitsRect (0x0098) */
	long pos = -1;
	int opcode = 0;
	for (long i = 10; i < dataSize - 1; i++) {
		if (data[i] == 0x00 && (data[i+1] == 0x9A || data[i+1] == 0x98)) {
			opcode = data[i+1];
			pos = i + 2; /* skip the opcode */
			break;
		}
	}
	if (pos < 0) return 0;

	/* DirectBitsRect has a 4-byte baseAddr prefix; PackBitsRect does not */
	if (opcode == 0x9A)
		pos += 4;

	/* Parse PixMap header */
	if (pos + 46 > dataSize) return 0;

	UInt16 pmRowBytesRaw = RdBE16(data + pos);
	int isPixMap = (pmRowBytesRaw & 0x8000) != 0;
	UInt16 pmRowBytes = pmRowBytesRaw & 0x3FFF;
	pos += 2;

	int boundsTop    = (int16_t)RdBE16(data + pos); pos += 2;
	int boundsLeft   = (int16_t)RdBE16(data + pos); pos += 2;
	int boundsBottom = (int16_t)RdBE16(data + pos); pos += 2;
	int boundsRight  = (int16_t)RdBE16(data + pos); pos += 2;

	int picWidth  = boundsRight - boundsLeft;
	int picHeight = boundsBottom - boundsTop;

	/* Read pixelSize from the PixMap header (we need it to decide 8-bit vs 16-bit) */
	/* PixMap fields after bounds: pmVersion(2) + packType(2) + packSize(4) +
	   hRes(4) + vRes(4) + pixelType(2) = 18 bytes to pixelSize */
	int pixelSize = 16; /* default for DirectBitsRect */
	if (isPixMap && pos + 20 <= dataSize) {
		pixelSize = (int)RdBE16(data + pos + 18);
	}

	/* Skip rest of PixMap header: 36 bytes total */
	pos += 36;

	/* For 8-bit indexed color: parse the embedded color table (CLUT) */
	UInt16 clut[256];
	memset(clut, 0, sizeof(clut));

	if (pixelSize <= 8 && isPixMap) {
		/* Color table: ctSeed(4) + ctFlags(2) + ctSize(2) */
		if (pos + 8 > dataSize) return 0;
		/* UInt32 ctSeed = ... */ pos += 4;
		/* UInt16 ctFlags = ... */ pos += 2;
		UInt16 ctSize = RdBE16(data + pos); pos += 2;
		int numColors = (int)ctSize + 1;
		if (numColors > 256) numColors = 256;

		/* Each entry: value(2) + r(2) + g(2) + b(2) = 8 bytes */
		for (int c = 0; c < numColors; c++) {
			if (pos + 8 > dataSize) break;
			UInt16 idx = RdBE16(data + pos); pos += 2;
			UInt16 r   = RdBE16(data + pos); pos += 2;
			UInt16 g   = RdBE16(data + pos); pos += 2;
			UInt16 b   = RdBE16(data + pos); pos += 2;
			/* Convert 16-bit Mac color to 5-bit and pack as ARGB1555 */
			int r5 = (r >> 11) & 0x1F;
			int g5 = (g >> 11) & 0x1F;
			int b5 = (b >> 11) & 0x1F;
			UInt16 pixel = (UInt16)(0x8000 | (r5 << 10) | (g5 << 5) | b5);
			if (idx < 256)
				clut[idx] = pixel;
			else
				clut[c] = pixel; /* some PICTs use sequential indices */
		}
	}

	/* Skip srcRect(8) + dstRect(8) + mode(2) = 18 bytes */
	pos += 18;

	/* Now at the PackBits pixel data */
	if (picWidth <= 0 || picHeight <= 0 || picWidth > SCREEN_WIDTH || picHeight > SCREEN_HEIGHT)
		return 0;

	UInt8 rowBuf[SCREEN_WIDTH * 2]; /* max decompressed row */
	int fbRowPixels = rowBytes / 2;

	for (int y = 0; y < picHeight; y++) {
		if (pos >= dataSize) break;

		int packedLen;
		if (pmRowBytes > 250) {
			if (pos + 2 > dataSize) break;
			packedLen = (int)RdBE16(data + pos);
			pos += 2;
		} else {
			packedLen = data[pos];
			pos += 1;
		}

		if (pos + packedLen > dataSize) break;

		memset(rowBuf, 0, pmRowBytes);

		UInt16 *fbRow = fb + y * fbRowPixels;
		int pixelsThisRow = picWidth;
		if (pixelsThisRow > SCREEN_WIDTH) pixelsThisRow = SCREEN_WIDTH;

		if (pixelSize == 16) {
			/* 16-bit: word-level PackBits, then byte-swap each pixel */
			UnpackBitsRow16(data + pos, packedLen, rowBuf, pmRowBytes);
			for (int x = 0; x < pixelsThisRow; x++) {
				UInt16 pixel = (UInt16)((rowBuf[x*2] << 8) | rowBuf[x*2+1]);
				fbRow[x] = pixel;
			}
		} else {
			/* 8-bit: byte-level PackBits, then look up each index in CLUT */
			UnpackBitsRow8(data + pos, packedLen, rowBuf, pmRowBytes);
			for (int x = 0; x < pixelsThisRow; x++) {
				fbRow[x] = clut[rowBuf[x]];
			}
		}

		pos += packedLen;
	}

	return 1;
}

/* ======================================================================== */
/* ShowPicScreen — load, decompress, parse PICT, and display                 */
/* ======================================================================== */

void ShowPicScreen(int id)
{
	Handle pic;
	Platform_ScreenClear();

	pic=Resources_Get(MakeFourCC("PPic"),id);
	if(pic)
	{
		long compressedSize=Resources_GetSize(pic);
		LZRWDecodeHandle(&pic,compressedSize);

		long decompSize = Resources_GetSize(pic);
		if(decompSize > 0)
			RenderPICTToFramebuffer((const UInt8*)*pic, decompSize);

		Resources_Release(pic);
	}

	Platform_Blit2Screen();
}

void ShowPicScreenNoFade(int id)
{
	ShowPicScreen(id);
}

void ScreenUpdate(void)
{
	Platform_Blit2Screen();
}

void DisposeInterface(void)
{
	if(gInterfaceInited)
	{
		gInterfaceInited=false;
	}
}

void SaveFlushEvents(void)
{
	Platform_FlushInput();
}

void InitInterface(void)
{
	if(!gInterfaceInited)
	{
		gInterfaceInited=true;

		/* Load all three menu state images */
		LoadPPicToBuffer(1000, sMenuNormal);
		LoadPPicToBuffer(1001, sMenuHover);
		LoadPPicToBuffer(1002, sMenuPressed);
		sMenuImagesLoaded = 1;
	}
	InputMode(kInputSuspended);
	/* Restore normal menu from buffer */
	if (sMenuImagesLoaded) {
		int rowBytes;
		UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
		memcpy(fb, sMenuNormal, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(UInt16));
		Platform_Blit2Screen();
	} else {
		ShowPicScreen(1000);
	}
	sHoverButton = kNoButton;
	SaveFlushEvents();
	gGameOn=false;
	/* Wait for action keys to be released to prevent immediate menu triggers */
	while(Platform_IsKeyDown(SDL_SCANCODE_RETURN)||
		  Platform_IsKeyDown(SDL_SCANCODE_KP_ENTER)||
		  Platform_IsKeyDown(SDL_SCANCODE_SPACE)||
		  Platform_IsKeyDown(SDL_SCANCODE_ESCAPE))
	{
		Platform_PollEvents();
		SDL_Delay(16);
	}
}

void WaitForPress(void)
{
	int pressed=false;
	Platform_ShowCursor();
	/* Wait for any held keys/buttons to be released first */
	for(;;)
	{
		Platform_PollEvents();
		if(Platform_ShouldQuit()) return;
		Platform_GetMouseClick(NULL,NULL);
		if(!Platform_IsKeyDown(SDL_SCANCODE_RETURN)&&
		   !Platform_IsKeyDown(SDL_SCANCODE_SPACE)&&
		   !Platform_IsKeyDown(SDL_SCANCODE_ESCAPE)&&
		   !Platform_ContinuePress())
			break;
		SDL_Delay(16);
	}
	while(!pressed)
	{
		Platform_PollEvents();
		if(Platform_ShouldQuit())
			break;
		if(Platform_IsKeyDown(SDL_SCANCODE_RETURN)||
		   Platform_IsKeyDown(SDL_SCANCODE_SPACE)||
		   Platform_IsKeyDown(SDL_SCANCODE_ESCAPE)||
		   Platform_ContinuePress()||
		   Platform_GetMouseClick(NULL,NULL))
			pressed=true;
		SDL_Delay(16);
	}
	SaveFlushEvents();
}

/* Restore the normal menu screen from buffer (avoids re-parsing PICT) */
static void RestoreMenuScreen(void)
{
	if (sMenuImagesLoaded) {
		int rowBytes;
		UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
		memcpy(fb, sMenuNormal, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(UInt16));
		Platform_Blit2Screen();
	} else {
		ShowPicScreen(1000);
	}
}

static void HandleCommand(int cmd)
{
	switch(cmd)
	{
		case kNoButton: return;
		case kRegisterButton:
			Register(true);
			RestoreMenuScreen();
			break;
		case kStartGameButton:
			StartGame(0);
			break;
		case kPrefsButton:
			Preferences();
			RestoreMenuScreen();
			break;
		case kHelpButton:
			ShowPicScreen(1007);
			WaitForPress();
			ShowPicScreen(1008);
			WaitForPress();
			RestoreMenuScreen();
			break;
		case kScoreButton:
			ShowHighScores(-1);
			RestoreMenuScreen();
			break;
		case kQuitButton:
			gExit=true;
			break;
	}
}

/* Show press feedback for a button, then execute the command */
static void ActivateButton(int button)
{
	if (sMenuImagesLoaded && button >= 0 && button < (int)kNumButtons) {
		BlitButtonRect(button, sMenuPressed);
		Platform_Blit2Screen();
		SimplePlaySound(147);
		SDL_Delay(100);
	}
	HandleCommand(button);
	/* Restore menu after returning from the command */
	if (!gGameOn && !gExit) {
		sHoverButton = kNoButton;
	}
}

void Eventloop(void)
{
	Platform_PollEvents();
	Platform_ShowCursor();

	if(Platform_ShouldQuit())
	{
		gExit=true;
		return;
	}

	/* --- Level select cheat: Shift+Enter (must be checked first) --- */
	if ((Platform_IsKeyDown(SDL_SCANCODE_LSHIFT) ||
	     Platform_IsKeyDown(SDL_SCANCODE_RSHIFT)) &&
	    (Platform_IsKeyDown(SDL_SCANCODE_RETURN) ||
	     Platform_IsKeyDown(SDL_SCANCODE_KP_ENTER)))
	{
		extern void StartGame(int lcheat);
		if (sMenuImagesLoaded) {
			BlitButtonRect(kStartGameButton, sMenuPressed);
			Platform_Blit2Screen();
			SimplePlaySound(147);
			SDL_Delay(100);
		}
		StartGame(1);
		if (!gGameOn && !gExit) sHoverButton = kNoButton;
		return;
	}

	/* --- Mouse hover tracking --- */
	if (sMenuImagesLoaded)
	{
		int mx, my, down;
		Platform_GetMouseState(&mx, &my, &down);
		int hover = HitTestButtons(mx, my);

		if (hover != sHoverButton) {
			/* Restore previous button to normal */
			if (sHoverButton != kNoButton)
				BlitButtonRect(sHoverButton, sMenuNormal);
			/* Show hover state for new button */
			if (hover != kNoButton)
				BlitButtonRect(hover, sMenuHover);
			sHoverButton = hover;
			Platform_Blit2Screen();
		}
	}

	/* --- Mouse click on menu buttons --- */
	{
		int mx, my;
		if(Platform_GetMouseClick(&mx, &my))
		{
			int button = HitTestButtons(mx, my);
			if(button != kNoButton)
			{
				ActivateButton(button);
				return;
			}
		}
	}

	/* --- Keyboard shortcuts with visual feedback --- */
	{
		int button = KeyToButton();
		if (button != kNoButton) {
			ActivateButton(button);
			return;
		}
	}
}
