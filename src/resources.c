/*
 * resources.c - Mac resource fork parser for Reckless Drivin' SDL port
 *
 * Parses a classic Mac resource fork that has been moved to the data fork
 * (the "Data" file). The resource fork format is:
 *
 *   Header (256 bytes):
 *     - data_offset   (4 bytes, big-endian)
 *     - map_offset    (4 bytes, big-endian)
 *     - data_length   (4 bytes, big-endian)
 *     - map_length    (4 bytes, big-endian)
 *     - padding to 256 bytes
 *
 *   Data section (at data_offset):
 *     Each resource entry: length (4 bytes, big-endian) + raw data bytes
 *
 *   Map section (at map_offset):
 *     - 16 bytes: copy of header fields
 *     - 4 bytes: next resource map handle (reserved)
 *     - 2 bytes: file reference number
 *     - 2 bytes: resource file attributes
 *     - 2 bytes: type_list_offset (from map start)
 *     - 2 bytes: name_list_offset (from map start)
 *     - Type list:
 *         num_types - 1 (2 bytes, big-endian)
 *         For each type:
 *           type code (4 bytes, FourCC)
 *           num_resources - 1 (2 bytes, big-endian)
 *           ref_list_offset from type_list_start (2 bytes, big-endian)
 *     - Reference list entries (12 bytes each):
 *         id (2 bytes, signed big-endian)
 *         name_offset (2 bytes, big-endian, 0xFFFF = no name)
 *         attrs_and_data_offset (4 bytes): top byte = attrs, low 3 bytes = offset from data section start
 *         reserved (4 bytes)
 *
 * The actual "Data" file for Reckless Drivin' contains:
 *   data_offset = 256 (0x100)
 *   map_offset  = 5801225 (0x588509)
 *   3 types: "Pack" (22 resources), "PPic" (10 resources), "Chck" (1 resource)
 */

#include "resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* ---- Internal structures ---- */

/* Parsed entry from the resource map */
typedef struct {
    FourCharCode type;
    SInt16       id;
    UInt32       data_offset;  /* offset from data section start */
    UInt8        attrs;
} ResourceEntry;

/* Tracked handle: stores size alongside the data pointer so we can
   retrieve the size given only a Handle (which is &tracked[i].data).
   The Handle returned to callers is (Handle)&tracked[i].data, and we
   recover the TrackedHandle via offsetof(TrackedHandle, data). */
typedef struct {
    long  size;
    char *data;
} TrackedHandle;

/* ---- Module state ---- */

static UInt8 *gFileData          = NULL;   /* entire file contents */
static long   gFileSize          = 0;
static UInt32 gDataSectionOffset = 0;      /* absolute file offset of data section */

static ResourceEntry *gEntries    = NULL;
static int            gEntryCount = 0;

/* Tracked handles returned to callers */
#define MAX_TRACKED_HANDLES 512
static TrackedHandle gTracked[MAX_TRACKED_HANDLES];
static int           gTrackedCount = 0;

/* ---- Helpers for reading big-endian values from raw bytes ---- */

static inline UInt16 read_u16(const UInt8 *p) {
    return (UInt16)(((UInt16)p[0] << 8) | (UInt16)p[1]);
}

static inline UInt32 read_u32(const UInt8 *p) {
    return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) |
           ((UInt32)p[2] << 8)  |  (UInt32)p[3];
}

static inline SInt16 read_s16(const UInt8 *p) {
    return (SInt16)read_u16(p);
}

/* ---- Public API ---- */

int Resources_Init(const char *dataFilePath) {
    FILE *fp = fopen(dataFilePath, "rb");
    if (!fp) {
        fprintf(stderr, "Resources_Init: cannot open '%s'\n", dataFilePath);
        return 0;
    }

    /* Determine file size */
    fseek(fp, 0, SEEK_END);
    gFileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Read the entire file into memory */
    gFileData = (UInt8 *)malloc(gFileSize);
    if (!gFileData) {
        fprintf(stderr, "Resources_Init: out of memory (%ld bytes)\n", gFileSize);
        fclose(fp);
        return 0;
    }
    if ((long)fread(gFileData, 1, gFileSize, fp) != gFileSize) {
        fprintf(stderr, "Resources_Init: short read on '%s'\n", dataFilePath);
        free(gFileData);
        gFileData = NULL;
        fclose(fp);
        return 0;
    }
    fclose(fp);

    /* ---- Parse the resource fork header (first 16 bytes) ---- */
    UInt32 data_offset = read_u32(gFileData + 0);
    UInt32 map_offset  = read_u32(gFileData + 4);
    /* data_length at +8 and map_length at +12 are not needed */

    /* Store the data section offset for use by Resources_Get */
    gDataSectionOffset = data_offset;

    /* ---- Parse the resource map ---- */
    const UInt8 *map = gFileData + map_offset;

    /* map + 0..15 : copy of header (skip)
       map + 16..19: next resource map handle (skip)
       map + 20..21: file ref num (skip)
       map + 22..23: file attributes (skip)
       map + 24..25: type_list_offset (from map start)
       map + 26..27: name_list_offset (from map start) */
    UInt16 type_list_offset = read_u16(map + 24);

    const UInt8 *type_list = map + type_list_offset;

    /* Number of types minus one (stored as uint16 big-endian) */
    UInt16 num_types_m1 = read_u16(type_list);
    int num_types = (int)num_types_m1 + 1;

    /* First pass: count total resources to allocate the entry array */
    int total_resources = 0;
    for (int t = 0; t < num_types; t++) {
        const UInt8 *te = type_list + 2 + t * 8;  /* each type entry is 8 bytes */
        UInt16 num_res_m1 = read_u16(te + 4);
        total_resources += (int)num_res_m1 + 1;
    }

    gEntries = (ResourceEntry *)calloc(total_resources, sizeof(ResourceEntry));
    if (!gEntries) {
        fprintf(stderr, "Resources_Init: out of memory for %d entries\n", total_resources);
        free(gFileData);
        gFileData = NULL;
        return 0;
    }
    gEntryCount = total_resources;

    /* Second pass: read each type and its reference list */
    int idx = 0;
    for (int t = 0; t < num_types; t++) {
        const UInt8 *te = type_list + 2 + t * 8;
        FourCharCode restype = read_u32(te);
        UInt16 num_res_m1    = read_u16(te + 4);
        UInt16 ref_offset    = read_u16(te + 6);  /* from type_list start */
        int num_res          = (int)num_res_m1 + 1;

        const UInt8 *ref_list = type_list + ref_offset;

        for (int r = 0; r < num_res; r++) {
            const UInt8 *re = ref_list + r * 12;  /* each ref entry is 12 bytes */
            SInt16 res_id           = read_s16(re + 0);
            /* name_offset at re+2: 0xFFFF means no name (unused here) */
            UInt32 attrs_and_offset = read_u32(re + 4);
            UInt8  attrs            = (UInt8)(attrs_and_offset >> 24);
            UInt32 res_data_offset  = attrs_and_offset & 0x00FFFFFF;
            /* reserved 4 bytes at re+8 (skip) */

            gEntries[idx].type        = restype;
            gEntries[idx].id          = res_id;
            gEntries[idx].data_offset = res_data_offset;
            gEntries[idx].attrs       = attrs;
            idx++;
        }
    }

    /* Initialize tracked handle pool */
    gTrackedCount = 0;
    memset(gTracked, 0, sizeof(gTracked));

    fprintf(stderr, "Resources_Init: loaded %d resources of %d types from '%s'\n",
            gEntryCount, num_types, dataFilePath);

    return 1;  /* success */
}

