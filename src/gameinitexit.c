#include "compat.h"
#include "input.h"
#include "screen.h"
#include "gameframe.h"
#include "roads.h"
#include "objects.h"
#include "error.h"
#include "trig.h"
#include "gamesounds.h"
#include "renderframe.h"
#include "interface.h"
#include "screenfx.h"
#include "textfx.h"
#include "sprites.h"
#include "packs.h"
#include "high.h"
#include "register.h"
#include "preferences.h"
#include "gamesounds.h"
#include "textrender.h"
#include "platform.h"
#include "sprites.h"

tRoad gRoadData;
UInt32 *gRoadLenght;
tRoadInfo *gRoadInfo;
tLevelData *gLevelData;
tTrackInfo *gTrackUp,*gTrackDown;
tMarkSeg *gMarks;
int gMarkSize;
int gLevelID;
tObject *gFirstObj,*gCameraObj,*gPlayerObj,*gSpikeObj,*gBrakeObj,*gFirstVisObj,*gLastVisObj;
tTrackSeg gTracks[kMaxTracks];
int gTrackCount;
int gPlayerLives,gExtraLives;
int gNumMissiles,gNumMines;
float gPlayerDeathDelay,gFinishDelay;
int gPlayerScore,gDisplayScore;
int gPlayerBonus;
UInt32 gPlayerAddOns;
float gGameTime;
float gXDriftPos,gYDriftPos,gXFrontDriftPos,gYFrontDriftPos,gZoomVelo;
int gGameOn;
int gPlayerCarID;
float gPlayerSlide[4]={0,0,0,0};
float gSpikeFrame;
int gLCheat;

int abs(int x);
void CopClear();

/* Byte-swap a tLevelData struct loaded from big-endian pack */
static void SwapLevelData(tLevelData *ld)
{
#ifndef __BIG_ENDIAN__
	int i;
	ld->roadInfo = SWAP16S(ld->roadInfo);
	ld->time = SWAP16(ld->time);
	for(i=0;i<10;i++)
	{
		ld->objGrps[i].resID = SWAP16S(ld->objGrps[i].resID);
		ld->objGrps[i].numObjs = SWAP16S(ld->objGrps[i].numObjs);
	}
	ld->xStartPos = SWAP16S(ld->xStartPos);
	ld->levelEnd = SWAP16(ld->levelEnd);
#endif
}

/* Byte-swap a tRoadInfo struct loaded from big-endian pack */
void SwapRoadInfo(tRoadInfo *ri)
{
#ifndef __BIG_ENDIAN__
	ri->friction = SWAPFloat(ri->friction);
	ri->airResistance = SWAPFloat(ri->airResistance);
	ri->backResistance = SWAPFloat(ri->backResistance);
	ri->tolerance = SWAP16(ri->tolerance);
	ri->marks = SWAP16S(ri->marks);
	ri->deathOffs = SWAP16S(ri->deathOffs);
	ri->backgroundTex = SWAP16S(ri->backgroundTex);
	ri->foregroundTex = SWAP16S(ri->foregroundTex);
	ri->roadLeftBorder = SWAP16S(ri->roadLeftBorder);
	ri->roadRightBorder = SWAP16S(ri->roadRightBorder);
	ri->tracks = SWAP16S(ri->tracks);
	ri->skidSound = SWAP16S(ri->skidSound);
	ri->filler = SWAP16S(ri->filler);
	ri->xDrift = SWAPFloat(ri->xDrift);
	ri->yDrift = SWAPFloat(ri->yDrift);
	ri->xFrontDrift = SWAPFloat(ri->xFrontDrift);
	ri->yFrontDrift = SWAPFloat(ri->yFrontDrift);
	ri->trackSlide = SWAPFloat(ri->trackSlide);
	ri->dustSlide = SWAPFloat(ri->dustSlide);
	/* dustColor and water are UInt8, no swap needed */
	ri->filler2 = SWAP16(ri->filler2);
	ri->slideFriction = SWAPFloat(ri->slideFriction);
#endif
}

/* Byte-swap all road info entries in the kPackRoad pack after loading */
void SwapAllRoadInfo(void)
{
#ifndef __BIG_ENDIAN__
	tPackHeader *pack = (tPackHeader*)*gPacks[kPackRoad];
	int count = SWAP16S(pack[0].id);
	int startId = SWAP16S(pack[1].id);
	int i;
	for(i=0;i<count;i++)
	{
		tRoadInfo *ri = (tRoadInfo*)GetSortedPackEntry(kPackRoad, startId+i, nil);
		if(ri) SwapRoadInfo(ri);
	}
#endif
}

