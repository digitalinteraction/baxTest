/* Cross-platform alternatives */
#ifdef _WIN32

	#define WIN_HANDLE

    /* Defines and headers */
    #define _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_DEPRECATE
    #include <windows.h>
    #include <io.h>
    
    /* Strings */
   #define strcasecmp _stricmp
//    #define snprintf _snprintf    

    /* Files */
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)

    ///* Sleep */
    //#define sleep(seconds) Sleep(seconds * 1000UL)
    //#define usleep(microseconds) Sleep(microseconds / 1000UL)

    /* Time */
    #define gmtime_r(timer, result) gmtime_s(result, timer)
    #define timegm _mkgmtime

    /* Socket */
    #include <winsock.h>
    #define _POSIX_
    typedef int socklen_t;
    #pragma warning( disable : 4996 )    /* allow deprecated POSIX name functions */
    #pragma comment(lib, "wsock32")
	#define socketErrno   (WSAGetLastError())
	#define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK

    /* Thread */
	#define thread_t HANDLE
    #define thread_create(thread, attr_ignored, start_routine, arg) ((*(thread) = CreateThread(attr_ignored, 0, start_routine, arg, 0, NULL)) == NULL)
    #define thread_join(thread, value_ptr_ignored) ((value_ptr_ignored), WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0)
    #define thread_cancel(thread) (TerminateThread(thread, -1) == 0)
    #define thread_return_t DWORD WINAPI
    #define thread_return_value(value) ((unsigned int)(value))

    /* Mutex */
	#define mutex_t HANDLE
    #define mutex_init(mutex, attr_ignored) ((*(mutex) = CreateMutex(attr_ignored, FALSE, NULL)) == NULL)
    #define mutex_lock(mutex) (WaitForSingleObject(*(mutex), INFINITE) != WAIT_OBJECT_0)
    #define mutex_unlock(mutex) (ReleaseMutex(*(mutex)) == 0)
    #define mutex_destroy(mutex) (CloseHandle(*(mutex)) == 0)

    /* Device discovery */
    #include <setupapi.h>
    #ifdef _MSC_VER
        #include <cfgmgr32.h>
    #endif
    #pragma comment(lib, "setupapi.lib")
    #pragma comment(lib, "advapi32.lib")    /* For RegQueryValueEx() */

#else

    /* Sockets */
	#include <sys/ioctl.h>
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netinet/tcp.h>
    #include <netdb.h>
    #include <termios.h>
    typedef int SOCKET;
    #define SOCKET_ERROR (-1)
    #define INVALID_SOCKET (-1) 
	#define closesocket close
    #define ioctlsocket ioctl
	#define socketErrno errno
	#define socketStrerr(_e) strerror(_e)
	#define SOCKET_EWOULDBLOCK EWOULDBLOCK

    /* Thread */
    #include <pthread.h>
    #define thread_t      pthread_t
    #define thread_create pthread_create
    #define thread_join   pthread_join
    #define thread_cancel pthread_cancel
    typedef void *        thread_return_t;
    #define thread_return_value(value_ignored) ((value_ignored), NULL)

    /* Mutex */
	#define mutex_t       pthread_mutex_t
    #define mutex_init    pthread_mutex_init
    #define mutex_lock    pthread_mutex_lock
    #define mutex_unlock  pthread_mutex_unlock
    #define mutex_destroy pthread_mutex_destroy

#endif


/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/stat.h>

#include "UDP.h"
#include "AsciiHex.h"
#include "SlipUtils.h"
#include "BaxUtils.h"
#include "Config.h"

// Debug setting
#undef DEBUG_LEVEL
#define DEBUG_LEVEL	0
#define DBG_FILE dbg_file
#if (DEBUG_LEVEL > 0)||(GLOBAL_DEBUG_LEVEL > 0)
static const char* dbg_file = "UDP";
#endif
#include "Debug.h"

unsigned char BaxUdpAutoDiscovery(Settings_t* settings, unsigned short timeout);
unsigned char BaxUdpConnect(Settings_t* settings);
unsigned char* UdpWaitOnPkt(SOCKET s, struct sockaddr_in *serverAddr, int* count, int timeoutms);

