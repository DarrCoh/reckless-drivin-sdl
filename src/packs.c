#include "compat.h"
#include <stdio.h>
#include "resources.h"
#include "lzrw.h"
#include "packs.h"
#include "register.h"
#include "interface.h"

typedef tPackHeader **tPackHandle;

Handle gPacks[kNumPacks];
#define kUnCryptedHeader 256

UInt32 CryptData(UInt32 *data,UInt32 len)
{
	UInt32 check=0;
#ifdef __BIG_ENDIAN__
	UInt32 xorKey=gKey;
#else
	/* The data was encrypted on big-endian PowerPC. The XOR was applied
	   to big-endian 32-bit words in memory.  On little-endian we must
	   byte-swap the key so the XOR produces the same byte pattern. */
	UInt32 xorKey=SWAP32(gKey);
#endif
	data+=kUnCryptedHeader/4;
	len-=kUnCryptedHeader;
	while(len>=4)
	{
		*data^=xorKey;
		check+=*data;
		data++;
		len-=4;
   	}
	if(len)
	{
		UInt8 *bp=(UInt8*)data;
		*bp^=gKey>>24;
		check+=(*bp++)<<24;
		if(len>1)
		{
			*bp^=(gKey>>16)&0xff;
			check+=(*bp++)<<16;
			if(len>2)
			{
				*bp^=(gKey>>8)&0xff;
				check+=(*bp++)<<8;
			}
		}
	}
	return check;
}


UInt32 LoadPack(int num)
{
	UInt32 check=0;
	if(!gPacks[num])
	{
		gPacks[num]=Resources_Get(MakeFourCC("Pack"),num+128);
		if(gPacks[num])
		{
			long compressedSize=Resources_GetSize(gPacks[num]);
			if(num>=kEncryptedPack||gLevelResFile)
				check=CryptData((UInt32*)*gPacks[num],compressedSize);
			LZRWDecodeHandle(&gPacks[num],compressedSize);
		}
	}
	return check;
}

int CheckPack(int num,UInt32 check)
{
	int ok=false;
	/* We only have one resource file in the SDL port, no UseResFile needed */
	if(!gPacks[num])
	{
		gPacks[num]=Resources_Get(MakeFourCC("Pack"),num+128);
		if(gPacks[num])
		{
			if(num>=kEncryptedPack)
				ok=check==CryptData((UInt32*)*gPacks[num],Resources_GetSize(gPacks[num]));
			Resources_Release(gPacks[num]);
			gPacks[num]=nil;
		}
	}
	return ok;
}

void UnloadPack(int num)
{
	if(gPacks[num])
	{
		Resources_Release(gPacks[num]);
		gPacks[num]=nil;
	}
}

Ptr GetSortedPackEntry(int packNum,int entryID,int *size)
{
	tPackHeader *pack=(tPackHeader*)*gPacks[packNum];
	int count=SWAP16S(pack[0].id);
	int startId=SWAP16S(pack[1].id);
	int idx=entryID-startId+1;
	UInt32 offs;
	if(idx<1||idx>count) {
		static int warnCount=0;
		if(warnCount<10) {
			fprintf(stderr, "GetSortedPackEntry: pack %d, entryID %d out of range (startId=%d, count=%d, idx=%d)\n",
				packNum, entryID, startId, count, idx);
			warnCount++;
		}
		if(size) *size=0;
		return NULL;
	}
	offs=SWAP32(pack[idx].offs);
	if(size)
		if(entryID-startId+1==count)
			*size=Resources_GetSize(gPacks[packNum])-offs;
		else
			*size=SWAP32(pack[entryID-startId+2].offs)-offs;
	return (Ptr)pack+offs;
}

int ComparePackHeaders(const void *v1,const void *v2)
{
	const tPackHeader *p1=(const tPackHeader*)v1;
	const tPackHeader *p2=(const tPackHeader*)v2;
	return SWAP16S(p1->id)-SWAP16S(p2->id);
}

Ptr GetUnsortedPackEntry(int packNum,int entryID,int *size)
{
	tPackHeader *pack=(tPackHeader*)*gPacks[packNum];
	tPackHeader key,*found;
	UInt32 offs;
	int count=SWAP16S(pack->id);
	key.id=SWAP16S((SInt16)entryID);
	found=(tPackHeader*)bsearch(&key,pack+1,count,sizeof(tPackHeader),ComparePackHeaders);
	if(found)
	{
		offs=SWAP32(found->offs);
		if(size)
			if(count==found-pack)
				*size=Resources_GetSize(gPacks[packNum])-offs;
			else
				*size=SWAP32((found+1)->offs)-offs;
		return (Ptr)pack+offs;
	}
	else return 0;
}

int NumPackEntries(int num)
{
	if(gPacks[num])
		return SWAP16S((**(tPackHandle)gPacks[num]).id);
	return 0;
}
