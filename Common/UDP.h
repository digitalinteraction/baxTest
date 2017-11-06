/*
	UDP socket usage
*/
#ifndef _UDP_H_
#define _UDP_H_

#include "Config.h"

/* BAX specific open*/
unsigned char BaxUdpOpen(Settings_t* settings);
void UdpCleanup (Settings_t* settings);

// Shim for api cross compatibility to typedef int (*GetByte_t)(Settings_t* settings);
int putcUdp(Settings_t* settings, unsigned char b);
int getcUdp(Settings_t* settings);

/* Hide server struct */
void* makeServer(void);
void* makeRemote(void);

/* Predeclare sockaddr_in*/
struct sockaddr_in;

/* Open a UDP socket*/
SOCKET opensocket(const char *host, int defaultPort, struct sockaddr_in *serverAddr);

/* UDP-transmit a packet */
int transmit(SOCKET s, struct sockaddr_in *serverAddr, const void *sendBuffer, size_t sendLength);

/* UDP-receive a packet */
int receive(SOCKET s, struct sockaddr_in *serverAddr, void *receiveBuffer, size_t receiveLength);



#endif