#define MAX_IP_P_ADD_LEN (16+6+1) //aaa.aaa.aaa.aaa:ppppp null
typedef struct {
	unsigned char petition[PETITION_LEN];
	char username[USERN_PASS_MAX_LEN];
	char password[USERN_PASS_MAX_LEN];
	unsigned char destMac[6];
	char netBiosName[17];
	char ipAddr[MAX_IP_P_ADD_LEN];
	char macAddrStr[19];
	uint32_t nextSession;
	uint32_t sessionTimout;
	uint32_t leaseLen;
	unsigned long long start;
} BaxDiscInfo_t;

size_t strcpytok(char* dest, const char* source, const char* tokens);
size_t strskiptok(const char* source, const char* tokens);
size_t strfindtok(const char* source, const char* tokens);

uint64_t ReadMacIEEE(unsigned char* dest, char* source)
{
	uint64_t ret = 0;
	int i;
	unsigned char dummy[6], *ptr;
	if(dest != NULL) ptr = dest;
	else ptr = dest = dummy;
	for(i=0;i<6;i++)
	{
		if(ReadHexToBinary(ptr, source, 1) != 1)
			return (uint64_t)0;
		ptr++;
		source+=3;
	}
	// Unpack to 64 bit int type
	ret = dest[0]; ret <<= 8;
	ret |= dest[1]; ret <<= 8;
	ret |= dest[2]; ret <<= 8;
	ret |= dest[3]; ret <<= 8;
	ret |= dest[4]; ret <<= 8;
	ret |= dest[5];
	return ret;
}

unsigned char BaxParseUdpDiscovery(BaxDiscInfo_t* info, char* pkt, int len)
{
	char* end = pkt+len;
	memset(info,0,sizeof(BaxDiscInfo_t));
	// Look for header first
	if(strncmp(pkt,"BAX ROUTER\r\n",12) != 0) return 0;
	pkt += 12;
	// Parse netbios name
	if(strncmp(pkt,"NETBIOS NAME: ",14) != 0) return 0;
	pkt += 14;
	pkt += strcpytok(info->netBiosName,pkt,"\r\n");
	pkt += strskiptok(pkt, "\r\n");
	if(*pkt == '\0' || pkt >= end) return 0;
	// Parse session
	if(strncmp(pkt,"NEXT SESSION: ",14) != 0) return 0;
	pkt += 14;
	info->nextSession = atoi(pkt);
	pkt += strfindtok(pkt, "M");
	if(*pkt == '\0' || pkt >= end) return 0;
	// Parse IP	
	if(strncmp(pkt,"MY IP: ",7) != 0) return 0;
	pkt += 7;
	pkt += strcpytok(info->ipAddr,pkt,"\r\n");
	pkt += strskiptok(pkt, "\r\n");
	if(*pkt == '\0' || pkt >= end) return 0;
	// Parse MAC
	if(strncmp(pkt,"MY MAC: ",8) != 0) return 0;
	pkt += 8;
	pkt += strcpytok(info->macAddrStr,pkt,"\r\n");
	return	1;
}
unsigned char BaxParseUdpResponse(BaxDiscInfo_t* info, char* pkt, int len)
{
	char* end = pkt+len;
	if(strncmp(pkt,"BAD CREDENTIALS: NEXT SESSION ",30) == 0)
	{
		// Return one, partial success
		pkt += 30;
		info->nextSession = atol(pkt);
		info->sessionTimout = 0;
		return 1;
	}
	else if(strncmp(pkt,"WELCOME: NEXT SESSION ",22) == 0)
	{
		// Return one, success
		pkt += 30;
		info->nextSession = atol(pkt); // Parse next session
		pkt += strfindtok(pkt, ":");
		pkt++;
		if(*pkt == '\0' || pkt >= end) return 0;
		info->sessionTimout = atol(pkt); // Parse timeout
		return 2;
	
	}
	return 0;
}


