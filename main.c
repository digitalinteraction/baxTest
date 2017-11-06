/*
 * BAX Receiver
 * K Ladha 2014
 */

#ifdef _WIN32
	#define _CRT_SECURE_NO_WARNINGS
    #include <windows.h>
	#include <conio.h>		// getch
	#include <io.h>			// isatty (for checking if stdout is a pipe)
#else
	#include <ctype.h>
	#include <string.h>
	#include <curses.h>
	#define _getch getch

	int kbhit(void)
	{
		int ch = getch();

		if (ch != ERR) {
			ungetch(ch);
			return 1;
		} else {
			return 0;
		}
	}
	
	#define _kbhit kbhit
#endif

#include <stdio.h>
#include <stdlib.h>
//#include "UDP.h"
//#include "Serial.h"
//#include "Utils.h"
#include "BaxRx.h"
#include "Config.h"

// Debug setting
#undef DEBUG_LEVEL
#define DEBUG_LEVEL	0
#define DBG_FILE dbg_file
#if (DEBUG_LEVEL > 0)||(GLOBAL_DEBUG_LEVEL > 0)
static const char* dbg_file = "main.c";
#endif
#include "Debug.h"

// Globals
Settings_t gSettings;
Status_t gStatus;
// BAX reciever settings
const char* CommandLineOptions = 
"Input options:                                                    \r\n"
"    'S'ource:       Default: Serial port                          \r\n"
"                    SerialPort      'S                            \r\n"
"                    File            'F'                           \r\n"
"                    UDP             'U'                           \r\n\r\n"
"    'F'ormat        Default: Radio Events                         \r\n"
"                    Radio events    'E'                           \r\n"
"                    Binary units    'U'                           \r\n\r\n"
"    'E'ncoding      Default: Hex ascii                            \r\n"
"                    Raw binary      'R'                           \r\n"
"                    Hex ascii       'H'                           \r\n"
"                    Slip encoded    'S'                           \r\n\r\n"
"    'D'escriptor,   Default: COM1                                 \r\n"
"    (COM1 , DAT12345.BIN, 192.168.0.100+12-34-56-78-9A-BC+username+password)\r\n\r\n"
"Output options:                                                   \r\n"
"    'O'utput        Default: stdout                               \r\n"
"                    File            'F'                           \r\n"
"                    Stdout          'S'                           \r\n\r\n"
"    Output 'M'mode  Default: Hex ascii                            \r\n"
"                    Raw binary      'R'                           \r\n"
"                    Hex ascii       'H'                           \r\n"
"                    Slip encoded    'S'                           \r\n"
"                    CSV output      'C'                           \r\n\r\n"
"    Outpu'T' file   Default: output.out	                       \r\n"
"                    e.g. output.bin                               \r\n\r\n"
"Bax settings:                                                     \r\n"
"    'P'acket filtering    Default: PNDE (all)                     \r\n"
"                    Pairing packets 'P'                           \r\n"
"                    Name packets    'N'                           \r\n"
"                    Decrypted       'D'                           \r\n"
"                    Encrypted       'E'                           \r\n"
"                    Encrypted       'R'                           \r\n\r\n"
"    'R'adio decryption settings    Default: IPF                   \r\n"
"                    Load info file   'I'                          \r\n"
"                    Pair new devices 'P'                          \r\n"
"                    Add to info file 'F'                          \r\n\r\n"
"    'I'nfo file name Default: BAX_INFO.BIN                        \r\n"
"                    e.g. BAX_INFO.BIN                             \r\n\r\n"
"    'C'onfig file name Default: BAX_SETUP.CFG                     \r\n"
"                    e.g. BAX_SETUP.CFG                            \r\n";

// Prototypes
int main(int argc, char *argv[]);
void PrintCLOPtions(void);
void ErrorExit(const char* fmt,...);
void CleanupOnExit(void);
void RunApp(void);

