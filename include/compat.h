#ifndef __COMPAT_H
#define __COMPAT_H

/*
 * compat.h - Mac OS type compatibility layer for Reckless Drivin' SDL port
 * Replaces all Carbon/Classic Mac OS types with standard C equivalents.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* Mac integer types */
typedef uint8_t   UInt8;
typedef uint16_t  UInt16;
typedef uint32_t  UInt32;
typedef uint64_t  UInt64;
typedef int8_t    SInt8;
typedef int16_t   SInt16;
typedef int32_t   SInt32;
typedef int64_t   SInt64;
typedef char*     Ptr;
typedef char**    Handle;
typedef int       Boolean;
typedef int       OSErr;
typedef uint32_t  FourCharCode;

/* Pascal string type - first byte is length */
typedef unsigned char Str255[256];
typedef unsigned char Str31[32];
typedef unsigned char Str15[16];
typedef unsigned char* StringPtr;

/* Mac constants */
#ifndef nil
#define nil NULL
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
#define noErr 0

/* Byte swapping for big-endian data (original game is PPC big-endian) */
#ifdef __BIG_ENDIAN__
  #define SWAP16(x) (x)
  #define SWAP32(x) (x)
#else
  static inline uint16_t SWAP16(uint16_t x) {
    return (x >> 8) | (x << 8);
  }
  static inline uint32_t SWAP32(uint32_t x) {
    return ((x >> 24) & 0xff) | ((x >> 8) & 0xff00) |
           ((x << 8) & 0xff0000) | ((x << 24) & 0xff000000);
  }
#endif

static inline int32_t SWAP32S(int32_t x) { return (int32_t)SWAP32((uint32_t)x); }
static inline int16_t SWAP16S(int16_t x) { return (int16_t)SWAP16((uint16_t)x); }

/* Swap a float stored in big-endian format */
static inline float SWAPFloat(float f) {
    union { float f; uint32_t u; } conv;
    conv.f = f;
    conv.u = SWAP32(conv.u);
    return conv.f;
}

/* Memory Manager replacements */
static inline Ptr NewPtr(long size) { return (Ptr)calloc(1, size); }
static inline Ptr NewPtrClear(long size) { return (Ptr)calloc(1, size); }
static inline void DisposePtr(Ptr p) { free(p); }

/* Handle emulation - we store: [4 bytes size][data...] and the Handle points to a pointer to data */
static inline Handle NewHandle(long size) {
    /* Allocate a block: pointer-to-data + actual data */
    char **h = (char**)malloc(sizeof(char*));
    if (!h) return nil;
    *h = (char*)malloc(size);
    if (!*h) { free(h); return nil; }
    memset(*h, 0, size);
    return (Handle)h;
}

static inline void DisposeHandle(Handle h) {
    if (h) {
        if (*h) free(*h);
        free(h);
    }
}

static inline long GetHandleSize(Handle h) {
    /* We need to track sizes - for now this is handled by the resource loader */
    /* This will be set properly by our resource system */
    return 0; /* placeholder - overridden by resource loader */
}

static inline void HLock(Handle h) { (void)h; } /* no-op */
static inline void HLockHi(Handle h) { (void)h; } /* no-op */

static inline void BlockMoveData(const void *src, void *dst, long size) {
    memmove(dst, src, size);
}

/* Pascal string helpers */
static inline int PStrLen(const unsigned char *s) { return s[0]; }
static inline void PStrCopy(unsigned char *dst, const unsigned char *src) {
    memcpy(dst, src, src[0] + 1);
}

/* Math */
#ifndef PI
#define PI 3.14159265358979323846
#endif

#endif /* __COMPAT_H */
