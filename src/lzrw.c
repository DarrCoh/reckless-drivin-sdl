/*
 * lzrw.c - LZRW3-A decompression for Reckless Drivin' SDL port
 *
 * Uses Ross Williams' LZRW3-A algorithm. The reference implementation is
 * public domain. This file contains a self-contained decompressor adapted
 * from the original LZRW3-A.c, plus wrapper functions for Reckless Drivin'.
 *
 * Pack resource format:
 *   Bytes 0-3: big-endian uncompressed size (game-specific header)
 *   Bytes 4+:  LZRW3-A compressed stream (starts with 4-byte copy flag)
 */

#include "lzrw.h"
#include "resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- LZRW3-A constants (from Ross Williams' reference) ---- */

#define FLAG_BYTES    4
#define FLAG_COPY     1

#define MAX_RAW_ITEM  18
#define MAX_CMP_GROUP (2+16*2)

#define HASH_TABLE_LENGTH       4096
#define HASH_TABLE_DEPTH_BITS   3
#define PARTITION_LENGTH_BITS   (12 - HASH_TABLE_DEPTH_BITS)
#define PARTITION_LENGTH        (1 << PARTITION_LENGTH_BITS)
#define HASH_TABLE_DEPTH        (1 << HASH_TABLE_DEPTH_BITS)
#define HASH_MASK               (PARTITION_LENGTH - 1)
#define DEPTH_MASK              (HASH_TABLE_DEPTH - 1)

#define START_STRING_18 ((uint8_t *) "123456789012345678")

#define HASH(PTR) \
    ((((40543*(((*(PTR))<<8)^((*((PTR)+1))<<4)^(*((PTR)+2))))>>4) & HASH_MASK) \
     << HASH_TABLE_DEPTH_BITS)

/* ---- LZRW3-A decompressor (adapted from Ross Williams' reference) ---- */

static void lzrw3a_decompress(const uint8_t *p_src_first, uint32_t src_len,
                                uint8_t *p_dst_first, uint32_t *p_dst_len) {
    const uint8_t *p_src = p_src_first + FLAG_BYTES;
    uint8_t *p_dst = p_dst_first;

    const uint8_t *p_src_post  = p_src_first + src_len;
    const uint8_t *p_src_max16 = p_src_first + src_len - (MAX_CMP_GROUP - 2);

    /* Hash table: 4096 pointers into the output buffer */
    uint8_t *hash[HASH_TABLE_LENGTH];

    uint32_t control = 1;
    uint32_t literals = 0;
    uint32_t cycle = 0;

    /* Check copy flag */
    if (*p_src_first == FLAG_COPY) {
        uint32_t copy_len = src_len - FLAG_BYTES;
        memcpy(p_dst_first, p_src_first + FLAG_BYTES, copy_len);
        *p_dst_len = copy_len;
        return;
    }

    /* Initialize hash table to point to a constant string */
    {
        int i;
        for (i = 0; i < HASH_TABLE_LENGTH; i++)
            hash[i] = START_STRING_18;
    }

    while (p_src < p_src_post) {
        uint32_t unroll;

        if (control == 1) {
            control = 0x10000 | *p_src++;
            control |= (*p_src++) << 8;
        }

        unroll = (p_src <= p_src_max16) ? 16 : 1;

        while (unroll--) {
            if (control & 1) {
                /* Copy item */
                uint8_t *p;
                uint32_t lenmt;
                uint8_t *p_ziv = p_dst;
                uint32_t index;

                lenmt = *p_src++;
                index = ((lenmt & 0xF0) << 4) | *p_src++;
                p = hash[index];
                lenmt &= 0xF;

                /* Copy 3 + lenmt bytes */
                *p_dst++ = *p++;
                *p_dst++ = *p++;
                *p_dst++ = *p++;
                while (lenmt--)
                    *p_dst++ = *p++;

                /* Flush pending literal hashings */
                if (literals > 0) {
                    uint8_t *r = p_ziv - literals;
                    hash[HASH(r) + cycle] = r;
                    cycle = (cycle + 1) & DEPTH_MASK;
                    if (literals == 2) {
                        r++;
                        hash[HASH(r) + cycle] = r;
                        cycle = (cycle + 1) & DEPTH_MASK;
                    }
                    literals = 0;
                }

                /* Update hash table with current position */
                hash[(index & (~DEPTH_MASK)) + cycle] = p_ziv;
                cycle = (cycle + 1) & DEPTH_MASK;
            } else {
                /* Literal item */
                *p_dst++ = *p_src++;

                if (++literals == 3) {
                    uint8_t *p = p_dst - 3;
                    hash[HASH(p) + cycle] = p;
                    cycle = (cycle + 1) & DEPTH_MASK;
                    literals = 2;
                }
            }

            control >>= 1;

            if (p_src >= p_src_post)
                break;
        }
    }

    *p_dst_len = (uint32_t)(p_dst - p_dst_first);
}

/* ---- Public API ---- */

void *LZRW_Decompress(const void *compressedData, long compressedSize, long *outSize) {
    if (!compressedData || compressedSize < 8) {
        if (outSize) *outSize = 0;
        return NULL;
    }

    const uint8_t *src = (const uint8_t *)compressedData;

    /* First 4 bytes: big-endian uncompressed size (game-specific header) */
    uint32_t uncompressed_size = ((uint32_t)src[0] << 24) |
                                 ((uint32_t)src[1] << 16) |
                                 ((uint32_t)src[2] << 8)  |
                                  (uint32_t)src[3];

    /* Sanity check */
    if (uncompressed_size > 64 * 1024 * 1024) {
        fprintf(stderr, "LZRW_Decompress: uncompressed size %u seems too large\n",
                (unsigned)uncompressed_size);
        if (outSize) *outSize = 0;
        return NULL;
    }

    /* Allocate output buffer with some overrun space */
    uint8_t *dst = (uint8_t *)calloc(1, uncompressed_size + 1024);
    if (!dst) {
        fprintf(stderr, "LZRW_Decompress: out of memory for %u bytes\n",
                (unsigned)uncompressed_size);
        if (outSize) *outSize = 0;
        return NULL;
    }

    /* LZRW3-A stream starts after the 4-byte game header */
    uint32_t lzrw_src_len = (uint32_t)(compressedSize - 4);
    uint32_t dst_len = 0;

    lzrw3a_decompress(src + 4, lzrw_src_len, dst, &dst_len);

    if (dst_len != uncompressed_size) {
        fprintf(stderr, "LZRW_Decompress: warning: expected %u bytes, got %u\n",
                (unsigned)uncompressed_size, (unsigned)dst_len);
    }

    if (outSize) *outSize = (long)dst_len;
    return dst;
}

void LZRWDecodeHandle(Handle *h, long compressedSize) {
    if (!h || !*h) return;

    char *compressed = **h;
    if (!compressed) return;

    long decompressed_size = 0;
    void *decompressed = LZRW_Decompress(compressed, compressedSize, &decompressed_size);
    if (!decompressed) {
        fprintf(stderr, "LZRWDecodeHandle: decompression failed\n");
        return;
    }

    /* Replace the handle's data buffer */
    free(**h);
    **h = (char *)decompressed;

    /* Update the tracked size */
    Resources_SetSize(*h, decompressed_size);
}