/* Read loop */
int main(int argc, char *argv[])
{
	int parsedArgs = 0;
	// Init defauts
	memset(&gSettings,0,sizeof(Settings_t));
	// Input
	gSettings.source = 'S';
	gSettings.format = 'E';
	gSettings.encoding = 'H';
	gSettings.input = "COM1";
	gSettings.inputFile = NULL;
	// Output
	gSettings.output = 'S';
	gSettings.outMode = 'H';
	gSettings.outFile = "output.out";
	gSettings.outputFile = NULL;	
	// Bax settings
	gSettings.linkMode = 0xff;
	gSettings.filter = 0xff;
	gSettings.baxInfoFile = NULL;
	gSettings.baxInfoFileSetting = "BAX_INFO.BIN";
	gSettings.baxConfigFile = "BAX_SETUP.CFG";
	gSettings.localServer = NULL;
	gSettings.remoteAddress = NULL;
	gSettings.udpSocket = 0;
	gSettings.udpState = NULL;
	gSettings.ipAddress = "0.0.0.0";
	gSettings.udpPort = BAX_UDP_PORT_FORWARDING;
	gSettings.destMac = "00-00-00-00-00-00";
	gSettings.username = "admin";
	gSettings.password = "password";
	// Reader specific functions
	gSettings.outPutc = NULL;
	gSettings.inGetc = NULL;
	gSettings.fd = 0;
	// Output tracking
	gSettings.pktCount = 0;
	gSettings.dataNum = 0;

	// Read ARGS
	if(argc > 1)argc--; // Decrement so it can be used as the index
	else 
	{
		// No args
		PrintCLOPtions();
	}
	while(argc > 0)		// Don't try to parse argv[0]
	{
		char first = argv[argc][0];
		char second = argv[argc][1];
		if(first == '-') 
		{
			switch(second) {
				case ('S'):
				case ('s') : {
					switch (argv[argc][2]) {
						case 'S':
						case 's': 
						case 'F':
						case 'f': 
						case 'U':
						case 'u': 
						case 'T':
						case 't': 
							gSettings.source =  toupper(argv[argc][2]);
						default : break;
					}
					break;
				}
				case ('F'):
				case ('f') : {
					switch (argv[argc][2]) {
						case 'E':
						case 'e': 
						case 'U':
						case 'u': 
							gSettings.format =  toupper(argv[argc][2]);
						default : break;
					}
					break;
				}
				case ('E'):
				case ('e') : {
					switch (argv[argc][2]) {
						case 'R':
						case 'r': 
						case 'H':
						case 'h': 
						case 'S':
						case 's': 
							gSettings.encoding =  toupper(argv[argc][2]);
						default : break;
					}
					break;
				}
				case ('D'):
				case ('d') : {
					gSettings.input = &argv[argc][2];
					break;
				}
				case ('O'):
				case ('o') : {
					switch (argv[argc][2]) {
						case 'F':
						case 'f': 
						case 'S':
						case 's': 
							gSettings.output =  toupper(argv[argc][2]);
						default : break;
					}
					break;
				}
				case ('M'):
				case ('m'):{
					switch (argv[argc][2]) {
						case 'R':
						case 'r': 
						case 'H':
						case 'h': 
						case 'S':
						case 's': 
						case 'C':
						case 'c': 
							gSettings.outMode =  toupper(argv[argc][2]);
						default : break;
					}
					break;
				}
				case ('T'):
				case ('t') : {
					gSettings.outFile = &argv[argc][2];
					break;
				}
				case ('P'):
				case ('p'): {
					int offset = 2;
					gSettings.filter = 0;
					while(argv[argc][offset] != '\0'){
					switch (argv[argc][offset]) {
						case 'P':
						case 'p': {
							gSettings.filter |= FILTER_FLAG_PAIRING;
							break;
						}
						case 'N':
						case 'n': {
							gSettings.filter |= FILTER_FLAG_NAME;
							break;
						}
						case 'D':
						case 'd': {
							gSettings.filter |= FILTER_FLAG_DECODED;
							break;
						}
						case 'E':
						case 'e': {
							gSettings.filter |= FILTER_FLAG_ENCRYPTED;
							break;
						}
						case 'R':
						case 'r': {
							gSettings.filter |= FILTER_FLAG_RAW;
							break;
						}
						default : break;
					}
					offset++;
					}// while
					break;
				}
				case ('R'):
				case ('r') : {
					int offset = 2;
					gSettings.linkMode = 0;
					while(argv[argc][offset] != '\0'){
					switch (argv[argc][offset]) {
						case 'I':
						case 'i': {
							gSettings.linkMode |= LINK_FLAG_FILE;
							break;
						}
						case 'F':
						case 'f': {
							gSettings.linkMode |= LINK_FLAG_ADD;
							break;
						}
						case 'P':
						case 'p': {
							gSettings.linkMode |= LINK_FLAG_PAIR;
							break;
						}
						default : break;
					}
					offset++;
					}// while
					break;
				}
				case ('I'):
				case ('i') : {
					gSettings.baxInfoFileSetting = &argv[argc][2];
					break;
				}
				case ('C'):
				case ('c') : {
					gSettings.baxConfigFile = &argv[argc][2];
					break;
				}
				default: {
					parsedArgs--;
					fprintf(stderr,"\r\nUnknown command line option %s",argv[argc]);
					break;
				}
			}// Switch
			parsedArgs++;
		}// if '-'
		// Next arg
		argc--;
	}// While

	if(parsedArgs <= 0)
	{
		PrintCLOPtions();
		exit(0);
	}

	// Indicate debug on
	DBG_INFO("Debug output on!");

	// Set cleanup funtion
	atexit(CleanupOnExit);

	// Now try to open input
	if(!OpenTransport(&gSettings))
	{
		exit(-1);
	}

	// Open output
	if(!OpenOutput(&gSettings))
	{
		exit(-1);
	}

	// Run the application
	RunApp();

	// Exit
	exit(0);
}

