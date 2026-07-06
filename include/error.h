#ifndef __ERROR
#define __ERROR

#include "compat.h"

void HandleError(int id);

static inline void DoError(OSErr id)
{
	if(id) HandleError(id);
}

#endif