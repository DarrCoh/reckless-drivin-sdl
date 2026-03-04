#ifndef __RESOURCES_H
#define __RESOURCES_H

/*
 * resources.h - Mac resource fork loader for Reckless Drivin' SDL port
 * Parses the 'Data' file (resource fork moved to data fork) and provides
 * GetResource()-compatible API.
 */

#include "compat.h"

/* Initialize the resource system by loading and parsing the Data file */
int Resources_Init(const char *dataFilePath);
void Resources_Shutdown(void);

/* Get a resource by type and ID. Returns a Handle (caller must not free). */
Handle Resources_Get(FourCharCode type, int id);

/* Get the size of a resource's data */
long Resources_GetSize(Handle h);

/* Release a resource (decrements ref or frees) */
void Resources_Release(Handle h);

/* Get the number of resources of a given type */
int Resources_Count(FourCharCode type);

/* Update the tracked size of a handle (used after in-place decompression).
   This only works for handles returned by Resources_Get. */
void Resources_SetSize(Handle h, long newSize);

/* Utility: make a FourCharCode from a string */
static inline FourCharCode MakeFourCC(const char *s) {
    return ((FourCharCode)s[0] << 24) | ((FourCharCode)s[1] << 16) |
           ((FourCharCode)s[2] << 8) | (FourCharCode)s[3];
}

#endif /* __RESOURCES_H */
