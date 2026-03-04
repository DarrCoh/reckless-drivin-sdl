#ifndef __INTERFACE
#define __INTERFACE

#include "compat.h"

extern int gExit;
extern short gLevelResFile,gAppResFile;
extern unsigned char gLevelFileName[64];

void SaveFlushEvents();
void Eventloop();
void InitInterface();
void DisposeInterface();
void ScreenUpdate(void);
void ShowPicScreen(int id);
void ShowPicScreenNoFade(int id);
void WaitForPress();

#endif