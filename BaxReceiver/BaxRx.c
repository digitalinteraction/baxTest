/* 
 * Copyright (c) 2013-2014, Newcastle University, UK.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met: 
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 */

// Simple interface to Si44 BAX receiver
// Karim Ladha, 2013-2014

// Includes
#ifdef _WIN32
	#define _CRT_SECURE_NO_WARNINGS
    #include <windows.h>
    #include <io.h>
#endif
#include <string.h>
#include <stdint.h>
#ifdef __C30__
#include <Compiler.h>
#include <TimeDelay.h>
#include "HardwareProfile.h"
#include "Settings.h"
#include "Peripherals/Rtc.h"
#include "Data.h"
#endif
#include "aes.h"
#include "AsciiHex.h"
#include "Peripherals/Si44.h"
#include "BaxRx.h"
#define BAX_CONFIG_MAKE_VARS
#include "Si44_config.h"
#include "Bitmap.h"

#ifdef __C30__
#ifdef USE_FAT_FS
	#include "FatFs/FatFsIo.h"
#else
	#include "MDD File System/FSIO.h"
#endif
#include "Utils/FSutils.h"
#else
	#include "Config.h"
	#include "BaxUtils.h"
#endif

#ifndef NULL
#define NULL 0
#endif

// Debug setting
#undef DEBUG_LEVEL
#define DEBUG_LEVEL	0
#define DBG_FILE dbg_file
#if (DEBUG_LEVEL > 0)||(GLOBAL_DEBUG_LEVEL > 0)
static const char* dbg_file = "baxrx";
#endif
#include "Debug.h"

// Globals
#ifndef MAX_BAX_INFO_ENTRIES
#ifdef _WIN32
	#pragma message("Device will not decrypt packets!")
#else
	#warning "Device will not decrypt packets!"
#endif
	#define MAX_BAX_INFO_ENTRIES 	0
	#define MAX_BAX_SAVED_PACKETS 	0
#endif
#if (MAX_BAX_INFO_ENTRIES > 0)
BaxDeviceInfo_t baxDeviceInfo[MAX_BAX_INFO_ENTRIES];				/*Device info struct*/
BaxEntry_t baxEntries[MAX_BAX_INFO_ENTRIES * MAX_BAX_SAVED_PACKETS];/*Device last packets*/
#endif
void(*BaxInfoPacketCB)(BaxPacket_t* pkt) = NULL;					/*Link packet call back*/

// There must be a packet and event handler somewhere
extern void EventCB (Si44Event_t* evt);
extern void BaxPacketEvent(Si44Event_t* evt); 

// Private prototypes
static void BaxEraseEntry(BaxEntry_t *entry);
static void BaxEraseInfo(BaxInfo_t* info);
static void BaxEraseDeviceInfo(BaxDeviceInfo_t* device);
static void BaxAddNewInfo(BaxInfo_t* entry);
static void BaxAddEntry(BaxDeviceInfo_t* device, BaxPacket_t* pkt);
static BaxDeviceInfo_t* BaxSearchInfo(unsigned long address);
static unsigned char BaxAddInfoToFile (FSFILE* file, BaxInfo_t* info);
static void BaxRfConfigFromFile(FSFILE* input_file);
void BaxChannelSurvey(FSFILE* output_file);

