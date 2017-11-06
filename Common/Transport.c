#ifdef _WIN32
	#define _CRT_SECURE_NO_DEPRECATE
#else
	#include <string.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "Config.h"
#include "Serial.h"
#include "UDP.h"
#include "AsciiHex.h"
#include "SlipUtils.h"
#include "BaxUtils.h"
#include "Peripherals/Si44.h"
#include "BaxRx.h"
#include "Si44_config.h"

// Debug setting
#undef DEBUG_LEVEL
#define DEBUG_LEVEL	0
#define DBG_FILE dbg_file
#if (DEBUG_LEVEL > 0)||(GLOBAL_DEBUG_LEVEL > 0)
static const char* dbg_file = "transport";
#endif
#include "Debug.h"

// Prototypes
void EventCB (Si44Event_t* evt);
void BaxPacketEvent(unsigned char* packedPkt);
int BaxProcessUnit(unsigned char* packedUnit);

extern void BaxUnpackPkt(unsigned char* buffer, BaxPacket_t* packet);
extern void BaxRepackPkt(BaxPacket_t* packet, unsigned char* buffer);
extern void BaxUnpackSensorVals(BaxPacket_t* packet, BaxSensorPacket_t* sensor);

// Code
int OpenTransport(Settings_t *settings)
{
	int ret = FALSE;
	switch(settings->source) {
		case 'S' : {
			settings->fd = openport(settings->input, 1, 10);
			if(settings->fd < 0) 
			{
				ErrorExit("Could not open com port %s",settings->input);
			}
			settings->inGetc = getcSerial;
			settings->outPutc = putcSerial;
			ret = TRUE;
			break;
		}
		case 'F' : {
			if(settings->input == NULL) break;

            if(*settings->input == '-') 
            {
                settings->inputFile = stdin;
            }
            else
            {
                settings->inputFile = fopen(settings->input, "rb");
            }

			if(settings->inputFile != NULL)
			{
				settings->inGetc = getcFile;
				settings->outPutc = putcFile;
				ret = TRUE;
			}
			else
			{
				ErrorExit("Could not open input file %s",settings->input);
			}
			break;
		}
		case 'U' : {
			int fieldCount, offset = 0;
			char* fields[4];
			DBG_INFO("\r\nUDP receive mode");
			// Parse input string descriptor d field
			for(fieldCount=0;fieldCount<4;fieldCount++)
			{
				// Set field pointer
				fields[fieldCount] = &settings->input[offset];
				// Check for a null char, end of string
				if(settings->input[offset] == '\0')
					break;
				// Advance to next null or '+'
				while(settings->input[offset] != '\0' && settings->input[offset] != '+') 
				{
					if(settings->input[offset] == ':' && fieldCount == 0)
					{
						// User specified a port number after the ip address, read port number
						settings->udpPort = atoi(&settings->input[offset+1]);
						// Convert colon to null to make valid ipaddress string
						settings->input[offset] = '\0';
					}
					offset++;
				}
				// Replace plus with null char
				if(settings->input[offset] == '+')
				{
					settings->input[offset] = '\0';
					offset++;
				}
			}
			// Check for correct number
			if(fieldCount!=4)
			{
				ErrorExit("Wrong argument number for udp");
			}
			else
			{
				DBG_INFO("\r\nip=%s, mac=%s, un=%s, pw=%s",fields[0],fields[1],fields[2], fields[3]);
			}
			gSettings.ipAddress = fields[0];
			gSettings.destMac = fields[1];
			gSettings.username = fields[2];
			gSettings.password = fields[3];
			// Open udp connection
			if(BaxUdpOpen(&gSettings))
			{
				DBG_INFO("\r\nUDP open success");
				ret = 1;
			}
			else
			{
				ErrorExit("UDP open failed:\r\nip=%s\r\nmac=%s\r\nnun=%s\r\npw=%s",fields[0],fields[1],fields[2],fields[3]);
			}
			break;
		}
		default :
			ErrorExit("Unknown/Unimplemented source: %c",settings->source);
			break;
	}// Switch
	return ret;
}

