#include <pcap.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <net/if.h>

typedef struct OnlineStatus
{
	int nOnline;
	unsigned int nSrc;
	unsigned int nDes;
}OnlineStatus;

unsigned int szIPSrc[256] = {0};
unsigned int szIPDes[256] = {0};
unsigned char szIPNet[4] = {192,168,88,1}; //Íø¹ØIP
unsigned int szIPSrcSnap[256] = {0};
unsigned int szIPDesSnap[256] = {0};
OnlineStatus szIPOnline[256] = {{0}};

static pthread_t g_ptAna = 0;

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
	if ((sockd = socket (AF_INET, SOCK_PACKET, htons(0x8086))) < 0) {
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

static void* pthreadAnaOnline(void* arg)
{
	FILE *fd = NULL;
	FILE *outlinefd = NULL;
	char outlineIP[32] = {0};
	int noutlineIP[4] = {0};
	int i;
	
	pthread_detach(pthread_self());
	while(1)
	{
		memcpy(szIPSrcSnap, szIPSrc, 256 * sizeof(int));
		memcpy(szIPDesSnap, szIPDes, 256 * sizeof(int));
		sleep(5);

		for (i = 2; i <= 254; i++)
		{
			if (szIPSrcSnap[i] != szIPSrc[i] || szIPDesSnap[i] != szIPDes[i])
			{
				szIPOnline[i].nOnline = 1;
			}
			else if (szIPOnline[i].nOnline != 0)
			{
				szIPOnline[i].nOnline += 1;
				if (20 == szIPOnline[i].nOnline)
				{
					szIPOnline[i].nOnline = 0;
				}
			}
		}
		for (i = 2; i <= 254; i++)
		{
			if (szIPOnline[i].nOnline == 1)
			{
				szIPOnline[i].nSrc = (szIPSrc[i] - szIPSrcSnap[i]) / 5;
				szIPOnline[i].nDes = (szIPDes[i] - szIPDesSnap[i]) / 5;
			}
			else
			{
				szIPOnline[i].nSrc = 0;
				szIPOnline[i].nDes = 0;
			}
		}
		if ((outlinefd = fopen("/tmp/outline", "rt+")) != NULL)
	    {
			memset(outlineIP, 0, sizeof(outlineIP));
			fscanf(outlinefd, "%d.%d.%d.%d", &noutlineIP[0], &noutlineIP[1], &noutlineIP[2], &noutlineIP[3]);
			//printf("%d.%d.%d.%d\n", noutlineIP[0], noutlineIP[1], noutlineIP[2], noutlineIP[3]);
			szIPOnline[noutlineIP[3]].nOnline = 0;
			fclose(outlinefd);
			unlink("/tmp/outline");
		}
		if ((fd = fopen("/tmp/online", "w+")) == NULL)
	    {
   		    printf("Cannot open input file.\n");	
			continue;
		}
		for (i = 2; i < 254; i++)
		{
			if (szIPOnline[i].nOnline)
			{
				//printf("%d:%d-%d\n", i, szIPSrc[i], szIPSrcSnap[i]);
				fprintf(fd, "%d.%d.%d.%d %u %u\n", szIPNet[0], szIPNet[1], szIPNet[2], i, szIPOnline[i].nSrc, szIPOnline[i].nDes);
				//printf("%d:des:%u\n", i, szIPOnline[i].nDes);
			}
		}
		//fflush(fd);
		fclose(fd);
	}
	
	return NULL;
}

void getPacket(u_char * arg, const struct pcap_pkthdr * pkthdr, const u_char * packet)
{
#if 0  
  int * id = (int *)arg;
  printf("id: %d\n", ++(*id));
  printf("Packet length: %d\n", pkthdr->len);
  printf("Number of bytes: %d\n", pkthdr->caplen);
  printf("Recieved time: %s", ctime((const time_t *)&pkthdr->ts.tv_sec)); 
  printf("IPNet:%d,%d,%d\n", szIPNet[0], szIPNet[1], szIPNet[2]);
  printf("proto:%02x%02x,src:%d.%d.%d.%d\n", packet[12], packet[13], packet[26], packet[27], packet[28], packet[29]);
#endif  
  if (packet[12] == 0x08 && packet[13] == 0x00
	&& packet[26] == szIPNet[0] && packet[27] == szIPNet[1] && packet[28] == szIPNet[2] && packet[29] != szIPNet[3])
  {
	szIPSrc[packet[29]] += pkthdr->caplen;
  }
  else if (packet[12] == 0x08 && packet[13] == 0x00
	&& packet[30] == szIPNet[0] && packet[31] == szIPNet[1] && packet[32] == szIPNet[2] && packet[33] != szIPNet[3])
  {
	szIPDes[packet[33]] += pkthdr->caplen;
  }
}

int arg_option(int argc, char *argv[])
{
	int opt;
	while((opt=getopt(argc,argv, "d")) != -1)
	{
		switch(opt)
		{
			case 'd':
			{
				daemon(0, 0);
				break;
			}
			default:
			{
				break;
			}
			
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	arg_option(argc, argv);

	char errBuf[PCAP_ERRBUF_SIZE];//, * devStr;
	char *brlan = NULL;
	brlan = get_iface_ip("br-lan");
	if (brlan)
	{
		inet_pton(AF_INET, brlan, szIPNet);
	}
	printf("br-lan:%s\n", brlan);

  /* open a device, wait until a packet arrives */
  pcap_t * device = pcap_open_live("br-lan", 65535, 1, 0, errBuf);
  
  if(!device)
  {
    printf("error: pcap_open_live(): %s\n", errBuf);
    exit(1);
  }

	if (0 != pthread_create(&g_ptAna, NULL, pthreadAnaOnline, NULL))
	{
		exit(1);
	}
  
  /* wait loop forever */
  int id = 0;
  pcap_loop(device, -1, getPacket, (u_char*)&id);
  
  pcap_close(device);

  return 0;
}