unsigned char BaxUdpOpen(Settings_t* settings)
{
	unsigned char ret = 0;

	// Make the server
	settings->localServer = makeServer();
	if(settings->localServer == NULL)
	{
		DBG_ERROR("Can not make local server");
		return 0;
	}

	// Make state
	settings->udpState = malloc(sizeof(BaxDiscInfo_t));
	if(settings->udpState == NULL)
	{
		DBG_ERROR("Can not make udp state");
		return 0;
	}
	memset(settings->udpState,0,sizeof(BaxDiscInfo_t));

	// Make settings correct
	settings->encoding = 'R'; // Raw binary units
	settings->format = 'U';

	// Check if ip is auto discovery one
	if(inet_addr(settings->ipAddress) == 0)
	{
		// Open the discovery socket
		settings->udpSocket = opensocket(NULL, BAX_UDP_PORT_DISCOVERY,  (struct sockaddr_in *)settings->localServer);
		if(settings->udpSocket == SOCKET_ERROR || settings->udpSocket == INVALID_SOCKET)
		{
			DBG_ERROR("Can not make udp socket");
			return 0;
		}
		BaxUdpAutoDiscovery(settings, 600);
	}

	// If may have changed if we were auot discovering
	if(inet_addr(settings->ipAddress) != 0)
	{
		// Initialise the TX side credentials
		struct sockaddr_in* remote;
		settings->remoteAddress = makeRemote();
		if(settings->remoteAddress == NULL)
			{DBG_ERROR("Can not make udp remote address");return 0;}
		remote = (struct sockaddr_in*)settings->remoteAddress;
		remote->sin_family = AF_INET;
		remote->sin_addr.s_addr = inet_addr(settings->ipAddress);
		remote->sin_port = htons(settings->udpPort);
		// Open the incoming udp socket
		settings->udpSocket = opensocket(NULL, BAX_UDP_PORT_FORWARDING,  (struct sockaddr_in *)settings->localServer);
		if(settings->udpSocket == SOCKET_ERROR || settings->udpSocket == INVALID_SOCKET)
		{
			DBG_ERROR("Can not make udp socket");
			return 0;
		}
		ret = BaxUdpConnect(settings);
	}
	return  ret;
}

void UdpCleanup (Settings_t* settings)
{
	// Close socket
	if(settings->udpSocket != SOCKET_ERROR && settings->udpSocket != INVALID_SOCKET)
	{
		closesocket(settings->udpSocket);
		settings->udpSocket = INVALID_SOCKET;
	}
	if(settings->udpState != NULL)
	{
		free(settings->udpState);
		settings->udpState = NULL;
	}
	// Free remote info
	if(settings->remoteAddress != NULL)
	{
		free(settings->remoteAddress);
		settings->remoteAddress = NULL;
	}
	// Free server - assumes if a server exists then WSA code was called too
	if(settings->localServer != NULL) 
	{
		free(settings->localServer);
		settings->localServer = NULL;

		#ifdef _WIN32
			WSACleanup();
		#endif
	}
}

unsigned char BaxUdpAutoDiscovery(Settings_t* settings, unsigned short timeout)
{
	int found = 0, maxRetrys = 5;
	int length;
	unsigned char *data;
	struct sockaddr_in from = {0};
	unsigned long long start = MillisecondsEpoch();
	while(found == 0 && maxRetrys > 0)
	{
		DBG_INFO("\r\nListening for local devices");
		data = UdpWaitOnPkt(settings->udpSocket, &from, &length, timeout*1000);
		if(data != NULL && length > 0)
		{
			// Parse input into settings
			BaxDiscInfo_t* info = (BaxDiscInfo_t*)settings->udpState;
			if(BaxParseUdpDiscovery((BaxDiscInfo_t*)settings->udpState, (char*)data, length))
			{
				// Compare MAC if specified
				uint64_t setmac = ReadMacIEEE(NULL, settings->destMac);
				if(setmac == 0 || setmac == ReadMacIEEE(NULL, info->macAddrStr))
				{	
					DBG_INFO("\r\nMatching device %s found",info->netBiosName);			
					// Change settings to match
					settings->ipAddress = info->ipAddr;
					settings->destMac = info->macAddrStr;
					//settings->udpPort = from.sin_port; // Source port of the received pkt
					settings->udpPort = BAX_UDP_PORT_FORWARDING;
					// Replace the ip address with the one from the server
					sprintf(settings->ipAddress,"%s",inet_ntoa(from.sin_addr));
					// Connect to the router
					found = 1;
					break;
				}
				else 
				{
					DBG_INFO("\r\nUnmatched device ignored");
				}
			}
			else
			{
				DBG_INFO("\r\nUnrecognised packet received");
				maxRetrys --;
			}
		}
		// Timeout
		if((MillisecondsEpoch() - start) > (timeout * 1000))
		{
			break;
		}
	}

	if(settings->udpSocket != SOCKET_ERROR && settings->udpSocket != INVALID_SOCKET)
	{
		closesocket(settings->udpSocket);
		settings->udpSocket = INVALID_SOCKET;
	}
	return found;
}

