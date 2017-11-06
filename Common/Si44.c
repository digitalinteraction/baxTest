// Driver for the Si4432 Radio with Antenna switch controls like the RFM22
// Requires hardware definitions for SPI control interface
// KL 2013

// Include
#include <stdlib.h>
#include <string.h>
#include "Config.h"
#include "Peripherals/Si44.h"
#include "Serial.h"
#include "AsciiHex.h"
#include "SlipUtils.h"

// Debug setting
#undef DEBUG_LEVEL
#define DEBUG_LEVEL	0
#define DBG_FILE dbg_file
#if (DEBUG_LEVEL > 0)||(GLOBAL_DEBUG_LEVEL > 0)
static const char* dbg_file = "si44";
#endif
#include "Debug.h"

#ifndef NULL
#define NULL 0
#endif

// Globals
volatile Si44RadioState_t 	Si44RadioState = SI44_OFF;
volatile Si44Status_t 		Si44Status = {{0}};
volatile Si44IntStat_t		Si44RadioInts = {{0}};
volatile Si44EventCB_t 		Si44EventCB = NULL;
volatile unsigned char		numPktsToRx = 0; // Set by rx command

// Internal Prototypes

// Source
// Redundant if using transport mode
void Si44SetEventCB(Si44EventCB_t CB){}

// List of commands, SI44_CMD_EOL terminated
void Si44CommandList(const Si44Cmd_t* cmdList)
{
	while((cmdList != NULL) && (cmdList->type != SI44_CMD_EOL))
	{
		Si44Command(cmdList, NULL);
		cmdList++;
	} 
}

// Execute command
Si44Event_t* Si44Command(const Si44Cmd_t* cmd, void* buffer)
{
	// The command will need to be sent over the transport
	// The event is NOT returned for the PC implementation
	// The buffer can not be used either, wait for events 
	unsigned char binaryCmd[SERIAL_WRITE_BUFFER_SIZE];
	unsigned char encodedCmd[SERIAL_WRITE_BUFFER_SIZE];
	int sendLen = 0, i = 0;
	// Check mode
	if(gSettings.source != 'S' && gSettings.format != 'E')
	{
		DBG_ERROR("Radio command attempt from wrong mode!");
		return NULL;
	}
	// Make binary copy
	binaryCmd[0] = cmd->type;
	binaryCmd[1] = cmd->len;
	memcpy(&binaryCmd[2],cmd->data,cmd->len);
	// Assertions
	if(cmd->len > (SERIAL_WRITE_BUFFER_SIZE/2 - 5)) // 5 = type+len+cr+lf+null 
	{
		DBG_ERROR("Command too long, aborted");
		return NULL;
	}
	// Encode to transport mode
	if(gSettings.encoding == 'H')
	{
		sendLen = WriteBinaryToHex((char*)encodedCmd, binaryCmd, 2+cmd->len, FALSE);
		encodedCmd[sendLen++]='\r';
		encodedCmd[sendLen++]='\n';
		encodedCmd[sendLen]='\0';
	}
	else if (gSettings.encoding == 'S')
	{
		encodedCmd[0] = SLIP_START_OF_PACKET;
		sendLen = WriteToSlip(&encodedCmd[1], binaryCmd, 2+cmd->len, FALSE);
		encodedCmd[1+sendLen] = SLIP_END_OF_PACKET;
		sendLen+=2;		
	}
	// Send the packet
	DBG_INFO("\r\nSi44 CMD %s",encodedCmd);
	for(i=0;i<sendLen;i++)
	{
		if(putcSerial(&gSettings, encodedCmd[i]) <= 0) 
		{
			DBG_ERROR("Error writing command port");
			break;
		}
	}
	return NULL;
}



//EOF