// Call first
void BaxRxInit(void)
{
	// Clear callback
	BaxInfoPacketCB = NULL;
	// Initialise device info struct
	BaxInitDeviceInfo();
#ifdef BAX_DEVICE_INFO_FILE
	// Load the device info file
	BaxLoadInfoFile(BAX_DEVICE_INFO_FILE);
#endif
	// Init radio using script
	Si44SetEventCB(EventCB);
	Si44CommandList(bax_setup);

	// Check header is default - router only
	#ifdef __C30__
	if((char)(gSettings.radioSubnet >> 8) != 'B')
	{
		unsigned char regValPair[2] = {Si44_Check_Header3,(gSettings.radioSubnet >> 8)}; 
		Si44Cmd_t cmd;
		cmd.type = 0x05;
		cmd.len = 0x02;
		cmd.data = regValPair;
		Si44Command(&cmd, NULL);
	}
	#endif

	#ifdef BAX_RF_SETTINGS_FILE
	// Survey/Custom configuration
	{
		FSFILE* rfConfig = FSfopen(BAX_RF_SETTINGS_FILE,"rb");
		if(rfConfig == NULL) 
		{
			DBG_INFO("No radio init script found");
		}
		else
		{
			// Read the config file and load settings
			BaxRfConfigFromFile(rfConfig);
			FSfclose(rfConfig);
		}
	}
	#endif
	#ifdef BAX_RF_SURVEY_OUT_FILE
	{
		FSFILE* rfSurvey = FSfopen(BAX_RF_SURVEY_OUT_FILE,"rb");
		if(rfSurvey == NULL)
		{
			FSfclose(rfSurvey);
			rfSurvey = FSfopen(BAX_RF_SURVEY_OUT_FILE,"wb");
			if(rfSurvey != NULL)
			{
				BaxChannelSurvey(rfSurvey);
				FSfclose(rfSurvey);
			}
			{ // Run config again
				FSFILE* rfConfig = FSfopen(BAX_RF_SETTINGS_FILE,"rb");
				if(rfConfig != NULL) 
				{
					// Read the config file and load settings
					BaxRfConfigFromFile(rfConfig);
					FSfclose(rfConfig);
				}
			}
		}
	}
	#endif
	if(Si44RadioState == SI44_HW_ERROR)
	{
		gStatus.radio_state = ERROR_STATE;
		return;
	}
	gStatus.radio_state = ACTIVE_STATE;
}

#ifdef __C30__
// Call intermittently from main
void BaxRxTasks(void)
{
	static unsigned char checkRegs[2];

	// Debug
	DBG_INFO("\r\nBAX RX TASKS");

	// One Hz upto every 60 sec
	if(gStatus.radio_state == OFF_STATE || gStatus.radio_state == ERROR_STATE) return; // Call init first

	// Check radio ok - the read will be checked by event handler
	if(Si44EventCB != NULL)
		Si44Command(bax_check, checkRegs);
}

void EventCB(Si44Event_t* evt)
{
	// Called for every radio event 
	if(evt == NULL) return;
	// Errors
	if(evt->err != SI44_OK)
	{
		DBG_INFO("\r\nSI44 ERROR, CMD %u ERR %u",evt->type,evt->err);	
	}
	// Process event
	switch(evt->type) {
		case SI44_RESET	: {
			DBG_INFO("\r\nSI44 RESET");
			break;
		}
		case SI44_CMD_STANDBY : {
			DBG_INFO("\r\nSI44 STANDBY");
			break;
		}
		case SI44_CMD_IDLE : {
			DBG_INFO("\r\nSI44 IDLE");
			break;
		}	
		case SI44_TX : {
			DBG_INFO("\r\nSI44 TXING");
			break;
		}
		case SI44_TX_DONE : {
			DBG_INFO("\r\nSI44 TXED");
			break;
		}
		case SI44_RX : {
			DBG_INFO("\r\nSI44 RX");
			break;
		}
		case SI44_READ_PKT : {
			DBG_INFO("\r\nSI44 PKT EVT");
			if(Si44RadioState != SI44_RXING)
				Si44Command(resumeRx, NULL);/*Re-enable RX*/
			BaxPacketEvent(evt); 			/*Process packet*/
			break;
		}
		case SI44_WRITE_REG	: {
			DBG_INFO("\r\nSI44 WRITE REG");
			break;
		}	
		case SI44_WRITE_REG_LIST : {
			DBG_INFO("\r\nSI44 WRITE LIST");
			break;
		}
		case SI44_READ_REG_LIST : {
			DBG_INFO("\r\nSI44 READ LIST");
			// This is out read reg check callback on gpioctrl
			// Check we get a payload of GPIO0_SETTING, GPIO1_SETTING
			if(evt->data[0] != GPIO0_SETTING || evt->data[1] != GPIO1_SETTING)
			{	
				DBG_ERROR("si44 cfg mismatch");
				gStatus.radio_state = ERROR_STATE;	// Indicate error
			}
			// Incase its ever caught not receiving
			if(Si44RadioState == SI44_IDLE)
				Si44Command(resumeRx, NULL);/*Re-enable RX*/
			break;
		}	
		case SI44_EVT_ERR : {
			DBG_INFO("\r\nSI44 ERROR");			
			break;
		}
		default	: {
			DBG_INFO("\r\nSI44 UNKNOWN EVENT");
			break;
		}
	};// Switch
}


