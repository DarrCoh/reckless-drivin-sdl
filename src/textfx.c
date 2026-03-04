#include "compat.h"
#include <string.h>
#include "screen.h"
#include "objects.h"
#include "textfx.h"
#include "trig.h"
#include "renderframe.h"
#include "packs.h"
#include "preferences.h"

#define kMaxFX	10
#define kEndShapeToken		0				// the end of shape maker
#define kLineStartToken		1				// the line start marker
#define kDrawPixelsToken	2				// the draw run marker
#define kSkipPixelsToken	3				// the skip pixels marker
#define kCharSize 32

/* Read big-endian RLE token: byte0=type, bytes1-3=24-bit data */
static inline void ReadToken(UInt8 *ptr, UInt8 *tokenType, UInt32 *tokenData)
{
	*tokenType = ptr[0];
	*tokenData = ((UInt32)ptr[1] << 16) | ((UInt32)ptr[2] << 8) | (UInt32)ptr[3];
}

/* Pad to next 4-byte boundary */
static inline int Pad4(int n) { return (n+3) & ~3; }
#define kExplAccel 10.0
#define kEffectAccel 150.0
#define kEffectTimeOut 2.0
#define kEffectSinAmp	30.0
#define kEffectSinFreq	(2.0/kCharSize)
#define kEffectSinVelo	(2*PI)

tTextEffect gTextFX[kMaxFX];
int gFXCount=0;

void NewTextEffect(tTextEffect *effect)
{
	if(gFXCount<kMaxFX)
	{
		gTextFX[gFXCount]=*effect;
		gTextFX[gFXCount].fxStartFrame=gFrameCount;
		gFXCount++;
	}
}

void MakeFXStringFromNumStr(Str31 numStr,Str31 fxStr)
{
	int i;
	for(i=0;i<numStr[0];i++)
		fxStr[i+1]=numStr[i+1]+0x2b;
	fxStr[0]=numStr[0];
}

void DrawZoomedCharLine8(UInt8 **data,SInt32 x,SInt32 y,UInt32 zoom)
{
	UInt8 *spritePos=*data;
	UInt8 *lineBase=gBaseAddr+y*gRowBytes;
	int stop=0;
	if((y<0)||(y>=gYSize-kInvLines)) goto noDrawZoomed;
	if(((x>>16)+(int)(zoom>>11)<0)||((x>>16)>=gXSize)) goto noDrawZoomed;
	if(((x>>16)<0)||((x>>16)+(int)(zoom>>11)>=gXSize)) goto noDrawZoomed;
	while(!stop)
	{
		UInt8 tokenType; UInt32 tokenData;
		ReadToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				{
					UInt32 i=0;
					UInt8 *src=spritePos+4;
					spritePos+=4+Pad4((int)tokenData);
					while(i<tokenData)
					{
						UInt8 *dst=lineBase+(x>>16);
						*dst=*(src+i);
						i++;
						x+=zoom;
					}
				}
				break;
			case kSkipPixelsToken:
				x+=tokenData*zoom;
				spritePos+=4;
				break;
			case kLineStartToken:
			case kEndShapeToken:
				stop=true;
				break;
		}
	}
noDrawZoomed:
	while(!stop)
	{
		UInt8 tokenType; UInt32 tokenData;
		ReadToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				spritePos+=4+Pad4((int)tokenData);
				break;
			case kSkipPixelsToken:
				spritePos+=4;
				break;
			case kLineStartToken:
			case kEndShapeToken:
				stop=true;
				break;
		}
	}
	*data=spritePos+4;
}

