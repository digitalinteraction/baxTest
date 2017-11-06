#ifndef _BITMAP_H_
#define _BITMAP_H_

// Generic Bitmap Header Writer by Dan
//
// NOTES:
// * Pass negative height for a top-down image (otherwise will be bottom-up).
// * bitsPerPixel should be 8 (for palette) or 24 (for BGR).
// * 1/2/4/8-bit images are greyscale (e.g. for 8-bit: 0=black, 255=white).
// * 24-bit images are B-G-R ordered.
// * Each scan line must be padded out to be a multiple of 4-bytes.
//
#ifdef __C30__
#include "HardwareProfile.h"
#ifdef USE_FAT_FS
	#include "FatFs/FatFsIo.h"
#else
	#include "MDD File System/FSIO.h"
#endif
#else
	#include "BaxUtils.h" // Fixes FS and adds other support
#endif

#define BITMAP_8_BIT_PALETTE 8
#define BITMAP_24_BIT_BGR    24

// Write a bitmap header (works for 1/2/4/8/16/32-bit, with <=8-bit having a greyscale palette, 16/32-bit would probably need the BI_BITFIELDS writing to be useful)
void BitmapWriteHeader(FSFILE *fp, int width, int height, int bitsPerPixel);

// [Deprecated] Old compatible function to write 8-bit bitmap header (use the generic function instead)
#define BitmapPalletizedWriteHeader(_fp, _width, _height) BitmapWriteHeader((_fp), (_width), (_height), BITMAP_8_BIT_PALETTE)

#endif