unsigned char BaxUdpConnect(Settings_t* settings)
{
	unsigned char ret = 0;
	BaxDiscInfo_t* info = (BaxDiscInfo_t*)settings->udpState;
	//struct sockaddr_in from = {0};
	int sent, length, retrys;
	unsigned char* data;
	// Set deired lease time
	info->leaseLen = 3600;
	// Copy configuration to zero padded buffers
	if(ReadMacIEEE(info->destMac,settings->destMac) == 0)
	{
		DBG_ERROR("Can't read udp mac");
		return 0;
	}
	strncpy(info->username, settings->username, USERN_PASS_MAX_LEN);
	strncpy(info->password, settings->password, USERN_PASS_MAX_LEN);
	// Send the petition and wait on response with retrys
	for(retrys = 5; retrys > 0; retrys--)
	{
		DBG_INFO("\r\nSending petition to: %s, session: %u",settings->ipAddress, info->nextSession);
		// Contruct a petition
		UdpMakePetition(info->petition, info->leaseLen, info->destMac, info->username, info->password, info->nextSession);
		// Send petition
		sent = transmit(settings->udpSocket, (struct sockaddr_in *)settings->remoteAddress, info->petition, PETITION_LEN);
		if(sent != PETITION_LEN){	DBG_INFO("\r\nSending failed");	}
		// Wait on response for 1000 ms
		data = UdpWaitOnPkt(settings->udpSocket, (struct sockaddr_in *)settings->localServer, &length, 1000);
		if(data != NULL && length > 0)
		{
			unsigned char result;
			// Parse response
			DBG_INFO("\r\nResponse received");
			result = BaxParseUdpResponse(info, (char*)data, length);
			if(result == 0)
			{
				DBG_INFO("\r\nBad response");
			}
			else if(result == 1)
			{
				DBG_INFO("\r\nFound next session, retrying");
			}
			else if (result == 2)
			{
				DBG_INFO("\r\nUdp negotiation success");
				info->start = MillisecondsEpoch();
				settings->inGetc = getcUdp;
				settings->outPutc = putcUdp;
				ret = 1;
				break;
			}
		}
		else
		{
			// Timeout
			DBG_INFO("\r\nNo response");
		}
	}
		
	return ret;
}