void DrawCharLine8(UInt8 **data,SInt32 x,SInt32 y)
{
	UInt8 *spritePos=*data;
	UInt8 *dst=gBaseAddr+x+y*gRowBytes;
	int stop=0;
	if((y<0)||(y>=gYSize-kInvLines)) goto noDraw;
	if((x+kCharSize<0)||(x>=gXSize)) goto noDraw;
	if((x<0)||(x+kCharSize>=gXSize)) goto noDraw;
	do
	{
		UInt8 tokenType; UInt32 tokenData;
		ReadToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				{
					UInt8 *src=spritePos+4;
					int byteCount=(int)tokenData;
					spritePos+=4+Pad4(byteCount);
					memcpy(dst, src, byteCount);
					dst+=byteCount;
				}
				break;
			case kSkipPixelsToken:
				dst+=tokenData;
				spritePos+=4;
				break;
			case kLineStartToken:
			case kEndShapeToken:
				stop=true;
				break;
		}
	}
	while (!stop);
noDraw:
	do
	{
		UInt8 tokenType; UInt32 tokenData;
		ReadToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				spritePos+=4+Pad4((int)tokenData);
				break;
			case kSkipPixelsToken:
				spritePos+=4;
				break;
			case kLineStartToken:
			case kEndShapeToken:
				stop=true;
				break;
		}
	}
	while (!stop);
	*data=spritePos+4;
}

void DrawZoomedCharLine16(UInt8 **data,SInt32 x,SInt32 y,UInt32 zoom)
{
	UInt8 *spritePos=*data;
	UInt8 *lineBase=gBaseAddr+y*gRowBytes;
	int stop=0;
	if((y<0)||(y>=gYSize-kInvLines)) goto noDrawZoomed;
	if(((x>>16)+(int)(zoom>>11)<0)||((x>>16)>=gXSize)) goto noDrawZoomed;
	if(((x>>16)<0)||((x>>16)+(int)(zoom>>11)>=gXSize)) goto noDrawZoomed;
	while(!stop)
	{
		UInt8 tokenType; UInt32 tokenData;
		ReadToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				{
					UInt32 i=0;
					UInt16 *src=(UInt16*)(spritePos+4);
					int byteCount=(int)tokenData*2;
					spritePos+=4+Pad4(byteCount);
					while(i<tokenData)
					{
						UInt16 *dst=(UInt16*)(lineBase+((x>>15)&0xfffe));
						*dst=SWAP16(*(src+i));
						i++;
						x+=zoom;
					}
				}
				break;
			case kSkipPixelsToken:
				x+=tokenData*zoom;
				spritePos+=4;
				break;
			case kLineStartToken:
			case kEndShapeToken:
				stop=true;
				break;
		}
	}
noDrawZoomed:
	while(!stop)
	{
		UInt8 tokenType; UInt32 tokenData;
		ReadToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				spritePos+=4+Pad4((int)tokenData*2);
				break;
			case kSkipPixelsToken:
				spritePos+=4;
				break;
			case kLineStartToken:
			case kEndShapeToken:
				stop=true;
				break;
		}
	}
	*data=spritePos+4;
}

void DrawCharLine16(UInt8 **data,SInt32 x,SInt32 y)
{
	UInt8 *spritePos=*data;
	UInt16 *dst=(UInt16*)(gBaseAddr+2*x+y*gRowBytes);
	int stop=0;
	if((y<0)||(y>=gYSize-kInvLines)) goto noDraw;
	if((x+kCharSize<0)||(x>=gXSize)) goto noDraw;
	if((x<0)||(x+kCharSize>=gXSize)) goto noDraw;
	do
	{
		UInt8 tokenType; UInt32 tokenData;
		ReadToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				{
					UInt16 *src=(UInt16*)(spritePos+4);
					int pixelCount=(int)tokenData;
					int byteCount=pixelCount*2;
					int p;
					spritePos+=4+Pad4(byteCount);
					for(p=0;p<pixelCount;p++)
						dst[p]=SWAP16(src[p]);
					dst+=pixelCount;
				}
				break;
			case kSkipPixelsToken:
				dst+=tokenData;
				spritePos+=4;
				break;
			case kLineStartToken:
			case kEndShapeToken:
				stop=true;
				break;
		}
	}
	while (!stop);