// For router device only. Called from the Rx isr to read the new packet
void BaxPacketEvent(Si44Event_t* evt)
{
	unsigned short temp;
	const unsigned short baxElementDataLen = sizeof(BaxPacket_t);
	// Check evt->err
	if(evt->err != SI44_OK) return;
	// Check packet data and len
	if(evt->data == NULL || evt->len != baxElementDataLen) return;
	// Make packet into data element
	BaxPacket_t* pkt = (BaxPacket_t*)evt->data;	// Cast ptr to bax pkt type
	// Check mask before accepting
	temp = ((((unsigned short)(*(unsigned char*)(&pkt->b[3])))<<8) + *(unsigned char*)&pkt->b[2]); // Read top word & verify mask
	if((gSettings.radioSubnetMask&temp)==(gSettings.radioSubnetMask&gSettings.radioSubnet))
	{	
		// We received a packet in our subnet
		SI44_LED = !SI44_LED;	// Flash led
		switch(pkt->pktType){
			case DECODED_BAX_PKT : 
			case DECODED_BAX_PKT_PIR : 
			case DECODED_BAX_PKT_SW : {
				// Try decode sensor packets
				if(BaxDecodePkt(pkt))
				{	
					// Make data element for recevied and decoded packets
					DBG_INFO("\r\nBAX SENSOR PKT");
					AddDataElement(TYPE_BAX_PKT,DATA_DEST_ALL, baxElementDataLen, GetOpenStreams(), pkt->b);
				}
				else
				{
					// Make data element for raw packets
					#ifdef ROUTE_UNDECODED_PACKETS
					DBG_INFO("\r\nBAX PKT ENCRYPTED");
					pkt->pktType = -pkt->pktType;
					AddDataElement(TYPE_BAX_PKT, DATA_DEST_ALL, baxElementDataLen, GetOpenStreams(), pkt->b);
					#endif
				}
				break;
			}
			case AES_KEY_PKT_TYPE : 
			case BAX_NAME_PKT : {
				// Make data element for recevied info packets + set sys_cmd flag
				DBG_INFO("\r\nBAX INFO PKT");
				AddDataElement(TYPE_BAX_PKT, DATA_DEST_ALL, baxElementDataLen, (CMD_SYSTEM_FLAG | GetOpenStreams()), pkt->b);
				break;
			}
			default : {
				// Other packets
				DBG_INFO("\r\nBAX PKT UNKNOWN.");
				break;
			}
		} // Switch scope
	}// Subnet mask
}
#endif

// Decode an encrypted packet
unsigned char BaxDecodePkt(BaxPacket_t* pkt)
{
	unsigned char temp[16];
	// Search for an info entry
	BaxDeviceInfo_t* device;
	device = BaxSearchInfo(pkt->address);
	// Check it found one
	if(device == NULL) return FALSE;
	// Decrypt (requires temp buffer)
	aes_decrypt_128(pkt->data,pkt->data,device->info.key,temp);
	// Update last packet list
	BaxAddEntry(device, pkt);
	// Done
	return TRUE;
}

// Adds the current packet to the last entries list
static void BaxAddEntry(BaxDeviceInfo_t* device, BaxPacket_t* pkt)
{
	#if (MAX_BAX_SAVED_PACKETS > 0)
	BaxEntry_t* temp;
	// Early out for no device entries
	if(MAX_BAX_SAVED_PACKETS <= 0) return;
	// Get pointer to oldest in list
	temp = device->entry[MAX_BAX_SAVED_PACKETS-1];
	// Shift the list of *pointers* to remove last entry
	memmove(&device->entry[1],&device->entry[0], ((MAX_BAX_SAVED_PACKETS-1) * sizeof(BaxEntry_t*)));
	// Overwrite the older entry and set time
	temp->time = RtcNow();
	temp->rssi = pkt->rssi;
	temp->pktType = pkt->pktType;
	memcpy(temp->data,pkt->data,sizeof(BaxDataPacket_t));
	// Set first entry to old pointer (pointers rotate and are preserved)
	device->entry[0] = temp;
	// Done
	#endif
	return;
}