// Shim for api cross compatibility to typedef int (*GetByte_t)(Settings_t* settings);
int getcUdp(Settings_t* settings)
{
	int ret = -1;
	BaxDiscInfo_t* info = (BaxDiscInfo_t*)settings->udpState;
	unsigned long long milliseconds, now = MillisecondsEpoch();
	int length;
	unsigned char *newUdpPkt;
	struct sockaddr_in from = {0};

	// State
	static unsigned char *pkt = NULL, *end;

	// Check for in packets with 1 ms timout
	if(pkt == NULL)
	{
		newUdpPkt = UdpWaitOnPkt(settings->udpSocket, &from, &length, 1);
		if((newUdpPkt != NULL) && (length == BINARY_DATA_UNIT_SIZE))
		{
			DBG_INFO("\r\nNew udp data in");
			// Set new reading state
			pkt = newUdpPkt;
			end = newUdpPkt+length;
		}
	}
	
	// If reading 
	if(pkt != NULL)
	{
		ret = *pkt++;	
		if(pkt >= end)
		{
			DBG_INFO("\r\nUdp element read");
			pkt = NULL;
		}
	}
	
	// Check timouts
	milliseconds = now - info->start;
	if((milliseconds/1000) > (info->sessionTimout / 2))
	{
		// If more than half the session is used, reconnect
		DBG_INFO("\r\nUdp renegotiating session");
		if(!BaxUdpConnect(settings))
		{
			ErrorExit("\r\nUdp reconnection timeout failed");
		}
	}

	return ret;
}
// Shim for api cross compatibility to typedef int (*PutByte_t)(Settings_t* settings, unsigned char b);
int putcUdp(Settings_t* settings, unsigned char b)
{
	int ret;
	ret = -1;
	DBG_ERROR("UDP putc attempted!");
	return ret;
}


unsigned char* UdpWaitOnPkt(SOCKET s, struct sockaddr_in *serverAddr, int* count, int timeoutms)
{
	unsigned int size_of_server = sizeof(struct sockaddr_in);
	static unsigned char rxBuffer[256];
	int length;
	//unsigned long long start = MillisecondsEpoch();
	if(count != NULL) *count = 0;
	for(;;)
	{
		length = recvfrom(s, (char*)rxBuffer, 256, 0, (struct sockaddr *)serverAddr, &size_of_server);
		if(length > 0)
		{
			// Debug print
			DBG_INFO("\r\nUDP RX:FROM:%s:%u\r\n",inet_ntoa(serverAddr->sin_addr),serverAddr->sin_port);
			DBG_WRITE(rxBuffer,length);
			if(count != NULL) *count = length;
			return rxBuffer;
		}
		else if ((length < 0)&&(socketErrno != SOCKET_EWOULDBLOCK))
		{
			ErrorExit("Socket failed error");
		}
		// Wait for 1ms
		usleep(1000);
		// Timeout
		if(timeoutms-- <= 0)
		{
			return NULL;
			break;
		}
	}
	return NULL;
}

void* makeServer(void)
{
	void* server;
	#ifdef _WIN32
    {
		static int once = 0;
		if(!once)
		{
			WSADATA wsaData;
			WSAStartup(MAKEWORD(1, 1), &wsaData);
		}
    }
	#endif
	server = malloc(sizeof(struct sockaddr_in));
	memset(server, 0, sizeof(struct sockaddr_in));
	return server;
}

void* makeRemote(void)
{
	void* remote;
	remote = malloc(sizeof(struct sockaddr));
	memset(remote, 0, sizeof(struct sockaddr));
	return remote;
}

/* Returns a socket error string */
#ifdef _WIN32
const char *socketStrerr(int e)
{
    static char errorString[256];
    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, e, 0, errorString, 255, NULL))
    {
        return errorString;
    }
    sprintf(errorString, "<unknown #%d 0x%x>", e, e);
    return errorString;
}
#endif


