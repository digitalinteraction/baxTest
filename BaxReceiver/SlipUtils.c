// Karim Ladha 2014 (based on Dan Jacksons code)
// Slip utilities

// SLIP-encoded packet -- write SLIP_END bytes before and after the packet: usb_putchar(SLIP_END);
#define SLIP_END     0xC0                   // End of packet indicator
#define SLIP_ESC     0xDB                   // Escape character, next character will be a substitution
#define SLIP_ESC_END 0xDC                   // Escaped sustitution for the END data byte
#define SLIP_ESC_ESC 0xDD                   // Escaped sustitution for the ESC data byte

#ifdef __C30__
	#include "HardwareProfile.h"
	#include "usb_config.h"
	#if defined(USB_USE_CDC)
		// KL fix, Util.c will not compile without usb support if you include usb.h
		#include "Usb/USB_CDC_MSD.h"
		#define no_block_getc() usb_getchar()
	#else
		#define no_block_getc() -1
	#endif
#else
	#define no_block_getc() -1
#endif
#include <stdio.h>
#include "SlipUtils.h"


short slipIndex = 0;
char slipBuffer[MAX_SLIP_IN_BUFFER];

const char *_user_getslip(void)
{
	int i, input;
	unsigned char value;

	// For upto a full buffers worth
	for(i=0;i<MAX_SLIP_IN_BUFFER;i++)
	{
		// Get a char
		input = no_block_getc();

		// Check there was one
		if(input == -1) return NULL;

		// Convert to unsigned char
		value = (unsigned char)input;

		// Check for end
		if(value == (unsigned char)SLIP_END)
		{
			if(slipIndex == 0) 	// Lone slip end (sync)
				continue;
			slipIndex = 0;		// Restart at begining
			return slipBuffer;
		}

		// Add to line
		slipBuffer[slipIndex] = value;	
		slipIndex++;
		
		// Check overrun - lose whole line
		if(slipIndex >= (short)MAX_SLIP_IN_BUFFER)
			slipIndex = 0;
	}// For
	return NULL; // Out of buffer, never gets here
}

// Encode SLIP (RFC 1055) data
unsigned short WriteToSlip(unsigned char* dest, void* source, unsigned short len, unsigned short maxLen)
{
	unsigned char *writeStart = dest, *readPos = (unsigned char*)source;
    while ((len > 0) && (maxLen > 1))
    {
        if (*readPos == (unsigned char) SLIP_END)
		{
			if(maxLen < 2)break;
			*dest++ = (unsigned char)SLIP_ESC;
			*dest++ = (unsigned char)SLIP_ESC_END;
			maxLen -= 2;
		}
        else if (*readPos == (unsigned char) SLIP_ESC)
		{
			if(maxLen < 2)break;
			*dest++ = (unsigned char)SLIP_ESC;
			*dest++ = (unsigned char)SLIP_ESC_ESC;
			maxLen -= 2;
		}
        else
		{
			*dest++ = *readPos;
			maxLen--;
		}
        readPos++;
		len--;
    }
    return dest - writeStart;
}

unsigned short ReadFromSlip(unsigned char* dest, const char* source, unsigned short maxLen)
{
	enum state_t 
	{
		STATE_ESCAPE, 
		STATE_READ
	} state = STATE_READ;

	unsigned short read = 0;
	unsigned char value;

	while(read < maxLen)
	{
		// Next binary value from read pointer
		value = *source++;

		// State part first
		switch(state){
			case STATE_ESCAPE: 			/* Convert to proper escaped byte*/
					 if(value == ((unsigned char)SLIP_ESC_END)) value = SLIP_END; 	
				else if(value == ((unsigned char)SLIP_ESC_ESC)) value = SLIP_ESC;	
				else 
				{
					state = STATE_READ;	/* Unknown escaped byte, reset state(this char will be lost)*/
					return 0;
				} 	
				state = STATE_READ;		/* Back to reading*/
				break;
			case STATE_READ :  			/* Byte checked in next switch*/
			default : 
				*dest = value;
				break;									
		} // Switch state
				
		// Inspect in char
		switch (value){
			case ((unsigned char)SLIP_ESC) : 
				state = STATE_ESCAPE;	/* Next byte is escaped, dest* overwritted*/
				break;
			case ((unsigned char)SLIP_END) :
				state = STATE_READ;		/* Un-escaped end, return line */
				if(read > 0)			/* If at least one byte read */
					return read;		/* Return read count now*/
				break;					/* Lone SLIP_END at start, ignore */
			default :					/* Increment pointer */
				dest++;					/* Add byte to output */
				read++;					/* Increment bytes read */
				break;
		}
	} // 
	// Return what we decoded
    return read;
}

