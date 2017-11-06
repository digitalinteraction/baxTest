// Karim Ladha 2014 (based on Dan Jacksons code)
// Slip utilities
#ifndef SLIP_UTILS_H
#define SLIP_UTILS_H

// Returns when a non-zero length slip encoded packet is received in the serial in
const char *_user_getslip(void);

// Write slip encoded data to a buffer - user must add slip end tags
// User binary tags to use
#define SLIP_START_OF_PACKET 	0xC0	// Actually a slip end, but still correct
#define SLIP_END_OF_PACKET 		0xC0	// Actually a slip end, but still correct
unsigned short WriteToSlip(unsigned char* dest, void* source, unsigned short len, unsigned short maxLen);

// Decode slip encoded data from a buffer - returns length read
unsigned short ReadFromSlip(unsigned char* dest, const char* source, unsigned short maxLen);

// Debug
#ifndef MAX_SLIP_IN_BUFFER
#define MAX_SLIP_IN_BUFFER 1
#endif
extern short slipIndex;
extern char slipBuffer[MAX_SLIP_IN_BUFFER];

#endif
