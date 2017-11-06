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

#ifndef _BAX_RX_H_
#define _BAX_RX_H_

#include <stdint.h>

// Headers
#ifdef __C30__
	#include "HardwareProfile.h"
	#include "Peripherals/Rtc.h"
	#ifdef USE_FAT_FS // For FSFILE typedef
		#include "FatFs/FatFsIo.h"
	#else
		#include "MDD File System/FSIO.h"
	#endif
#else
	#include <stdio.h>
	#include "BaxUtils.h"
	#include "Config.h"
	//typedef uint32_t DateTime;
#endif
 
// Definitions
#define AES_KEY_PKT_TYPE	0		/* Packet type for encryption packets */
#define BAX_NAME_PKT		4		/* Name type pkt format */
#define DECODED_BAX_PKT		1		/* BAX2.0 format */
#define DECODED_BAX_PKT_PIR	2		/* BAX2.0 format */
#define DECODED_BAX_PKT_SW	3		/* BAX2.0 format */
#define ENCRYPTED_PKT_TYPE_OFFSET	127	/* Encrypted packet types are the normal types but negative i.e. >127 if unsigned */

#define RAW_PKT_LIMIT		127		/* Legacy define */

// Debug packet types
#define HEADER_BYTE_MSB_RAW			'R'	/*Added to packet header[3] for RAW modes*/ 
#define PACKET_TYPE_RAW_UINT8_x14	5	/*14 data bytes after pktNum & txPwr*/
#define PACKET_TYPE_RAW_SINT8_x14	6	/*14 data bytes after pktNum & txPwr*/
#define PACKET_TYPE_RAW_UINT16_x7	7	/*7 data shorts after pktNum & txPwr*/
#define PACKET_TYPE_RAW_SINT16_x7	8	/*7 data shorts after pktNum & txPwr*/

#ifndef AES_BLOCK_SIZE
	#define AES_BLOCK_SIZE		16
#elif (AES_BLOCK_SIZE != 16)
	#error "AES block size must be 16 for this driver"
#endif
#define BAX_PACKET_SIZE		(4+1+1+AES_BLOCK_SIZE)
#define BAX_PKT_DATA_LEN	AES_BLOCK_SIZE
#define BAX_NAME_LEN		AES_BLOCK_SIZE	/* N.b. Including null (15+1)*/

// Storage of bax keys and historical packets in ram - set externally in hw profile

//#define MAX_BAX_INFO_ENTRIES	20 	/*Number of devices we save the keys for, 36 bytes ram each*/
//#define MAX_BAX_SAVED_PACKETS	2	/*Number historical packets saved, 20 bytes each multiplied by max keys*/

// Types
// Describes a generalised bax packet, data portion described below
typedef union BaxPacket_tag {
	struct {
		/* Locally added */
		uint32_t address;			//b[0] address header (4)
		unsigned char rssi;				//b[4] rssi header (1)
		/* Over ridden by decrypter if unknown, set negative*/
		signed char 	pktType;		//b[5] encryption data starts after here for link pkts (1)
		/* Data */
		unsigned char data[BAX_PKT_DATA_LEN];//b[6] - b[21], (16) may be safely cast to one of following
	};
	unsigned char b[4+1+1+AES_BLOCK_SIZE]; //+4 for address +1 for rssi +1 for type (22 bytes)
} BaxPacket_t;

// The raw packet data structure (after decryption)
typedef struct BaxSensorPacket_tag {
		/* Data */
		unsigned char 	pktId; 			//b[0] 
		signed char 	xmitPwrdBm;		//b[1]
		unsigned short 	battmv;			//b[2]
		unsigned short	humidSat;		//b[4]
		signed short 	tempCx10;		//b[6]
		unsigned short 	lightLux;		//b[8]
		unsigned short	pirCounts;		//b[10]
		unsigned short	pirEnergy;		//b[12]
		unsigned short	swCountStat;	//b[14]	
} BaxSensorPacket_t;

// The name packet data structure
typedef struct {
		/* Name */
		char name[BAX_NAME_LEN];
} BaxNamePacket_t;

// The blank/inspecific packet data structure
typedef struct {
		/* Raw bytes */
		char data[BAX_PKT_DATA_LEN];
} BaxDataPacket_t;

// The key packet data structure
typedef struct {
		/* Encryption key */
		unsigned char aesKey[AES_BLOCK_SIZE];
} BaxLinkPacket_t;

// The bax info structure for the decryption key and user friendly name
typedef struct {				/*36 bytes total, PADDED TO 64 BYTES IN FILES*/
		uint32_t address; 		/*4 bytes*/
		uint8_t key[AES_BLOCK_SIZE];/*16 bytes*/
		char name[BAX_NAME_LEN];/*16 bytes*/
}BaxInfo_t;

// The historical packet type storing device packet history
typedef struct {			/*22 bytes total*/
	DateTime time;			/*4 bytes timestamp*/
	unsigned char rssi;		/*1 byte rssi (raw)*/
	signed char pktType;	/*1 byte packet type*/
	unsigned char data[BAX_PKT_DATA_LEN];/*16 bytes data*/
}BaxEntry_t; 

// The structure holding device info
typedef struct {
	BaxInfo_t info;
	#if (MAX_BAX_SAVED_PACKETS > 0)
	BaxEntry_t *entry[MAX_BAX_SAVED_PACKETS];
	#endif
}BaxDeviceInfo_t;

// Globals
// Externed for debug only
extern BaxDeviceInfo_t baxDeviceInfo[];
extern BaxEntry_t baxEntries[];

// RSSI to dBm macro
#define RssiTodBm(_c) ((signed char)-128 + ((unsigned char)_c>>1))

// Includes

// Prototypes
// Call first
void BaxRxInit(void);
// Intermittently, used to check for HW errors
void BaxRxTasks(void);
// Erase saved bax info on disk, replace with ram copy
void BaxSaveInfoFile(void);
// Load bax info from disk
void BaxLoadInfoFile(char* info_file_name);
// Init script file reader
unsigned char BaxFileCmd(FSFILE * input_file);
// Retrieve device info/data
char* BaxGetName(unsigned long address);
BaxEntry_t* BaxGetLast(unsigned long address, unsigned short offset);
unsigned char BaxDecodePkt(BaxPacket_t* pkt);
// Device discovery setter
extern void(*BaxInfoPacketCB)(BaxPacket_t* pkt);
void BaxSetDiscoveryCB(void(*CallBack)(BaxPacket_t* pkt));
void BaxInfoPktDetected (BaxPacket_t* pkt); /*Private*/
// Add info struct to ram
void BaxAddKey(BaxInfo_t* key);
// Initialise the info structure
void BaxInitDeviceInfo(void);
unsigned char BaxLoadInfoFromFile (FSFILE* file, BaxInfo_t* read);
#endif
//EOF