/* Byte-swap a tTrackInfo and all its segments */
static void SwapTrackInfo(tTrackInfo *ti)
{
#ifndef __BIG_ENDIAN__
	UInt32 i;
	ti->num = SWAP32(ti->num);
	for(i=0;i<ti->num;i++)
	{
		ti->track[i].flags = SWAP16(ti->track[i].flags);
		ti->track[i].x = SWAP16S(ti->track[i].x);
		ti->track[i].y = SWAP32S(ti->track[i].y);
		ti->track[i].velo = SWAPFloat(ti->track[i].velo);
	}
#endif
}

/* Byte-swap a tObjectPos array loaded from pack */
static void SwapObjectPositions(tObjectPos *objs, int count)
{
#ifndef __BIG_ENDIAN__
	int i;
	for(i=0;i<count;i++)
	{
		objs[i].x = SWAP32S(objs[i].x);
		objs[i].y = SWAP32S(objs[i].y);
		objs[i].dir = SWAPFloat(objs[i].dir);
		objs[i].typeRes = SWAP16S(objs[i].typeRes);
		objs[i].filler = SWAP16S(objs[i].filler);
	}
#endif
}

/* Byte-swap mark segments */
static void SwapMarks(tMarkSeg *marks, int count)
{
#ifndef __BIG_ENDIAN__
	int i;
	for(i=0;i<count;i++)
	{
		union { float f; uint32_t u; } cx, cy, cx2, cy2;
		cx.f = marks[i].p1.x; cx.u = SWAP32(cx.u); marks[i].p1.x = cx.f;
		cy.f = marks[i].p1.y; cy.u = SWAP32(cy.u); marks[i].p1.y = cy.f;
		cx2.f = marks[i].p2.x; cx2.u = SWAP32(cx2.u); marks[i].p2.x = cx2.f;
		cy2.f = marks[i].p2.y; cy2.u = SWAP32(cy2.u); marks[i].p2.y = cy2.f;
	}
#endif
}

/* Byte-swap road data (array of tRoadSeg = SInt16[4]) */
static void SwapRoadData(tRoadSeg *data, int count)
{
#ifndef __BIG_ENDIAN__
	int i;
	for(i=0;i<count;i++)
	{
		data[i][0] = SWAP16S(data[i][0]);
		data[i][1] = SWAP16S(data[i][1]);
		data[i][2] = SWAP16S(data[i][2]);
		data[i][3] = SWAP16S(data[i][3]);
	}
#endif
}

Ptr LoadObjs(Ptr dataPos)
{
	int i;
	UInt32 numObjs = *(UInt32*)dataPos;
#ifndef __BIG_ENDIAN__
	numObjs = SWAP32(numObjs);
	*(UInt32*)dataPos = numObjs;
#endif
	tObjectPos *objs=(tObjectPos*)(dataPos+sizeof(UInt32));
	SwapObjectPositions(objs, numObjs);
	for(i=0;i<numObjs;i++)
	{
		tObject *theObj=NewObject(gFirstObj,objs[i].typeRes);
		theObj->dir=objs[i].dir;
		theObj->pos.x=objs[i].x;
		theObj->pos.y=objs[i].y;
	}
	return (Ptr)(objs+numObjs);
}

int NumLevels()
{
	/* The game has 10 levels (kPackLevel1 through kPackLevel10) */
	return 10;
}

void GameEndSequence();