// Set this to the callback fptr to enable device discovery. Call at main scope.
void BaxInfoPktDetected (BaxPacket_t* pkt)
{
	// Info structure and pointer stacked
	BaxInfo_t* infoToSave = NULL;
	BaxInfo_t  tempInfo;
	// Early out
	if(pkt==NULL)return;
	BaxEraseInfo(&tempInfo);
	// Check packet type
	if(pkt->pktType == AES_KEY_PKT_TYPE)
	{
		// Copy in parts
		tempInfo.address = pkt->address;
		memcpy(tempInfo.key,pkt->data,AES_BLOCK_SIZE);
		// Indicate we have a new entry
		infoToSave = &tempInfo;
		// Add it to ram
		BaxAddNewInfo(infoToSave);
		DBG_INFO("\r\nNew bax info added.");
	}
	else if (pkt->pktType == BAX_NAME_PKT)
	{
		// Search for a pre-existing info entry for this device
		unsigned short i;
		BaxDeviceInfo_t* device;
		device = BaxSearchInfo(pkt->address);
		// Can't name devices we don't know
		if(device == NULL) return;
		// Format and copy name field
		for(i=0;i<(BAX_NAME_LEN-1);i++)
		{
			// Read each char
			char c = pkt->data[i];
			// Force to alpha numeric plus space/dash
			if(	(c < '0' && c != ' ' && c != '-' && c != '\0') || /* Allow space, dash, null */
				(c > '9' && c < 'A') || /* Remove this ascii range */
				(c > 'Z' && c < 'a') || /* Remove this ascii range */
				(c > 'z') )  			/* Remove end of ascii range */
			c = '_'; 					/* Replace illegal chars with '_' */
			device->info.name[i] = c;
			// Early out on null
			if (c == '\0') break;
		}
		// Null terminate
		device->info.name[BAX_NAME_LEN-1] = '\0';
		DBG_INFO("\r\nNew device name: %s",device->info.name);
		// Save to file too
		infoToSave = &device->info;	
	}
	#ifdef BAX_DEVICE_INFO_FILE
	// If we have a new info entry to save
	if(infoToSave != NULL)
	{
		// Add to file as well
		FSFILE* info_file = FSfopen(BAX_DEVICE_INFO_FILE,"ab");
		if(info_file)
		{
			// Save to file
			if(BaxAddInfoToFile(info_file, infoToSave))
			{
				DBG_INFO("\r\nNew bax info saved to file.");
			}
			// Close
			FSfclose(info_file);
		}
	}
	#endif
	return;
}

// Device discovery callback set - embedded only
void BaxSetDiscoveryCB(void(*CallBack)(BaxPacket_t* pkt))
{
	DBG_INFO("\r\nBax discover: %s",(CallBack)?"ON":"OFF");
	BaxInfoPacketCB = CallBack;
}

// Erase a device entry
static void BaxEraseEntry(BaxEntry_t *entry)
{
	if(entry == NULL) return;
	entry->time = DATETIME_INVALID;
	//memset(entry->pkt,0,sizeof(BaxDataPacket_t)); Not needed
}

// Erase a device info struct
static void BaxEraseInfo(BaxInfo_t* info)
{
	if(info == NULL) return;
	// Erase the info sections
	memset(info,0,sizeof(BaxInfo_t));
}

// Erase sensor info and data, pointers left alone
static void BaxEraseDeviceInfo(BaxDeviceInfo_t* device)
{
	// Erase device info
	BaxEraseInfo(&device->info);
	// Invalidate device data entries
	#if (MAX_BAX_SAVED_PACKETS > 0)
	{
		unsigned short i;
		for(i=0;i<MAX_BAX_SAVED_PACKETS;i++)
		{
			BaxEraseEntry(device->entry[i]);
		}
	}
	#endif
}

