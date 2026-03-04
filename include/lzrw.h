/*
 * lzrw.h - LZRW3-A decompression for Reckless Drivin' SDL port
 *
 * The original game compressed resources using LZRW3-A (by Ross Williams).
 * Each compressed blob starts with a 4-byte big-endian uncompressed size,
 * followed by the LZRW3-A compressed payload.
 */

#ifndef __LZRW_H
#define __LZRW_H

#include "compat.h"

/*
 * Decompress LZRW3-A data.
 *
 * The input buffer (compressedData, compressedSize) begins with a 4-byte
 * big-endian uncompressed size, followed by the LZRW3-A compressed stream.
 *
 * Returns a newly malloc'd buffer containing the decompressed data.
 * The caller is responsible for freeing it.
 * On success, *outSize is set to the decompressed length.
 * Returns NULL on failure.
 */
void *LZRW_Decompress(const void *compressedData, long compressedSize, long *outSize);

/*
 * Decompress a Handle in-place (replaces original LZRWDecodeHandle).
 *
 * Takes a pointer to a Handle. The Handle's data starts with a 4-byte
 * big-endian uncompressed size followed by compressed data.
 * On return, the Handle is replaced with a new one containing the
 * decompressed data, and the original is freed.
 *
 * compressedSize is the total size of the compressed data (including the
 * 4-byte header).
 */
void LZRWDecodeHandle(Handle *h, long compressedSize);

#endif /* __LZRW_H */
