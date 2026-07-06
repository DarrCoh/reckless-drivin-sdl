#ifndef __REGISTER
#define __REGISTER

#include "compat.h"

extern UInt32 gKey;
extern int gRegistered;

void Register(int fullscreen);
int CheckRegi();

#endif