// Initialise the device info
void BaxInitDeviceInfo(void)
{
	// Init the device info structure
	unsigned short i = 0;
	
#if(MAX_BAX_SAVED_PACKETS > 0)
	unsigned short j = 0;
#endif
	
	// Assign pointers to entries, clear entries
	for(i=0;i<MAX_BAX_INFO_ENTRIES;i++)
	{
		// Initialise entry pointers
		#if(MAX_BAX_SAVED_PACKETS > 0)
		for(j=0;j<MAX_BAX_SAVED_PACKETS;j++)
		{
			baxDeviceInfo[i].entry[j] = &baxEntries[(i*MAX_BAX_SAVED_PACKETS)+j];
		}
		#endif
		// Wipe all info/entries
		#if(MAX_BAX_INFO_ENTRIES > 0)
		BaxEraseDeviceInfo(&baxDeviceInfo[i]);
		#endif
	}
}

// Add a new info struct to ram
static void BaxAddNewInfo(BaxInfo_t* entry)
{
#if (MAX_BAX_INFO_ENTRIES > 0)
	unsigned long address = entry->address;
	unsigned short i;
#endif
	if(entry == NULL) return;	

	// Erase all duplicate entries for address
	#if (MAX_BAX_INFO_ENTRIES > 0)
	for(i=0;i<MAX_BAX_INFO_ENTRIES;i++)
	{
		if(baxDeviceInfo[i].info.address == address)
		{
			DBG_INFO("\r\nOLD KEY DEL.");
			BaxEraseDeviceInfo(&baxDeviceInfo[i]);
		}
	}
	#endif

	// Add new entries in first empty slot (if duplicate deleted, this will be that slot)
	#if (MAX_BAX_INFO_ENTRIES > 0)
	for(i=0;i<MAX_BAX_INFO_ENTRIES;i++)
	{
		if(baxDeviceInfo[i].info.address == 0ul)
		{	
			DBG_INFO("\r\nNEW KEY ADD.");
			memcpy(&baxDeviceInfo[i].info,entry,sizeof(BaxInfo_t));
			break;
		}
	}
	#endif

	// Dump newest in list if no space
	#if (MAX_BAX_INFO_ENTRIES > 0)
	if(i >= MAX_BAX_INFO_ENTRIES)
	{
		unsigned short j;
		const unsigned short lastIndex = MAX_BAX_INFO_ENTRIES-1;
		DBG_INFO("\r\nOLD KEY REPLACED");
		// Save pointers (this entangles the entry pointers but avoids moving larger memory chunks)
		for(j=0;j<MAX_BAX_SAVED_PACKETS;j++)
			{baxDeviceInfo[lastIndex].entry[j] = baxDeviceInfo[0].entry[j];}
		// Move the memory device info chunk to replace first entry
		memmove(&baxDeviceInfo[0],&baxDeviceInfo[1],sizeof(BaxDeviceInfo_t)*(lastIndex));
		// Clear device info and entry data
		BaxEraseDeviceInfo(&baxDeviceInfo[lastIndex]);
		// Copy in new info structure
		memcpy(&baxDeviceInfo[lastIndex].info,entry,sizeof(BaxInfo_t));
	}
	#endif
	return;
}

// Retrieve a pointer to the info structure using current raw packet
static BaxDeviceInfo_t* BaxSearchInfo(unsigned long address)
{
	#if (MAX_BAX_INFO_ENTRIES > 0)
	unsigned short i;
	for(i=0;i<MAX_BAX_INFO_ENTRIES;i++)
	{
		if(baxDeviceInfo[i].info.address == address)
			return &baxDeviceInfo[i];
	}
	#endif
	return NULL;
}

// Retrieve a name for an address if present
char* BaxGetName(unsigned long address)
{
	// Search for an info entry
	BaxDeviceInfo_t* device;
	device = BaxSearchInfo(address);
	// Check it found one
	if(device == NULL) return NULL;
	// Return pointer
	return device->info.name;
}