noDraw:
	do
	{
		UInt8 tokenType; UInt32 tokenData;
		ReadToken(spritePos, &tokenType, &tokenData);
		switch (tokenType)
		{
			case kDrawPixelsToken:
				spritePos+=4+Pad4((int)tokenData*2);
				break;
			case kSkipPixelsToken:
				spritePos+=4;
				break;
			case kLineStartToken:
			case kEndShapeToken:
				stop=true;
				break;
		}
	}
	while (!stop);
	*data=spritePos+4;
}

static inline void DrawZoomedCharLine(UInt8 **data,SInt32 x,SInt32 y,UInt32 zoom)
{
	if(gPrefs.hiColor)
		DrawZoomedCharLine16(data,x,y,zoom);
	else
		DrawZoomedCharLine8(data,x,y,zoom);	
}

static inline void DrawCharLine(UInt8 **data,SInt32 x,SInt32 y)
{
	if(gPrefs.hiColor)
		DrawCharLine16(data,x,y);
	else
		DrawCharLine8(data,x,y);	
}


void DrawTextFX(int xDrawStart,int yDrawStart)
{
	int i,line,ch;
	for(i=0;i<gFXCount;i++)
	{
		float dt=(gFrameCount-gTextFX[i].fxStartFrame)*kFrameDuration*(gTextFX[i].effectFlags&kEffectTiny?3:1);
		float exploZoom=(gTextFX[i].effectFlags&kEffectExplode?1+0.5*dt*dt*kExplAccel:1)*(gTextFX[i].effectFlags&kEffectTiny?0.25:1);
		float baseX=gTextFX[i].x-gTextFX[i].text[0]*kCharSize*0.5*exploZoom;
		float baseY=gTextFX[i].y-kCharSize*0.5*exploZoom;
		if(gTextFX[i].effectFlags&kEffectAbsPos)
		{
			baseY=yDrawStart-gTextFX[i].y-baseY;
			baseX-=xDrawStart;
			gTextFX[i].x-=xDrawStart;
			gTextFX[i].y=yDrawStart-gTextFX[i].y;
			gTextFX[i].effectFlags^=kEffectAbsPos;
		}
		if(gTextFX[i].effectFlags&kEffectMoveUp)
			baseY-=0.5*dt*dt*kEffectAccel;
		else if(gTextFX[i].effectFlags&kEffectMoveDown)
			baseY+=0.5*dt*dt*kEffectAccel;
		if(gTextFX[i].effectFlags&kEffectMoveLeft)
			baseX-=0.5*dt*dt*kEffectAccel;
		else if(gTextFX[i].effectFlags&kEffectMoveRight)
			baseX+=0.5*dt*dt*kEffectAccel;
		for(ch=1;ch<=gTextFX[i].text[0];ch++)
		{
			Ptr rawCH=GetSortedPackEntry(kPackcRLE,gTextFX[i].text[ch]-'A'+128,nil);
			float y=baseY;
			if(!rawCH){baseX+=exploZoom*kCharSize;continue;}
			Ptr theCH=rawCH+8;
			for(line=0;line<kCharSize;line++)
			{
				float x=baseX+((gTextFX[i].effectFlags&kEffectSinLines)?sin(line*kEffectSinFreq+dt*kEffectSinVelo)*kEffectSinAmp*exploZoom:0);
				if(exploZoom!=1)
					DrawZoomedCharLine(&theCH,(SInt32)(x*65536.0),y,(SInt32)(exploZoom*65536.0));
				else
					DrawCharLine(&theCH,(SInt32)(x),(SInt32)(y));
				y+=exploZoom;
			}
			baseX+=exploZoom*kCharSize;
		}
		if(dt*(gTextFX[i].effectFlags&kEffectTiny?2:1)>=kEffectTimeOut)
		{
			if(gFXCount>i+1)
				BlockMoveData(gTextFX+i+1,gTextFX+i,(gFXCount-i-1)*sizeof(tTextEffect));
			gFXCount--;
		}
	}
}

