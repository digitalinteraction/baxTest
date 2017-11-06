// Bitmap Header Writer by Dan

#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE
//#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __C30__
#include "FSconfig.h"
#ifdef USE_FAT_FS
	#include "FatFs/FatFsIo.h"
#else
	#include "MDD File System/FSIO.h"
#endif
#undef FILE
#define FILE FSFILE
#define fputc FSfputc
#else
	#include "BaxUtils.h" // Fixes FS and adds other support
#endif

// Endian-independent short/long read/write
static void fputshort(short v, FILE *fp) { fputc((char)(((v) >> 0) & 0xff), fp); fputc((char)(((v) >> 8) & 0xff), fp); }
static void fputlong(long v, FILE *fp) { fputc((char)(((v) >> 0) & 0xff), fp); fputc((char)(((v) >> 8) & 0xff), fp); fputc((char)(((v) >> 16) & 0xff), fp); fputc((char)(((v) >> 24) & 0xff), fp); }

// IMPORTANT: Pass negative height for a top-down image (otherwise will be bottom-up)
void BitmapWriteHeader(FSFILE *fp, int width, int height, int bitsPerPixel)
{
	unsigned int i;
	const unsigned long headerSize = 54;	// Header size
	const unsigned int stride = 4 * ((width * ((bitsPerPixel + 7) / 8) + 3) / 4);	// Byte width of each line
	const unsigned long imageSize = (unsigned long)stride * abs(height);	// Total number of bytes that will be written
	unsigned int paletteEntries = 0;
	
	// If we have a palette
	if (bitsPerPixel <= 8)
	{
		paletteEntries = ((unsigned int)1 << bitsPerPixel);
	}
	
    fputc('B', fp); fputc('M', fp); // bfType
    fputlong(headerSize + (paletteEntries << 2) + imageSize, fp);  // bfSize
    fputshort(0, fp);              // bfReserved1
    fputshort(0, fp);              // bfReserved2
    fputlong(headerSize + (paletteEntries << 2), fp);  // bfOffBits
    fputlong(40, fp);              // biSize
    fputlong(width, fp);           // biWidth
    fputlong(height, fp);          // biHeight
    fputshort(1, fp);              // biPlanes
    fputshort(bitsPerPixel, fp);   // biBitCount
    fputlong(0, fp);               // biCompression (0=BI_RGB, 3=BI_BITFIELDS)
    fputlong(imageSize, fp);       // biSizeImage
    fputlong(0, fp);               // biXPelsPerMeter
    fputlong(0, fp);               // biYPelsPerMeter
    fputlong(0, fp);               // biClrUsed
    fputlong(0, fp);               // biClrImportant

    // Grey-scale palette entries
	for (i = 0; i < paletteEntries; i++)
	{
		unsigned char v = (unsigned char)((i * 255) / (paletteEntries - 1));  // Bit costly to compute, but will work for 1/2/4/8-bits
		fputc(v, fp); fputc(v, fp); fputc(v, fp); fputc(0x00, fp); 	// RGBX
	}
    
	return;
}