// Retrieve the last packet for an address if present
BaxEntry_t* BaxGetLast(unsigned long address, unsigned short offset)
{
	#if (MAX_BAX_SAVED_PACKETS == 0)
		return NULL;
	#else
	// Search for an info entry
	BaxDeviceInfo_t* device;
	device = BaxSearchInfo(address);
	// Check it found one
	if(device == NULL) return NULL;
	// Check offset is valid
	if(offset >= MAX_BAX_SAVED_PACKETS) return NULL;
	// Check entry has been written
	if(device->entry[offset]->time == DATETIME_INVALID) return NULL;
	// Return pointer to entry
	return device->entry[offset];
	#endif
}

// Load an info struct from a file pointer, read upto next entry
unsigned char BaxLoadInfoFromFile (FSFILE* file, BaxInfo_t* read)
{
	// Align to 64 byte block
	signed long filePos = FSftell(file);
	signed long remaining = FSFileSize(file) - filePos;
 	signed long alignment = filePos % 64ul;

	if(alignment != 0)
	{
		FSfseek(file,-(64-alignment),SEEK_CUR);
		remaining += (64-alignment);
	}
	// If there is enough file remaining
	if(remaining < 64)
	{
		// File ended
		return FALSE;
	}

	// Read new entry to pointer
	/* 36 bytes total, PADDED TO 64 BYTES IN FILES */
	// Failure => Read error
	if(FSfread(&read->address, 1, sizeof(uint32_t), file) != sizeof(uint32_t))	// Read 4 bytes into address
		return FALSE;
	if(FSfread(&read->key, 1, AES_BLOCK_SIZE, file) != AES_BLOCK_SIZE)	// Read 16 bytes into key
		return FALSE;
	if(FSfread(&read->name, 1, BAX_NAME_LEN, file) != BAX_NAME_LEN)	// Read 16 bytes into name
		return FALSE;

	// Seek to start of next entry
	FSfseek(file,(64-sizeof(BaxInfo_t)),SEEK_CUR);
	return TRUE;
}

// Save an info struct from a file pointer 
static unsigned char BaxAddInfoToFile (FSFILE* file, BaxInfo_t* info)
{
	// Aligned to 64 byte block
	unsigned char buffer[64];
	signed long filePos = FSftell(file);
 	signed long alignment = filePos % 64ul;
	if(alignment != 0)
	{
		// Over write misaligned section
		FSfseek(file,-alignment,SEEK_CUR);
	}
	// Make new entry, zero pad
	memset(buffer,0,64);
	memcpy(buffer,info,sizeof(BaxInfo_t));
	// Write new entry to file
	if(FSfwrite(buffer,1,64,file) != 64)
	{
		// Write error
		return FALSE;
	}
	// Done
	return TRUE;
}

// Load device info from file
void BaxLoadInfoFile(char* info_file_name)
{
	FSFILE* info_file;
	// Key
	BaxInfo_t read;
	if(MAX_BAX_INFO_ENTRIES == 0) return;
	// Clear all info, initialise pointers
	BaxEraseInfo(&read);
	// Open file
	info_file = FSfopen(info_file_name,"rb");
	if(info_file)
	{
		DBG_INFO("\r\nLoading bax config file.");
		for(;;)
		{
			// Read new entry
			if(BaxLoadInfoFromFile (info_file, &read))
			{
				// Add key
				DBG_INFO("\r\nInfo loaded from file");
				BaxAddNewInfo(&read);
			}
			else
			{
				// Failed
				break;
			}
		}
		// Close
		FSfclose(info_file);
	}
}

// Erases old info values and replaces them with current list from ram
void BaxSaveInfoFile(void)
{
	#ifdef BAX_DEVICE_INFO_FILE
	FSFILE* info_file;
	// Save to file
	if( (MAX_BAX_INFO_ENTRIES) == 0) return;

	info_file = FSfopen(BAX_DEVICE_INFO_FILE,"wb");
	if(info_file)
	{
		unsigned short i;
		DBG_INFO("\r\nReplacing bax config file.");
		for (i=0;i<MAX_BAX_INFO_ENTRIES;i++)
		{
			// Write to file
			if(BaxAddInfoToFile (info_file, &baxDeviceInfo[i].info))
			{
				DBG_INFO("\r\nInfo saved");
			}
			else
			{
				break;
			}
		}
		// Close file
		FSfclose(info_file);
	}
	#endif
	return;
}

