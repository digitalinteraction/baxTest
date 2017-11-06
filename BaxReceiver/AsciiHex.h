// Karim Ladha 2014
// Utils for converting between binary and ascii hex
#ifndef _ASCII_HEX_H_
#define _ASCII_HEX_H_

// Simple function to write capitalised hex to a buffer from binary
// adds no spaces, adds a terminating null, returns chars written
// Endianess specified, for little endian, read starts at last ptr pos backwards
unsigned short WriteBinaryToHex(char* dest, void* source, unsigned short len, unsigned char littleEndian);

// Simple function to read an ascii string of hex chars from a buffer 
// For each hex pair, a byte is written to the out buffer
// Returns number read, earlys out on none hex char (caps not important)
unsigned short ReadHexToBinary(unsigned char* dest, const char* source, unsigned short maxLen);

#endif