void PrintCLOPtions(void)
{
	fprintf(stdout,"%s",CommandLineOptions);
	fprintf(stdout,"\r\nPress any key to exit....");
	getc(stdin);
	exit(1);
}

void RunApp(void)
{
	static unsigned long long lastTimeMs = 0;

	// Now open BAX receiver (reader)
	// Allow reader to try loading the info file
	if(gSettings.linkMode & LINK_FLAG_FILE)
		gSettings.baxInfoFile = gSettings.baxInfoFileSetting;

	if(gSettings.source == 'S' && gSettings.format == 'E')
	{
		// Init the receiver if in radio control mode
		BaxRxInit();
	}
	else
	{
		// Init the reader parts only
		BaxInitDeviceInfo();
		// Load info file (if ptr is set above)
		BaxLoadInfoFile(gSettings.baxInfoFile);
	}

	// Stop reader adding to the file if this is disabled
	if(!(gSettings.linkMode & LINK_FLAG_ADD))
		gSettings.baxInfoFile = NULL;

	while(gStatus.app_state != ERROR_STATE)
	{
		if(_kbhit() != 0 && _getch() == 27) break;	// Exit on ESC hit

		// Transport tasks (read input)
		TransportTasks(&gSettings);
	
		// Bax receiver tasks
		if(gSettings.source == 'S' && gSettings.format == 'E')
		{
			unsigned long long now = MillisecondsEpoch();
			if(lastTimeMs == 0) lastTimeMs = now;
			// If we are controlling an actual radio dongle
			else if((now - lastTimeMs) > 300000lu)
			{
				lastTimeMs = now;
				TransportCheckHardware();
			}
		}

		// Realtime user/caller stdin commands
		// TODO:
	}
	return;
}

void CleanupOnExit(void)
{
	// Cleanup code....

	// Close port
	CloseTransport(&gSettings);
	// Close log file
	CloseOutput(&gSettings);

	// Pause
#if defined(_WIN32) && defined(_DEBUG)
	if(_isatty(_fileno(stdout)))		// If stdout isn't a pipe...
	{
		fprintf(stdout,"\r\nPress Return to exit....");
		_getch();
	}
#endif
}

void ErrorExit(const char* fmt,...)
{
    va_list myargs;
    va_start(myargs, fmt);
	fprintf(stderr, "\r\nEXIT ON ERROR:");
	vfprintf(stderr, fmt, myargs); // Divert to stderr
	fprintf(stderr, "\r\n");
	va_end(myargs);
	exit(-1);
}


//EOF