int CloseTransport(Settings_t* settings)
{
	int ret = FALSE;
	switch(settings->source) {
		case 'S' : {
			if (settings->fd < 0)
			{
				DBG_INFO("Can't close invalid serial port %s",settings->input);
			}
			closeport(settings->fd);
			ret = TRUE;
			break;
		}
		case 'F' : {
			if(settings->inputFile == NULL)break;
			fclose(settings->inputFile);
			settings->inputFile = NULL;
			break;
		}
		case 'U' : {
			DBG_INFO("UDP close");
			UdpCleanup (&gSettings);
			break;
		}
		default :
			DBG_ERROR("Cant close unknown/unimplemented source: %c",settings->source);
			break;
	}// Switch
	return ret;
}

int OpenOutput(Settings_t* settings)
{
	int ret = TRUE;
	if(settings->output == 'F') 
	{
		if(settings->outFile != NULL)
			settings->outputFile = fopen(settings->outFile,"wb");
		else 
			settings->outputFile = NULL;
		if(settings->outputFile == NULL)
		{
			ErrorExit("Can't open output file %s",settings->outFile);
		}
	}
	else if(settings->output == 'S') 
	{
		settings->outputFile = stdout;
		setvbuf ( stdout , NULL , _IOLBF , 1024 );
	}
	else
	{
		ErrorExit("Unknown output setting?");
	}
	return ret;
}

int CloseOutput(Settings_t* settings)
{
	int ret = TRUE;
	if(settings->outputFile != NULL)
	{
		if(settings->outputFile != stdout)
		{
			fclose(settings->outputFile);
			settings->outputFile = NULL;
		}
		else
		{
			fflush(settings->outputFile);
		}
		ret = TRUE;
	}
	return ret;
}

void TransportTasks(Settings_t* settings)
{
	const char* line;
	// This buffer will encapsulate the data 
	Si44Event_t event;
	unsigned char rawData[MAX_BINARY_PACKET_LEN];
	unsigned short length = 0;

	// Early out if no reader
	if(settings->inGetc == NULL) return;

	// Get line (or packet) from transport and parse it
	line = comm_gets(&gSettings);
	if(line != NULL)
	{
		/*
			For streams there are two modes, one is a binary unit of 32 bytes (bax file mode)
			and the other is the event pass through (raw radio modes).
		*/
		if(settings->encoding == 'H')
		{
			length = ReadHexToBinary(rawData, line, MAX_BINARY_PACKET_LEN);
		}
		else if (settings->encoding == 'S')
		{
			length = ReadFromSlip(rawData, line, MAX_BINARY_PACKET_LEN);
		}
		else if (settings->encoding == 'R')
		{
			memcpy(rawData,line,BINARY_DATA_UNIT_SIZE);
			length = BINARY_DATA_UNIT_SIZE;
		}
		else
		{
			DBG_ERROR("Read mode unknown");
		}
	}

	// Pass on none zero length events
	if(length > 0)
	{
		DBG_INFO("\r\nNew event read %u bytes",length);
		/*
			If we are reading a file we need to output the data
			entries through the bax receiver. I.e. convert them 
			back into a receive event.
		*/
		// Make an event out of binary unit
		if(settings->format == 'U')
		{
			// Check length, discard wrong length packets
			if(length != BINARY_DATA_UNIT_SIZE) 
			{
				DBG_ERROR("Binary unit not 32 bytes?");
				return;
			}
			// Process unit with packet handler
			BaxProcessUnit(rawData);
		}
		// Otherwise the binary data is the event but needs reforming to be safe (non-aligned structs)
		else if (settings->format == 'E')
		{
			event.type = rawData[0];
			event.err = rawData[1];
			event.len = rawData[2];
			event.data = &rawData[3]; 	
			// Process event with radio handler, may forward to pkt handler
			EventCB(&event);		
		}
		else
		{
			DBG_ERROR("Unknown input format");
		}
	}
	
	// Output commands

	// Input from transport (TODO)
	
	return;
}

