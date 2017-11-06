#ifndef _SERIAL_H_
#define _SERIAL_H_

#include "Config.h"

// Open a serial port
int openport(const char *infile, char writeable, int timeout);

// Return the number of bytes available on a port
int availableport(int fd);
 
// Read from a port with a timeout
int readport(int fd, unsigned char *buffer, size_t len, unsigned int timeout);

// Shim for api cross compatibility to typedef int (*GetByte_t)(Settings_t* settings);
int getcSerial(Settings_t* settings);
// Shim for api cross compatibility to typedef int (*PutByte_t)(Settings_t* settings, unsigned char b);
int putcSerial(Settings_t* settings, unsigned char b);

// Close port
int closeport(int fd);

#endif
//EOF