void DrawTextFXZoomed(float xDrawStart,float yDrawStart,float zoom)
{
	int i,line,ch;
	for(i=0;i<gFXCount;i++)
	{
		float dt=(gFrameCount-gTextFX[i].fxStartFrame)*kFrameDuration*(gTextFX[i].effectFlags&kEffectTiny?3:1);
		float exploZoom=(gTextFX[i].effectFlags&kEffectExplode?1+0.5*dt*dt*kExplAccel:1)*(gTextFX[i].effectFlags&kEffectTiny?0.25:1);
		float baseX=gTextFX[i].x-gTextFX[i].text[0]*kCharSize*0.5*exploZoom;
		float baseY=gTextFX[i].y-kCharSize*0.5*exploZoom;
		if(gTextFX[i].effectFlags&kEffectAbsPos)
		{
			baseY=(yDrawStart-baseY)/zoom;
			baseX=(baseX-xDrawStart)/zoom;
			gTextFX[i].x=(gTextFX[i].x-xDrawStart)/zoom;
			gTextFX[i].y=(yDrawStart-gTextFX[i].y)/zoom;
			gTextFX[i].effectFlags^=kEffectAbsPos;
		}
		if(gTextFX[i].effectFlags&kEffectMoveUp)
			baseY-=0.5*dt*dt*kEffectAccel;
		else if(gTextFX[i].effectFlags&kEffectMoveDown)
			baseY+=0.5*dt*dt*kEffectAccel;
		if(gTextFX[i].effectFlags&kEffectMoveLeft)
			baseX-=0.5*dt*dt*kEffectAccel;
		else if(gTextFX[i].effectFlags&kEffectMoveRight)
			baseX+=0.5*dt*dt*kEffectAccel;
		for(ch=1;ch<=gTextFX[i].text[0];ch++)
		{
			Ptr rawCH=GetSortedPackEntry(gPrefs.hiColor?kPackcR16:kPackcRLE,gTextFX[i].text[ch]-'A'+128,nil);
			float y=baseY;
			if(!rawCH){baseX+=exploZoom*kCharSize;continue;}
			Ptr theCH=rawCH+8;
			for(line=0;line<kCharSize;line++)
			{
				float x=baseX+((gTextFX[i].effectFlags&kEffectSinLines)?sin(line*kEffectSinFreq+dt*kEffectSinVelo)*kEffectSinAmp*exploZoom:0);
				if(exploZoom!=1)
					DrawZoomedCharLine(&theCH,(SInt32)(x*65536.0),y,(SInt32)(exploZoom*65536.0));
				else
					DrawCharLine(&theCH,(SInt32)(x),(SInt32)(y));
				y+=exploZoom;
			}
			baseX+=exploZoom*kCharSize;
		}
		if(dt*(gTextFX[i].effectFlags&kEffectTiny?2:1)>=kEffectTimeOut)
		{
			if(gFXCount>i+1)
				BlockMoveData(gTextFX+i+1,gTextFX+i,(gFXCount-i-1)*sizeof(tTextEffect));
			gFXCount--;
		}
	}
}

void SimpleDrawText(Str255 text,int xPos,int yPos)
{
	int ch,line;
	for(ch=1;ch<=text[0];ch++)
	{
		Ptr rawCH=GetSortedPackEntry(gPrefs.hiColor?kPackcR16:kPackcRLE,text[ch]-'A'+128,nil);
		int y=yPos;
		if(!rawCH){xPos+=kCharSize/2;continue;}
		Ptr theCH=rawCH+8;
		for(line=0;line<kCharSize/2;line++)
		{
			DrawZoomedCharLine(&theCH,xPos<<16,y,1<<15);
			DrawZoomedCharLine(&theCH,xPos<<16,y,1<<15);
			y++;				
		}
		xPos+=kCharSize/2;
	}
}

void ClearTextFX()
{
	gFXCount=0;	
}