void TransportCheckHardware(void)
{
	// Read gpio control regs - checks for reset condition
	const Si44Reg_t si44_read_gpioctrl[] = {
		Si44_MAKE_LIST_VAL(Si44_GPIO0_Configuration,	1),		// RX state 
	   	Si44_MAKE_LIST_VAL(Si44_GPIO1_Configuration,	1),		// TX state
		SI44_REG_TYPE_EOL // End of list
	};
	const Si44Cmd_t bax_check[] = {
		{SI44_READ_REG_LIST,  	sizeof(si44_read_gpioctrl),	(void*)si44_read_gpioctrl},
		{SI44_CMD_EOL,			0,	NULL} // Issued as single cmd so wont read this bit
	};	
	// Check radio ok - the read will be checked by event handler
	if(Si44EventCB != NULL)
		Si44Command(bax_check, NULL);
}

// Si44 radio event handler
void EventCB(Si44Event_t* evt)
{
	static Si44RadioState_t StateBeforeTx = SI44_OFF;
	// Called for every radio event 
	if(evt == NULL) return;
	// Errors
	if(evt->err != SI44_OK)
	{
		DBG_INFO("\r\nSI44 ERROR, ERR %u",evt->err);	
	}
	// Process event
	switch(evt->type) {
		case SI44_READ_PKT : {
			DBG_INFO("\r\nSI44 PKT EVT");
			if(Si44RadioState != SI44_RXING)
				Si44Command(resumeRx, NULL); 	/*Re-enable RX, packet is already read out*/
			BaxPacketEvent(evt->data); 			/*Process packet*/
			break;
		}
		case SI44_RESET	: {
			Si44RadioState = SI44_OFF;
			DBG_INFO("\r\nSI44 RESET");
			break;
		}
		case SI44_CMD_STANDBY : {
			Si44RadioState = SI44_STANDBY;
			DBG_INFO("\r\nSI44 STANDBY");
			break;
		}
		case SI44_CMD_IDLE : {
			Si44RadioState = SI44_IDLE;
			DBG_INFO("\r\nSI44 IDLE");
			break;
		}	
		case SI44_TX : {
			StateBeforeTx = Si44RadioState;
			Si44RadioState = SI44_TXING;
			DBG_INFO("\r\nSI44 TXING");
			break;
		}
		case SI44_TX_DONE : {
			Si44RadioState = StateBeforeTx;
			DBG_INFO("\r\nSI44 TXED");
			break;
		}
		case SI44_RX : {
			Si44RadioState = SI44_RXING;
			DBG_INFO("\r\nSI44 RX");
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
			if(evt->len == 2) // Probably the response to checking the register values
			{
				if(evt->data[0] != GPIO0_SETTING || evt->data[1] != GPIO1_SETTING)
				{	
					DBG_ERROR("si44 cfg mismatch");
					gStatus.radio_state = ERROR_STATE;	// Indicate error
					Si44RadioState = SI44_HW_ERROR;
				}
			}
			// Incase its ever caught not receiving
			if(Si44RadioState == SI44_IDLE)
				Si44Command(resumeRx, NULL);
			break;
		}	
		case SI44_EVT_ERR : {
			Si44RadioState = SI44_HW_ERROR;
			DBG_INFO("\r\nSI44 ERROR");			
			break;
		}
		default	: {
			DBG_INFO("\r\nSI44 UNKNOWN EVENT");
			break;
		}
	};// Switch
}