Handle Resources_Get(FourCharCode type, int id) {
    if (!gFileData || !gEntries) return NULL;

    /* Find the matching entry */
    for (int i = 0; i < gEntryCount; i++) {
        if (gEntries[i].type == type && gEntries[i].id == (SInt16)id) {
            /* Compute absolute offset into the file.
               gDataSectionOffset is where the data section starts,
               gEntries[i].data_offset is the offset within that section. */
            UInt32 abs_offset = gDataSectionOffset + gEntries[i].data_offset;

            /* The resource data at this location starts with a 4-byte big-endian
               length prefix, followed by the raw resource bytes. */
            const UInt8 *entry_ptr = gFileData + abs_offset;
            UInt32 res_length = read_u32(entry_ptr);
            const UInt8 *res_data = entry_ptr + 4;

            /* Allocate a tracked handle and copy the data into it */
            if (gTrackedCount >= MAX_TRACKED_HANDLES) {
                fprintf(stderr, "Resources_Get: tracked handle pool exhausted\n");
                return NULL;
            }

            int slot = gTrackedCount++;
            gTracked[slot].size = (long)res_length;
            gTracked[slot].data = (char *)malloc(res_length);
            if (!gTracked[slot].data) {
                fprintf(stderr, "Resources_Get: out of memory for %u bytes\n",
                        (unsigned)res_length);
                gTrackedCount--;
                return NULL;
            }
            memcpy(gTracked[slot].data, res_data, res_length);

            /* Return a Handle (char**) pointing to the data pointer inside
               the TrackedHandle. Callers dereference it with *h to get
               the raw bytes. We can recover the TrackedHandle later via
               offsetof(TrackedHandle, data). */
            return (Handle)&gTracked[slot].data;
        }
    }

    /* Log in a way that shows the FourCC as characters.
       FourCharCode is big-endian packed, so byte 0 is the high byte. */
    char cc[5];
    cc[0] = (char)(type >> 24);
    cc[1] = (char)(type >> 16);
    cc[2] = (char)(type >> 8);
    cc[3] = (char)(type);
    cc[4] = '\0';
    fprintf(stderr, "Resources_Get: resource '%s' id=%d not found\n", cc, id);
    return NULL;
}

long Resources_GetSize(Handle h) {
    if (!h) return 0;

    /* h == &gTracked[slot].data, so we recover the TrackedHandle base address
       by subtracting the offset of the 'data' member. */
    TrackedHandle *t = (TrackedHandle *)((char *)h - offsetof(TrackedHandle, data));
    return t->size;
}

void Resources_SetSize(Handle h, long newSize) {
    if (!h) return;
    TrackedHandle *t = (TrackedHandle *)((char *)h - offsetof(TrackedHandle, data));
    t->size = newSize;
}

void Resources_Release(Handle h) {
    if (!h) return;

    TrackedHandle *t = (TrackedHandle *)((char *)h - offsetof(TrackedHandle, data));

    if (t->data) {
        free(t->data);
        t->data = NULL;
    }
    t->size = 0;
}

int Resources_Count(FourCharCode type) {
    int count = 0;
    for (int i = 0; i < gEntryCount; i++) {
        if (gEntries[i].type == type) {
            count++;
        }
    }
    return count;
}

void Resources_Shutdown(void) {
    /* Free all tracked handles that have not been individually released */
    for (int i = 0; i < gTrackedCount; i++) {
        if (gTracked[i].data) {
            free(gTracked[i].data);
            gTracked[i].data = NULL;
        }
    }
    gTrackedCount = 0;

    /* Free the entry table */
    if (gEntries) {
        free(gEntries);
        gEntries = NULL;
    }
    gEntryCount = 0;

    /* Free the file data buffer */
    if (gFileData) {
        free(gFileData);
        gFileData = NULL;
    }
    gFileSize = 0;
    gDataSectionOffset = 0;
}