/* Open a UDP socket*/
SOCKET opensocket(const char *remote, int defaultPort, struct sockaddr_in *serverAddr)
{
    SOCKET s = SOCKET_ERROR;
    char serverName[128] = "localhost"; /* "localhost"; "127.0.0.1"; */
    char *portIndex;
    int doBind = 1, serverPort = defaultPort;
    struct hostent *hp;

    /* assigned parameters */
    if (remote != NULL && strlen(remote) > 0)
    {
        strcpy(serverName, remote);
        if ((portIndex = strstr(serverName, ":")) != NULL)
        {
            *portIndex++ = '\0';
            serverPort = atoi(portIndex);
        }
		// We are not connecting locally, i.e. as a client
		doBind = 0;
    }

    /* get server host information, name and address */
    hp = gethostbyname(serverName);
    if (hp == NULL)
    {
        serverAddr->sin_addr.s_addr = inet_addr(serverName);
        hp = gethostbyaddr((char *)&serverAddr->sin_addr.s_addr, sizeof(serverAddr->sin_addr.s_addr), AF_INET);
    }
    if (hp == NULL)
    {
        DBG_ERROR("Problem getting host socket information (%s)\n", socketStrerr(socketErrno));
        return SOCKET_ERROR;
    }
    memcpy(&(serverAddr->sin_addr), hp->h_addr, hp->h_length);
    serverAddr->sin_family = AF_INET; 
    serverAddr->sin_port = htons(serverPort);
    DBG_INFO("\r\nServer address: [%s] = %s : %d\n", hp->h_name, inet_ntoa(serverAddr->sin_addr), serverPort);
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); 
    if (s < 0) 
    {
        DBG_ERROR("Socket creation failed (%s)\n", socketStrerr(socketErrno));
        return SOCKET_ERROR;
    }

	/* Allow rapid reuse of this socket */
	//{
	//	int option = 1;
	//	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option));
	//}
    
	/* Bind the socket */
	if((doBind) && (bind(s, (struct sockaddr *) serverAddr, sizeof(struct sockaddr)) == SOCKET_ERROR))
	{
		DBG_ERROR("Socket bind failed (%s)\n", socketStrerr(socketErrno));
		return SOCKET_ERROR;
	}
	else
	{
		DBG_INFO("Socket un-bound for client use.");
	}	

	// Set non-blocking
	{
		int value = 1;
		ioctlsocket(s, FIONBIO, &value);
	}
	
    return s;
}


/* UDP-transmit a packet */
int transmit(SOCKET s, struct sockaddr_in *serverAddr, const void *sendBuffer, size_t sendLength)
{
    int sent = 0;
    if (sendLength > 0)
    {
        if (sendto(s, (const char *)sendBuffer, sendLength, 0, (struct sockaddr *)serverAddr, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
        {
            DBG_ERROR("Send failed (%s)\n", socketStrerr(socketErrno));
            return 0;
        }
        sent += (size_t)sendLength;
    }
    return sent;
}


/* UDP-receive a packet */
int receive(SOCKET s, struct sockaddr_in *serverAddr, void *receiveBuffer, size_t receiveLength)
{
	int len;
	unsigned int lenServ = sizeof(struct sockaddr_in);

	len = recvfrom(s, (char *)receiveBuffer, (int)receiveLength, 0, (struct sockaddr*)serverAddr, &lenServ);

	if (len == 0 || len == SOCKET_ERROR)
	{ 
		if(socketErrno != SOCKET_EWOULDBLOCK)
		{
			DBG_ERROR("Receive failed (%s)\n", socketStrerr(socketErrno)); 
			return -1;
		}
		return 0; 
	}

	DBG_INFO("UDPRECV: Received from %s:%d\n", inet_ntoa(serverAddr->sin_addr), ntohs(serverAddr->sin_port));

	return len;
}			

size_t strcpytok(char* dest, const char* source, const char* tokens)
{
	size_t ret = 0;
	char *toks;
	*dest = '\0';
	for(;;)
	{
		if(*source == '\0')
			return ret;
		for(toks = (char*)tokens; *toks != '\0'; toks++)
		{
			if(*source == *toks)
			{
				return ret;
			}
		}
		*dest++ = *source++;
		*dest = '\0';
		ret++;
	}
	return ret;
}
size_t strskiptok(const char* source, const char* tokens)
{
	size_t ret = 0;
	char *toks;
	for(;;)
	{
		if(*source == '\0')
			return ret;
		for(toks = (char*)tokens; *toks != '\0'; toks++)
		{
			if(*source == *toks)
			{
				break;
			}
		}
		if(*toks == '\0') break;
		ret++;
		source++;
	}
	return ret;
}
size_t strfindtok(const char* source, const char* tokens)
{
	size_t ret = 0;
	char *toks;
	for(;;)
	{
		if(*source == '\0')
			return ret;
		for(toks = (char*)tokens; *toks != '\0'; toks++)
		{
			if(*source == *toks)
			{
				return ret;
			}
		}
		ret++;
		source++;
	}
	return ret;
}
