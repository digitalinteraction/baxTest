// Configuration file for BaxRx.exe
#ifndef CONFIG_H_
#define CONFIG_H_

// Includes for types
//#ifndef SOCKET
#ifdef _WIN32
	#include <windows.h>
//	typedef UINT_PTR SOCKET;
#else
	#ifndef SOCKET
		//typedef int SOCKET;
		#define SOCKET int
	#endif
#endif
//#endif
#include <stdint.h>
#include <stdio.h>

// Debugging
#define ERROR_FILE	"ERRORS.TXT"
#define GLOBAL_DEBUG_LEVEL 2

// Link options
#define LINK_FLAG_FILE	0x01
#define LINK_FLAG_PAIR	0x02
#define LINK_FLAG_ADD	0x04

// Filter options
#define FILTER_FLAG_PAIRING		0x01
#define FILTER_FLAG_NAME		0x02
#define FILTER_FLAG_DECODED		0x04
#define FILTER_FLAG_ENCRYPTED	0x08
#define FILTER_FLAG_RAW			0x10

// BAX device memory
#define MAX_BAX_INFO_ENTRIES 	255
#define MAX_BAX_SAVED_PACKETS 	1
#define MAX_BINARY_PACKET_LEN	256
#define BAX_DEVICE_INFO_FILE	gSettings.baxInfoFile
#define BAX_RF_SETTINGS_FILE	gSettings.baxConfigFile

// Reader 
#define SERIAL_READ_BUFFER_SIZE 256
#define SERIAL_WRITE_BUFFER_SIZE 256

// Bax init script file reader
//#define BAX_MAX_FILE_LINE_BUFFER 256

// UDP defines
#define BAX_UDP_PORT_FORWARDING		30303
#define BAX_UDP_PORT_DISCOVERY		30304

// Types
struct Settings_tag;
typedef int (*GetByte_t)(struct Settings_tag* settings);
typedef int (*PutByte_t)(struct Settings_tag* settings, unsigned char b);

// Generic state type
typedef enum {
	NOT_PRESENT = -2,
	ERROR_STATE = -1,
	OFF_STATE = 0,		
	STATE_OFF = 0,	/*Settings - OFF*/
	PRESENT = 1,
	STATE_ON = 1,	/*Settings - ON*/
	INITIALISED = 2,
	ACTIVE_STATE = 3
} state_t;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

// Settings struct
typedef struct Settings_tag {
	// Input
	char source;
	char format;
	char encoding;
	char* input;
	FILE* inputFile;
	// Output
	char output;
	char outMode;
	char* outFile;
	FILE* outputFile;	
	// Bax settings
	unsigned char linkMode;
	unsigned char filter;
	char* baxInfoFile;
	char* baxInfoFileSetting ;
	char* baxConfigFile; /* Init script */
	// Reader specific functions
	PutByte_t outPutc;
	GetByte_t inGetc;
	int fd;
	// UDP specific options
	void* localServer;
	void* remoteAddress;
	SOCKET udpSocket; 
	unsigned short udpPort;
	void* udpState;
	char* ipAddress;
	char* username;
	char* password;
	char* destMac;
	// Output tracking
	unsigned long pktCount;
	unsigned long dataNum;
} Settings_t;

typedef struct {
	state_t app_state;
	state_t radio_state;
}Status_t;

// Globals
extern Settings_t gSettings;
extern Status_t gStatus;

// Prototypes for transport
int OpenTransport(Settings_t* settings);
int CloseTransport(Settings_t* settings);
int OpenOutput(Settings_t* settings);
int CloseOutput(Settings_t* settings);
void TransportTasks(Settings_t* settings);
void TransportCheckHardware(void);

// Exit error handler
void ErrorExit(const char* fmt,...);

#endif