int LoadLevel()
{
	int i,sound;
	if(gLevelID>=kEncryptedPack-kPackLevel1||gLevelResFile)
		if(!gRegistered)
		{
			ShowPicScreen(1005);
			WaitForPress();
			BeQuiet();
			InitInterface();
			/* ShowCursor() - handled by platform layer */
			if(!gLCheat)
				CheckHighScore(gPlayerScore);
			return false;
		}

	gFirstObj=(tObject*)NewPtrClear(sizeof(tObject));
	gFirstObj->next=gFirstObj;
	gFirstObj->prev=gFirstObj;

	if(gLevelID>=NumLevels())
	{
		GameEndSequence();
		gLevelID=0;
	}

	LoadPack(kPackLevel1+gLevelID);
	gLevelData=(tLevelData*)GetSortedPackEntry(kPackLevel1+gLevelID,1,nil);
	SwapLevelData(gLevelData);
	gMarks=(tMarkSeg*)GetSortedPackEntry(kPackLevel1+gLevelID,2,&gMarkSize);
	gMarkSize/=sizeof(tMarkSeg);
	SwapMarks(gMarks, gMarkSize);
	gRoadInfo=(tRoadInfo*)GetSortedPackEntry(kPackRoad,gLevelData->roadInfo,nil);
	/* Road info already byte-swapped at pack load time by SwapAllRoadInfo() */
	gTrackUp=(tTrackInfo*)((Ptr)gLevelData+sizeof(tLevelData));
	SwapTrackInfo(gTrackUp);
	gTrackDown=(tTrackInfo*)((Ptr)gTrackUp+sizeof(UInt32)+gTrackUp->num*sizeof(tTrackInfoSeg));
	SwapTrackInfo(gTrackDown);
	gRoadLenght=(UInt32*)LoadObjs((Ptr)gTrackDown+sizeof(UInt32)+gTrackDown->num*sizeof(tTrackInfoSeg));
#ifndef __BIG_ENDIAN__
	*gRoadLenght = SWAP32(*gRoadLenght);
#endif
	gRoadData=(tRoad)((Ptr)gRoadLenght+sizeof(UInt32));
	SwapRoadData((tRoadSeg*)gRoadData, *gRoadLenght);

	for(i=0;i<9;i++)
		if((*gLevelData).objGrps[i].resID)
			InsertObjectGroup((*gLevelData).objGrps[i]);

	gPlayerObj=NewObject(gFirstObj,gRoadInfo->water?kNormalPlayerBoatID:gPlayerCarID);
	gPlayerObj->pos.x=gLevelData->xStartPos;
	gPlayerObj->pos.y=500;
	gPlayerObj->control=kObjectDriveUp;
	gPlayerObj->target=1;
	gCameraObj=gPlayerObj;
	gPlayerBonus=1;
//	gPlayerObj=nil; //	Uncomment this line to make the player car ai controlled
	gSpikeObj=nil;
	gBrakeObj=nil;
	CopClear();
	SortObjects();

	gGameTime=0;
	gTrackCount=0;
	gPlayerDeathDelay=0;
	gFinishDelay=0;
	gPlayerBonus=1;
	gDisplayScore=gPlayerScore;
	gXDriftPos=0;
	gYDriftPos=0;
	gXFrontDriftPos=0;
	gYFrontDriftPos=0;
	gZoomVelo=kMaxZoomVelo;
	ClearTextFX();
	StartCarChannels();
	gScreenBlitSpecial=true;
	return true;
}

void DisposeLevel()
{
	UnloadPack(kPackLevel1+gLevelID);
	gPlayerObj=nil;
	while((tObject*)gFirstObj->next!=gFirstObj)
	{
		SpriteUnused((*(tObject*)gFirstObj->next).frame);
		RemoveObject((tObject*)gFirstObj->next);
	}
	DisposePtr((Ptr)gFirstObj);
}

extern int gOSX;

/* Level metadata — names and descriptions derived from road properties */
typedef struct {
	const char *name;
	const char *desc;
	int timeSec;
} LevelInfo;

static const LevelInfo kLevelInfo[10] = {
	{"Highway",           "Smooth asphalt, easy traffic",             150},
	{"Coastal Road",      "Windy seaside drive with drift",           150},
	{"Dirt Track",        "Low grip gravel, watch your slide",        180},
	{"Highway II",        "Back on asphalt, heavier traffic",         150},
	{"The River",         "Grab the boat! Water level",               180},
	{"Coastal Road II",   "Stronger winds, more cops",                150},
	{"Ice Road",          "Frozen surface, minimal traction",         180},
	{"Night Drive",       "Dark roads, stay sharp",                   150},
	{"Mountain Pass",     "Winding roads, mixed surface",             180},
	{"The Gauntlet",      "Low grip, long haul, everything at once",  240},
};

/* Vehicle metadata */
typedef struct {
	int id;
	const char *name;
	const char *desc;
} VehicleInfo;