// This executes a text radio init script from a file.
// The format is 0x000NRRVV\r, N is CMD number 
// Supports: 0,1,2,3,4,6. 6 is write reg where RR is the reg and VV is the value
static void BaxRfConfigFromFile(FSFILE* input_file)
{
	while(BaxFileCmd(input_file) != 0);
	// Done reading file...
	return;
}

// Reads file for a command, sends it, returns TOTAL command length (2+cmd->len), zero = no further commands
unsigned char BaxFileCmd(FSFILE* input_file)
{
#ifndef BAX_MAX_FILE_LINE_BUFFER
	#define BAX_MAX_FILE_LINE_BUFFER 32
#endif
	char fileLineBuffer[BAX_MAX_FILE_LINE_BUFFER], *line;
	unsigned short read = 0;
	Si44Cmd_t cmd;
	
	// Checks
	if(input_file == NULL) return 0;
	if(Si44RadioState == SI44_HW_ERROR)return 0;

	// Read file line, look for "0x" token at start of command
	for(;;)
	{
		// Read a command from the file
		line = FSfgets(fileLineBuffer, BAX_MAX_FILE_LINE_BUFFER, input_file);
		if(line == NULL) 		break;		// End of file
		else if (*line == '\0')	continue;	// Probably extra CR or LF or empty line
		else if (line[0] != '0' || (line[1] != 'x' && line[1] != 'X'))continue;	// Not starting with '0x', probably white space or comment
		// Read the line in hex mode into itself (safe if outPtr<=readPtr)
		read = ReadHexToBinary((unsigned char*)fileLineBuffer, line+2, BAX_MAX_FILE_LINE_BUFFER);					
		// Set command fields
		cmd.type = fileLineBuffer[0];
		cmd.len = fileLineBuffer[1];
		// Set ptr to third hex char
		cmd.data = &fileLineBuffer[2];
		// Check command length is ok, skip if not
		if((read < 2)  || cmd.len != (read-2))
		{
			DBG_INFO("\r\nMalformed hex command in file");
			continue;
		}
		// Send command
		Si44Command(&cmd, NULL);

		// Output events generated. Get line (or packet) from transport
        // (comm_gets not defined unless SERIAL_READ_BUFFER_SIZE is set)
		#if !defined(__C30__) && defined(SERIAL_READ_BUFFER_SIZE)
		{
			// Wait up to 10ms for a response
			unsigned long long waitUntil = MillisecondsEpoch() + 10;
			for(;;)
			{
				const char*	line = comm_gets(&gSettings);
				if(line != NULL)
				{
					if(line != NULL)
					{
						// Debug out
						DBG_INFO("\r\nCmd:");
						DBG_DUMP(fileLineBuffer,(2 + fileLineBuffer[1]));

						read = ReadHexToBinary((unsigned char*)fileLineBuffer, line, BAX_MAX_FILE_LINE_BUFFER);
						DBG_INFO("Evt:");
						DBG_DUMP(fileLineBuffer,(3 + fileLineBuffer[2]));;
						if(fileLineBuffer[1] != SI44_OK){DBG_INFO("-ERROR!");}
						break;
					}
				}
				if(MillisecondsEpoch() >= waitUntil)
				{ 
					// Debug out
					DBG_INFO("\r\nCmd:");
					DBG_DUMP(fileLineBuffer,(2 + fileLineBuffer[1]));
					DBG_INFO("Evt:No response.");
					break;
				}
			}
		}
		#endif
	}// For(;;) whole file.
	return (unsigned char) read;
}

