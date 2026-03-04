#include "compat.h"
#include <stdio.h>
#include <string.h>
#include "screen.h"
#include "platform.h"
#include "packs.h"
#include "error.h"
#include "preferences.h"

#define kEndShapeToken		0				// the end of shape maker
#define kLineStartToken		1				// the line start marker
#define kDrawPixelsToken	2				// the draw run marker
#define kSkipPixelsToken	3				// the skip pixels marker

/* Note: The RLE data from packs is big-endian. The token byte is the first byte
   of a 32-bit word, and tokenData is the lower 24 bits (big-endian).
   On little-endian, we need to read these correctly. */

static inline UInt32 ReadRLEToken(UInt8 *ptr, UInt8 *tokenType, UInt32 *tokenData)
{
	/* Original Mac (big-endian) format: byte0=token, bytes1-3=data (24-bit) */
	*tokenType = ptr[0];
	*tokenData = ((UInt32)ptr[1] << 16) | ((UInt32)ptr[2] << 8) | (UInt32)ptr[3];
	return *tokenData;
}

/* Skip past the Rect structure at the start of RLE data (8 bytes) */
#define kRectSize 8

/* Pad byte count up to next 4-byte boundary */
static inline int RLEPad4(int n) { return (n+3) & ~3; }

void DrawRLE8(int h,int v,int id)
{
	int rowBytes=gRowBytes;
	UInt8 *spritePos=GetSortedPackEntry(kPacksRLE,id,nil);
	UInt8 *lineStart;
	UInt8 *dst;
	int stop=0;
	if(!spritePos) return;
	spritePos+=kRectSize;
	lineStart=gBaseAddr+h+v*rowBytes;
	dst=lineStart;
	do
	{
		UInt8 tokenType;
		UInt32 tokenData;
		ReadRLEToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				{
					UInt8 *src=spritePos+4;
					int byteCount=(int)tokenData;
					spritePos+=4+RLEPad4(byteCount);
					memcpy(dst, src, byteCount);
					dst+=byteCount;
				}
				break;
			case kSkipPixelsToken:
				dst+=tokenData;
				spritePos+=4;
				break;
			case kLineStartToken:
				lineStart+=rowBytes;
				dst=lineStart;
				spritePos+=4;
				break;
			case kEndShapeToken:
				stop=true;
				break;
			default:
				stop=true;
		}
	}
	while (!stop);
}

void DrawRLE16(int h,int v,int id)
{
	int rowBytes=gRowBytes;
	UInt8 *spritePos=GetSortedPackEntry(kPacksR16,id,nil);
	UInt8 *lineStart;
	UInt16 *dst;
	UInt8 *bufEnd=(UInt8*)gBaseAddr+gRowBytes*(gYSize+FRAMEBUFFER_GUARD_ROWS);
	int stop=0;
	if(!spritePos) return;
	spritePos+=kRectSize;
	lineStart=gBaseAddr+h*2+v*rowBytes;
	dst=(UInt16*)lineStart;
	do
	{
		UInt8 tokenType;
		UInt32 tokenData;
		ReadRLEToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				{
					UInt16 *src=(UInt16*)(spritePos+4);
					int pixelCount=(int)tokenData;
					int byteCount=pixelCount*2;
					spritePos+=4+RLEPad4(byteCount);
					if((UInt8*)dst >= gBaseAddr && (UInt8*)(dst+pixelCount) <= bufEnd)
					{
						int p;
						for(p=0;p<pixelCount;p++)
							dst[p]=SWAP16(src[p]);
					}
					dst+=pixelCount;
				}
				break;
			case kSkipPixelsToken:
				dst+=tokenData;
				spritePos+=4;
				break;
			case kLineStartToken:
				lineStart+=rowBytes;
				dst=(UInt16*)lineStart;
				spritePos+=4;
				break;
			case kEndShapeToken:
				stop=true;
				break;
			default:
				stop=true;
		}
	}
	while (!stop);
}

void DrawRLEYClip8(int h,int v,int id)
{
	int rowBytes=gRowBytes;
	UInt8 *spritePos=GetSortedPackEntry(kPacksRLE,id,nil);
	UInt8 *lineStart;
	UInt8 *dst;
	int stop=0;
	if(!spritePos) return;
	spritePos+=kRectSize;
	lineStart=gBaseAddr+h+v*rowBytes;
	dst=lineStart;
	do
	{
		UInt8 tokenType;
		UInt32 tokenData;
		ReadRLEToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				{
					UInt8 *src=spritePos+4;
					int byteCount=(int)tokenData;
					spritePos+=4+RLEPad4(byteCount);
					if(v>=0)
						memcpy(dst, src, byteCount);
					dst+=byteCount;
				}
				break;
			case kSkipPixelsToken:
				dst+=tokenData;
				spritePos+=4;
				break;
			case kLineStartToken:
				lineStart+=rowBytes;
				dst=lineStart;
				spritePos+=4;
				v++;
				if(v>=gYSize)return;
				break;
			case kEndShapeToken:
				stop=true;
				break;
			default:
				stop=true;
		}
	}
	while (!stop);
}

void DrawRLEYClip16(int h,int v,int id)
{
	int rowBytes=gRowBytes;
	UInt8 *spritePos=GetSortedPackEntry(kPacksR16,id,nil);
	UInt8 *lineStart;
	UInt16 *dst;
	int stop=0;
	if(!spritePos) return;
	spritePos+=kRectSize;
	lineStart=gBaseAddr+h*2+v*rowBytes;
	dst=(UInt16*)lineStart;
	do
	{
		UInt8 tokenType;
		UInt32 tokenData;
		ReadRLEToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				{
					UInt16 *src=(UInt16*)(spritePos+4);
					int pixelCount=(int)tokenData;
					int byteCount=pixelCount*2;
					spritePos+=4+RLEPad4(byteCount);
					if(v>=0)
					{
						int p;
						for(p=0;p<pixelCount;p++)
							dst[p]=SWAP16(src[p]);
					}
					dst+=pixelCount;
				}
				break;
			case kSkipPixelsToken:
				dst+=tokenData;
				spritePos+=4;
				break;
			case kLineStartToken:
				lineStart+=rowBytes;
				dst=(UInt16*)lineStart;
				spritePos+=4;
				v++;
				if(v>=gYSize)return;
				break;
			case kEndShapeToken:
				stop=true;
				break;
			default:
				stop=true;
		}
	}
	while (!stop);
}

void DrawRLE(int h,int v,int id)
{
	if(gPrefs.hiColor)
		 DrawRLE16(h,v,id);
	else
		 DrawRLE8(h,v,id);
}

void DrawRLEYClip(int h,int v,int id)
{
	if(gPrefs.hiColor)
		 DrawRLEYClip16(h,v,id);
	else
		 DrawRLEYClip8(h,v,id);
}