static const VehicleInfo kVehicles[] = {
	{128, "Sports Car",       "All-rounder, good grip and speed"},
	{133, "Muscle Car",       "Light and fast, burns rubber"},
	{129, "Sedan",            "Heavy, steady, reliable"},
	{137, "Station Wagon",    "Family car with some punch"},
	{135, "Compact",          "Tiny and nimble, fragile"},
	{138, "Coupe",            "Quick and light, low profile"},
	{147, "Buggy",            "Small and zippy, watch the bumps"},
	{140, "Motorcycle",       "Fast, light, easily wrecked"},
	{141, "Sport Bike",       "Quick but fragile"},
	{156, "Police Car",       "Cop interceptor, serious power"},
	{155, "Police Bike",      "Cop motorcycle, fast but light"},
	{157, "Police Chopper",   "Helicopter! Flies over everything"},
	{142, "Semi Truck",       "22 tons of unstoppable force"},
	{143, "Delivery Truck",   "Big, slow, hard to steer"},
	{144, "Bus",              "16 tons, wide turns"},
	{248, "Monster Truck",    "30 tons, insane engine power"},
	{217, "Tank",             "36 tons with a cannon!"},
	{179, "APC",              "Armored, fast, tight turning"},
	{218, "Helicopter",       "Flies above the road"},
	{201, "Speedboat",        "For water levels, handles waves"},
	{203, "Cargo Ship",       "Big and slow on the water"},
	{204, "Barge",            "Massive, barely steers"},
	{208, "Police Boat",      "Cop speedboat, fast on water"},
	{212, "Armed Patrol Boat","Cop boat with missiles!"},
};
#define kNumVehicles (sizeof(kVehicles) / sizeof(kVehicles[0]))

