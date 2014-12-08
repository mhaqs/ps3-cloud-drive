/*
*	PSL1GHT Logger
*				NZHawk
*	A simple lib for you devs out there to include and setup
*	network debugging quickly.
*					  http://nzhawk.mezoka.com/pslightlogger
*/
//EDIT THESE VALUES TO SUIT YOUR NETWORK/////////
#define PCIP		"10.0.0.2"			/////
#define PCPORT		18194					/////
/////////////////////////////////////////////////
/////////////////////////////////////////////////

#include <net/net.h>

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Configure these (especially the IP) to your own setup.
// Use netcat to receive the results on your PC:
// UDP: nc -u -l -p 4000
// For some versions of netcat the -p option may need to be removed.

#define INITSTRING	"Logging Started\n"
#define BYESTRING	"Logging Stopped\n"

static int s;
struct sockaddr_in server;
void debugInit()
{
	s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	memset(&server, 0, sizeof(server));
	server.sin_len = sizeof(server);
	server.sin_family = AF_INET;
	inet_pton(AF_INET, PCIP, &server.sin_addr);
	server.sin_port = htons(PCPORT);

	sendto(s, INITSTRING, strlen(INITSTRING), 0, (struct sockaddr*)&server, sizeof(server));
}

void debugClose()
{
	sendto(s, BYESTRING, strlen(BYESTRING), 0, (struct sockaddr*)&server, sizeof(server));
	close(s);
}

void debugPrintf(const char *format, ...)
{
	if(s == -1)
		return;
	char logBuffer[1024];
	int max = sizeof(logBuffer);
	va_list va;
	va_start(va, format);
	
	int wrote = vsnprintf(logBuffer, max, format, va);
	if(wrote > max)
		wrote = max;
	va_end(va);
	sendto(s, logBuffer, wrote, 0, (struct sockaddr *)&server, sizeof(server));
}