// The BAX packet event handler
void BaxPacketEvent(unsigned char* packedPkt)
{
	DateTime timestamp;
	char buffer[SERIAL_WRITE_BUFFER_SIZE];
	BaxPacket_t pkt;

	// Check the data length and data
	if(packedPkt == NULL) return;

	// Make a binary unit type by adding a timestamp and data number
	timestamp = RtcNow();
	memcpy(buffer,&gSettings.dataNum,4);// Data number
	memcpy(buffer+4,&timestamp,4);		// Timestamp
	buffer[8] = 0;						// Continuation flag
	gSettings.dataNum++;				// Increment data num
	memcpy(buffer+BAX_OFFSET_BINARY_UNIT,packedPkt,BAX_PACKET_SIZE);

	// Unpack packet to readable type and allow decryption to work
	BaxUnpackPkt(packedPkt, &pkt);	

	// Switch on pkt type
	switch(pkt.pktType){
		case (unsigned char)DECODED_BAX_PKT : 
		case (unsigned char)DECODED_BAX_PKT_PIR : 
		case (unsigned char)DECODED_BAX_PKT_SW : {
			// Try decrypt
			if(BaxDecodePkt(&pkt))
			{
				DBG_INFO("\r\nDECODED BAX SENSOR PKT");
				// Re-pack decrypted pkt
				BaxRepackPkt(&pkt, (unsigned char*)buffer+BAX_OFFSET_BINARY_UNIT);
				break;
			}
			else
			{
				signed char* type = (signed char*)buffer+BAX_OFFSET_BINARY_UNIT+BAX_FIELD_OS_pktType;
				DBG_INFO("\r\nENCRYPTED BAX SENSOR PKT");
				// Not decrypted, leave payload alone, negate type
				*type = -(*type);
				break;
			}
		}
		case (unsigned char)AES_KEY_PKT_TYPE : {
			// Link packet
			DBG_INFO("\r\nBAX KEY PKT");
			break;
		}
		case (unsigned char)BAX_NAME_PKT : {
			// Make data element for recevied info packets + set sys_cmd flag
			DBG_INFO("\r\nBAX NAME PKT");
			break;
		}
		case (unsigned char)PACKET_TYPE_RAW_SINT8_x14 :
		case (unsigned char)PACKET_TYPE_RAW_UINT8_x14 : {
			// Make data element for recevied raw data packet
			DBG_INFO("\r\nBAX RAW 8BIT DATA PKT");
			break;
		}
		case (unsigned char)PACKET_TYPE_RAW_SINT16_x7 :
		case (unsigned char)PACKET_TYPE_RAW_UINT16_x7 : {
			// Make data element for recevied raw data packet
			DBG_INFO("\r\nBAX RAW 16BIT DATA PKT");
			break;
		}
		default : {
			DBG_INFO("\r\nBAX UNKNOWN PACKET");
			break;
		}
	} // Switch scope

	// Now process binary unit
	BaxProcessUnit((unsigned char*) buffer);

	return;
}