void GetLevelNumber()
{
	int rowBytes;
	UInt16 *fb = Platform_GetFramebuffer(&rowBytes);
	int fbStride = rowBytes / 2;
	int level = gLevelID + 1;  /* 1-based display */
	int vehIdx = 0; /* index into kVehicles */
	int maxLevel = NumLevels();
	int done = 0;
	int cancelled = 0;
	int row = 0; /* 0 = level, 1 = vehicle */

	/* Find current vehicle in list */
	for (int i = 0; i < (int)kNumVehicles; i++)
		if (kVehicles[i].id == gPlayerCarID) { vehIdx = i; break; }

	Platform_ShowCursor();
	Platform_FlushInput();

	while (!done) {
		Platform_PollEvents();
		if (Platform_ShouldQuit()) { cancelled = 1; done = 1; break; }

		/* --- Draw --- */
		TR_FillRect(fb, fbStride, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COL_DKGRAY);

		/* Title */
		TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 30, "CHEAT MODE", COL_RED, 3);

		/* Divider */
		TR_FillRect(fb, fbStride, 80, 68, 480, 1, COL_GRAY);

		/* --- Level section --- */
		{
			const LevelInfo *li = &kLevelInfo[level - 1];
			UInt16 headerCol = (row == 0) ? COL_YELLOW : COL_GRAY;
			UInt16 nameCol   = (row == 0) ? COL_WHITE  : COL_GRAY;
			char numBuf[48];

			TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 85, "LEVEL", headerCol, 2);

			snprintf(numBuf, sizeof(numBuf), "<  %d  >", level);
			TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 120, numBuf,
			                      (row == 0) ? COL_YELLOW : COL_WHITE, 3);

			TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 165, li->name, nameCol, 3);

			TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 205, li->desc,
			                      (row == 0) ? COL_CYAN : COL_GRAY, 2);

			snprintf(numBuf, sizeof(numBuf), "Time: %d:%02d", li->timeSec / 60, li->timeSec % 60);
			TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 235, numBuf,
			                      (row == 0) ? COL_GRAY : COL_DKGRAY, 2);
		}

		/* Divider */
		TR_FillRect(fb, fbStride, 80, 268, 480, 1, COL_GRAY);

		/* --- Vehicle section --- */
		{
			const VehicleInfo *vi = &kVehicles[vehIdx];
			UInt16 headerCol = (row == 1) ? COL_YELLOW : COL_GRAY;
			char idBuf[48];

			TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 278, "VEHICLE", headerCol, 2);

			/* Draw vehicle sprite preview on the left */
			{
				tObjectTypePtr objType = (tObjectTypePtr)GetUnsortedPackEntry(kPackObTy, vi->id, 0);
				if (objType) {
					DrawSprite(objType->frame, 160, 340, 0.0f, 2.0f);
				}
			}

			/* Name and description on the right */
			snprintf(idBuf, sizeof(idBuf), "<  %s  >", vi->name);
			TR_DrawStringCentered(fb, fbStride, 400, 318, idBuf,
			                      (row == 1) ? COL_YELLOW : COL_WHITE, 3);

			TR_DrawStringCentered(fb, fbStride, 400, 358, vi->desc,
			                      (row == 1) ? COL_CYAN : COL_GRAY, 2);
		}

		/* Divider */
		TR_FillRect(fb, fbStride, 80, 395, 480, 1, COL_GRAY);

		/* Instructions */
		TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 415,
		                      "Up/Down: Select   Left/Right: Change", COL_GRAY, 2);
		TR_DrawStringCentered(fb, fbStride, SCREEN_WIDTH / 2, 445,
		                      "Enter: Start   Esc: Cancel", COL_GRAY, 2);

		Platform_Blit2Screen();

		/* --- Input --- */
		if (Platform_IsKeyDown(SDL_SCANCODE_UP) || Platform_IsKeyDown(SDL_SCANCODE_DOWN)) {
			row = 1 - row;
			SDL_Delay(180);
		}
		if (Platform_IsKeyDown(SDL_SCANCODE_LEFT)) {
			if (row == 0) {
				level--;
				if (level < 1) level = maxLevel;
			} else {
				vehIdx = (vehIdx + (int)kNumVehicles - 1) % (int)kNumVehicles;
			}
			SDL_Delay(150);
		}
		if (Platform_IsKeyDown(SDL_SCANCODE_RIGHT)) {
			if (row == 0) {
				level++;
				if (level > maxLevel) level = 1;
			} else {
				vehIdx = (vehIdx + 1) % (int)kNumVehicles;
			}
			SDL_Delay(150);
		}
		if (Platform_IsKeyDown(SDL_SCANCODE_RETURN) ||
		    Platform_IsKeyDown(SDL_SCANCODE_KP_ENTER)) {
			done = 1;
		}
		if (Platform_IsKeyDown(SDL_SCANCODE_ESCAPE)) {
			cancelled = 1;
			done = 1;
		}

		/* Mouse click */
		{
			int mx, my;
			if (Platform_GetMouseClick(&mx, &my)) {
				/* Level area */
				if (my >= 100 && my <= 260) {
					row = 0;
					if (mx < SCREEN_WIDTH / 2 - 60) {
						level--;
						if (level < 1) level = maxLevel;
					} else if (mx > SCREEN_WIDTH / 2 + 60) {
						level++;
						if (level > maxLevel) level = 1;
					}
				}
				/* Vehicle area */
				else if (my >= 280 && my <= 390) {
					row = 1;
					if (mx < SCREEN_WIDTH / 2 - 60) {
						vehIdx = (vehIdx + (int)kNumVehicles - 1) % (int)kNumVehicles;
					} else if (mx > SCREEN_WIDTH / 2 + 60) {
						vehIdx = (vehIdx + 1) % (int)kNumVehicles;
					}
				}
				/* Instructions area = confirm */
				else if (my >= 410) {
					done = 1;
				}
			}
		}

		SDL_Delay(16);
	}

	Platform_HideCursor();
	Platform_FlushInput();

	if (!cancelled) {
		gLevelID = level - 1;
		gPlayerCarID = kVehicles[vehIdx].id;
	} else {
		gEndGame = true;
	}
}

void StartGame(int lcheat)
{
	DisposeInterface();
	gPlayerLives=3;
	gExtraLives=0;
	gPlayerAddOns=0;
	gPlayerDeathDelay=0;
	gFinishDelay=0;
	gPlayerScore=0;
	gLevelID=0;
	gPlayerCarID=kNormalPlayerCarID;
	gNumMissiles=0;
	gNumMines=0;
	gGameOn=true;
	gEndGame=false;
	if(lcheat)
		GetLevelNumber();
	gLCheat=lcheat;
	FadeScreen(1);
	/* HideCursor() - handled by platform layer */
	ScreenMode(kScreenRunning);
	InputMode(kInputRunning);
	if(LoadLevel()){
		ScreenClear();
		FadeScreen(512);
		RenderFrame();
		InitFrameCount();
	}
}

void EndGame()
{
	gPlayerLives=0;//so RenderFrame will not draw Panel.
	RenderFrame();
	DisposeLevel();
	BeQuiet();
	SimplePlaySound(152);
	GameOverAnim();
	InitInterface();
	/* ShowCursor() - handled by platform layer */
	if(!gLCheat)
		CheckHighScore(gPlayerScore);
}