// Channel survey required direct event access - not supported over transport (in this way)
#if defined(BAX_RF_SURVEY_OUT_FILE) && defined(__C30__)
static void BaxChannelSurvey(FSFILE* output_file)
{
	#define BMP_LINES				256ul 		/* Bitmap lines per channel*/
	#define SURVEY_TIME_PER_CH_SECONDS	4		/* Seconds measured per channel*/
	#define SURVEY_SAMPLE_INTERVAL		1		/* Sample interval ms*/
	#define SURVEY_POINTS_PER_PIX		((1000ul*SURVEY_TIME_PER_CH_SECONDS)/(SURVEY_SAMPLE_INTERVAL*BMP_LINES))
	int i,j,k;
	unsigned char ledVal = 0x00;
	unsigned char temp[4];
	Si44Reg_t ch_ctrl[2];
	const Si44Reg_t si44_rssi_read[2] = {Si44_MAKE_LIST_VAL(Si44_RSSI,	1),	SI44_REG_TYPE_EOL};
	Si44Cmd_t readRssi = {SI44_READ_REG_LIST, sizeof(si44_rssi_read), (void*)si44_rssi_read};
	Si44EventCB_t CBsave = Si44EventCB;

	// Change channel script
	Si44Cmd_t cmdSetChList[] = {	{SI44_CMD_STANDBY, 		0,				NULL},
									{SI44_WRITE_REG_LIST, 	sizeof(ch_ctrl),(void*)ch_ctrl},
									{SI44_RX, 				0,				NULL},
									{SI44_CMD_EOL,			0,				NULL}};
	
	// Assumes radio is ready for survey and in idle/rx/standby
	// Checks
	if(output_file == NULL) return;
	if(Si44RadioState == SI44_HW_ERROR)return;

	// Disable handler
	Si44EventCB = NULL;	

	// Write header - 256 channels, N samples per channel
	BitmapWriteHeader(output_file, BMP_LINES, (long)-256, 24);

	// Set rx on
	Si44Command(resumeRx, NULL);

	for(i=0;i<256;i++)
	{
		Si44Event_t* evt;
		// RX/TX carrier freq channel settings
		ch_ctrl[0] = Si44_MAKE_LIST_VAL_NB(Si44_Frequency_Hopping_Ch,i);
		ch_ctrl[1] = SI44_REG_TYPE_EOL;
		// Set channel
		Si44CommandList(cmdSetChList);
		// Write first pix
		temp[0] = i;
		temp[1] = 0;
		temp[2] = 255-i;
		FSfwrite(temp,1,3,output_file);
		for(j=0;j<(BMP_LINES-1);j++)
		{
			unsigned short ave,max,min;
			// Make a point sample
			ave = max = 0; min = 255;
			for(k=0;k<SURVEY_POINTS_PER_PIX;k++)
			{
				unsigned char rssi;
				// Brief wait for RX settle / sample spacing
				DelayMs(SURVEY_SAMPLE_INTERVAL);
				// Check pkt not received
				if(Si44RadioState != SI44_RXING)
					Si44Command(resumeRx, NULL); // Rx on
				// Send read rssi cmd
				evt = Si44Command(&readRssi, &rssi);
				// Process values
				rssi = *(unsigned char*)evt->data;
				ave += rssi;
				if(max < rssi) max = rssi;
				if(min > rssi) min = rssi;
			}
			// Make pix
			temp[0] = ave/SURVEY_POINTS_PER_PIX;
			temp[1] = max;
			temp[2] = min;
			// Output to file, first column is channel num
			FSfwrite(temp,1,3,output_file);
		}
		RtcSwwdtReset();
		// Flash LED so user knows whats going on
		if(ledVal == 0x00) 			ledVal = 0x80;
		else if (ledVal == 0xFF) 	ledVal = 0x7f;
		if (ledVal & 0x80)	{ledVal>>=1;ledVal|=0x80;}
		else				{ledVal>>=1;}
		ledOveride = (unsigned short)ledVal << 8 | ledVal;
		ledOverideTimer = 20*SURVEY_TIME_PER_CH_SECONDS;
	}

	// Restore CB and reset receiver
	Si44EventCB = CBsave;
	Si44Command(resumeRx, NULL);
	return;
}
#endif
//EOF