int BaxProcessUnit(unsigned char* packedUnit)
{
	int outLen = 0, sent = 0;
	BaxPacket_t pkt;
	BaxSensorPacket_t sensor;

	char buffer[SERIAL_WRITE_BUFFER_SIZE];
	// Checks 
	if(packedUnit == NULL) return 0;

	// Check how filtering options apply...
	BaxUnpackPkt(packedUnit + BAX_OFFSET_BINARY_UNIT, &pkt);	
	// Try decoding encrypted pkts using receiver function
	if((unsigned char)pkt.pktType > (unsigned char)ENCRYPTED_PKT_TYPE_OFFSET)
	{
		// If decoded, set type to decoded
		if(BaxDecodePkt(&pkt)) 
		{
			pkt.pktType = (unsigned char)-pkt.pktType;
		}
	}
	switch(pkt.pktType){
		case (unsigned char)AES_KEY_PKT_TYPE : {
			if(gSettings.linkMode & (unsigned char)LINK_FLAG_ADD)
			{
				BaxInfoPktDetected(&pkt);
			}
			if(!(gSettings.filter & (unsigned char)FILTER_FLAG_PAIRING))
			{
				// Not sending pairing packets
				return 0;
			}
			break;
		}
		case (unsigned char)BAX_NAME_PKT : {
			if(gSettings.linkMode & (unsigned char)LINK_FLAG_ADD)
			{
				BaxInfoPktDetected(&pkt);
			}
			if(!(gSettings.filter & (unsigned char)FILTER_FLAG_NAME))
			{
				// Not sending pairing name
				return 0;
			}
			break;
		}
		case (unsigned char)DECODED_BAX_PKT : 
		case (unsigned char)DECODED_BAX_PKT_PIR :
		case (unsigned char)DECODED_BAX_PKT_SW : {
			if(!(gSettings.filter & (unsigned char)FILTER_FLAG_DECODED))
			{
				// Not sending decoded pkts
				return 0;
			}
			break;
		}
		case (unsigned char)PACKET_TYPE_RAW_UINT8_x14 :
		case (unsigned char)PACKET_TYPE_RAW_SINT8_x14 :
		case (unsigned char)PACKET_TYPE_RAW_UINT16_x7 :
		case (unsigned char)PACKET_TYPE_RAW_SINT16_x7 : {
			if(!(gSettings.filter & (unsigned char)FILTER_FLAG_RAW))
			{
				// Not sending raw data pkts
				return 0;
			}
			break;
		}
		default : { /*( > ENCRYPTED_PKT_TYPE_OFFSET and other unknow packets)*/
			if(!(gSettings.filter & (unsigned char)FILTER_FLAG_ENCRYPTED))
			{
				// Not sending decoded pkts
				return 0;
			}
			break;
		}
	}// Packet type switch

	// Send data out to caller in correct mode
	switch(gSettings.outMode) {
		case 'R' : {
			// Raw binary hex mode
			sent = fwrite(packedUnit,sizeof(char),BINARY_DATA_UNIT_SIZE,gSettings.outputFile);
			break;
		}
		case 'H' : {
			// Encode into ascii hex
			outLen = WriteBinaryToHex(buffer, packedUnit, BINARY_DATA_UNIT_SIZE, FALSE);
			// Write
			sent = fwrite(buffer,sizeof(char),outLen,gSettings.outputFile);
			outLen += 2;
			sent += fwrite("\r\n",sizeof(char),2,gSettings.outputFile);
			
			break;
		}
		case 'S' : {
			// Encode as slip
			buffer[0] = SLIP_START_OF_PACKET;
			outLen = WriteToSlip((unsigned char*) &(buffer[1]), packedUnit, BINARY_DATA_UNIT_SIZE, FALSE);
			buffer[1+outLen] = SLIP_END_OF_PACKET;
			// Write
			outLen += 2;
			sent = fwrite(buffer,sizeof(char),outLen,gSettings.outputFile);

			break;
		}
		case 'C' : {
			// CSV output
			//sent += fprintf(gSettings.outputFile,"%lu,",UnpackLE32(packedUnit, 0));			// Data number
			sent = fprintf(gSettings.outputFile,"%s,",RtcToString(UnpackLE32(packedUnit,4)));	// Date, Time
			WriteBinaryToHex(buffer, packedUnit+BAX_OFFSET_BINARY_UNIT, 4, TRUE);				// Address (big endian print)
			sent += fprintf(gSettings.outputFile,"%s,",buffer);
			sent += fprintf(gSettings.outputFile,"%d,%d,",RssiTodBm(pkt.rssi), pkt.pktType);

			switch(pkt.pktType){
				case (unsigned char)DECODED_BAX_PKT : 
				case (unsigned char)DECODED_BAX_PKT_PIR : 
				case (unsigned char)DECODED_BAX_PKT_SW : {
					BaxUnpackSensorVals(&pkt, &sensor);
					sent += fprintf(gSettings.outputFile,"%u,%d,%u,%u.%02u,",
						sensor.pktId, 			// pktId
						sensor.xmitPwrdBm, 		// txPwr dbm
						sensor.battmv, 			// battmv
						sensor.humidSat >> 8, 	// humidSat MSB
						(((signed short)39*(sensor.humidSat & 0xff))/100));// humidSat LSB
					sent += fprintf(gSettings.outputFile,"%d,%u,%u,%u,%u\r\n",
						sensor.tempCx10,			// tempCx10
						sensor.lightLux, 			// lightLux
						sensor.pirCounts,			// pirCounts
						sensor.pirEnergy,			// pirEnergy
						sensor.swCountStat); 		// swCountStat
					break;
				}
				case (unsigned char)PACKET_TYPE_RAW_UINT8_x14 : {
					const unsigned char* val = &pkt.data[2];
					sent += fprintf(gSettings.outputFile,"%d,%d,",pkt.data[0],pkt.data[1]);				// pktId, txPwr
					sent += fprintf(gSettings.outputFile,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
									val[0],val[1],val[2],val[3],val[4],val[5],val[6],
									val[7],val[8],val[9],val[10],val[11],val[12],val[13]);
					break;
				}
				case (unsigned char)PACKET_TYPE_RAW_SINT8_x14 : {
					const signed char* val = (const signed char*)&pkt.data[2];
					sent += fprintf(gSettings.outputFile,"%d,%d,",pkt.data[0],pkt.data[1]);				// pktId, txPwr
					sent += fprintf(gSettings.outputFile,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
									val[0],val[1],val[2],val[3],val[4],val[5],val[6],
									val[7],val[8],val[9],val[10],val[11],val[12],val[13]);
					break;
				}
				case (unsigned char)PACKET_TYPE_RAW_UINT16_x7 : {
					const unsigned char* b = &pkt.data[2];
					unsigned short i, val[7];
					sent += fprintf(gSettings.outputFile,"%d,%d,",RssiTodBm(pkt.rssi), pkt.pktType);	// RSSI, pktType,
					sent += fprintf(gSettings.outputFile,"%d,%d,",pkt.data[0],pkt.data[1]);				// pktId, txPwr
					// Unpack shorts
					for(i=0;i<7;i++){val[i] = UnpackLE16((unsigned char*)b, (i*2));}
					sent += fprintf(gSettings.outputFile,"%u,%u,%u,%u,%u,%u,%u\r\n",val[0],val[1],val[2],val[3],val[4],val[5],val[6]);
					break;
				}
				case (unsigned char)PACKET_TYPE_RAW_SINT16_x7 : {
					const unsigned char* b = &pkt.data[2];
					signed short i, val[7];
					sent += fprintf(gSettings.outputFile,"%d,%d,",RssiTodBm(pkt.rssi), pkt.pktType);	// RSSI, pktType,
					sent += fprintf(gSettings.outputFile,"%d,%d,",pkt.data[0],pkt.data[1]);				// pktId, txPwr
					// Unpack shorts
					for(i=0;i<7;i++){val[i] = (signed short)UnpackLE16((unsigned char*)b, (i*2));}
					sent += fprintf(gSettings.outputFile,"%d,%d,%d,%d,%d,%d,%d\r\n",val[0],val[1],val[2],val[3],val[4],val[5],val[6]);
					break;
				}
				default : {
					// Raw binary
					WriteBinaryToHex(buffer, pkt.data, BAX_PKT_DATA_LEN, FALSE);				// Raw undecoded packets
					sent += fprintf(gSettings.outputFile,"%s\r\n",buffer);
					break;
				}
			}
			outLen = sent; // Prevent next check showing error
			break;
		}
		default : {
			DBG_INFO("\r\nUnknown output format");
			break;
		}
	}
	
	fflush(gSettings.outputFile);		// Flush to stdout

	// Check
	if(outLen != sent)
	{
		DBG_ERROR("Output write error");
		return -1;
	}
	return sent;
}
