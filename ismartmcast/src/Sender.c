#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <sys/unistd.h>

#define HELLO_PORT 15000
#define HELLO_GROUP "224.0.0.251"
in_addr_t szIPNet = {0};

char * safe_strdup(const char *s) {
	char * retval = NULL;
	if (!s) {
		exit(1);
	}
	retval = strdup(s);
	if (!retval) {
		exit(1);
	}
	return (retval);
}

char *
get_iface_ip(const char *ifname)
{
#if defined(__linux__)
	struct ifreq if_data;
	struct in_addr in;
	char *ip_str;
	int sockd;
	u_int32_t ip;

	/* Create a socket */
	if ((sockd = socket (AF_INET, SOCK_PACKET, htons(8086))) < 0) {
		return NULL;
	}

	/* Get IP of internal interface */
	strcpy (if_data.ifr_name, ifname);

	/* Get the IP address */
	if (ioctl (sockd, SIOCGIFADDR, &if_data) < 0) {
		return NULL;
	}
	memcpy ((void *) &ip, (void *) &if_data.ifr_addr.sa_data + 2, 4);
	in.s_addr = ip;

	ip_str = inet_ntoa(in);
	close(sockd);
	return safe_strdup(ip_str);
#elif defined(__NetBSD__)
	struct ifaddrs *ifa, *ifap;
	char *str = NULL;

	if (getifaddrs(&ifap) == -1) {
		return NULL;
	}
	/* XXX arbitrarily pick the first IPv4 address */
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, ifname) == 0 &&
				ifa->ifa_addr->sa_family == AF_INET)
			break;
	}
	if (ifa == NULL) {
		goto out;
	}
	str = safe_strdup(inet_ntoa(
				((struct sockaddr_in *)ifa->ifa_addr)->sin_addr));
out:
	freeifaddrs(ifap);
	return str;
#else
	return safe_strdup("0.0.0.0");
#endif
}


int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	int fd, cnt;
	struct ip_mreq mreq;
	char *message = "Hello world !!!";

	/* create what looks like an ordinary UDP socket */
	if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) 
	{
		perror("socket");
		exit(1);
	}

	char *brlan = NULL;
	if (argc == 2)
	{
		brlan = get_iface_ip("wlan0");
	}
	else
	{
		brlan = NULL;
	}
    if (brlan)
    {
		printf("bind ip:%s\n", brlan);
        inet_pton(AF_INET, brlan, &szIPNet);
		memset(&addr,0,sizeof(addr));
		addr.sin_family=AF_INET;
		//addr.sin_addr.s_addr=htonl(INADDR_ANY); /* N.B.: differs from sender */
		addr.sin_addr.s_addr=szIPNet;
		addr.sin_port=htons(HELLO_PORT);

		/* bind to receive address */
		if (bind(fd,(struct sockaddr *) &addr,sizeof(addr)) < 0)
		{
			perror("bind");
			exit(1);
		}

    }
	else
	{
		printf("no bind\n");
	}


	/* set up destination address */
	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=inet_addr(HELLO_GROUP);
	addr.sin_port=htons(HELLO_PORT);

	/* now just sendto() our destination! */
	int nMLen = 0;
	while (1)
	{
		if (sendto(fd,message, nMLen, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) 
		{
			perror("sendto");
			exit(1);
		}
		printf("%d byte sended\n", nMLen);
		nMLen++;
		if (nMLen > 1000)
		{
			nMLen = 0;
		}
		usleep(10000);
	}
}

