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


void getPacket(u_char * arg, const struct pcap_pkthdr * pkthdr, const u_char * packet)
{
#if 0 
  int * id = (int *)arg;
  printf("id: %d\n", ++(*id));
  printf("Packet length: %d\n", pkthdr->len);
  printf("Number of bytes: %d\n", pkthdr->caplen);
  printf("Recieved time: %s", ctime((const time_t *)&pkthdr->ts.tv_sec)); 
#endif  
	static unsigned int i;
	static unsigned int j;
	static unsigned int k;
	int nheadlen = packet[2];
	i++;
	if (packet[nheadlen + 0] == 0x08
			&& packet[nheadlen + 4] == 0x01 && packet[nheadlen + 5] == 0x00 && packet[nheadlen + 6] == 0x5e
			&& packet[nheadlen + 7] == 0x00 && packet[nheadlen + 8] == 0x00 && packet[nheadlen + 9] == 0xfb)
	{
		j++;
		printf("%4u %4u %4u     Data : %4u : %02x : %02x%02x%02x%02x%02x%02x : %02x%02x%02x%02x%02x%02x\n", i, j, k, pkthdr->len - nheadlen, packet[nheadlen + 1],
			packet[nheadlen + 16],packet[nheadlen + 17],packet[nheadlen + 18],packet[nheadlen + 19],packet[nheadlen + 20],packet[nheadlen + 21],
			packet[nheadlen + 4],packet[nheadlen + 5],packet[nheadlen + 6],packet[nheadlen + 7],packet[nheadlen + 8],packet[nheadlen + 9]);
	}
	#if 1
	else if (packet[nheadlen + 0] == 0x88
			&& packet[nheadlen + 16] == 0x01 && packet[nheadlen + 17] == 0x00 && packet[nheadlen + 18] == 0x5e
			&& packet[nheadlen + 19] == 0x00 && packet[nheadlen + 20] == 0x00 && packet[nheadlen + 21] == 0xfb)
	{
		k++;
		/*
		printf("%4u %4u %4u Qos Data : %4u : %02x : %02x%02x%02x%02x%02x%02x : %02x%02x%02x%02x%02x%02x\n", i, j, k, pkthdr->len - nheadlen, packet[nheadlen + 1],
			packet[nheadlen + 10],packet[nheadlen + 11],packet[nheadlen + 12],packet[nheadlen + 13],packet[nheadlen + 14],packet[nheadlen + 15],
			packet[nheadlen + 16],packet[nheadlen + 17],packet[nheadlen + 18],packet[nheadlen + 19],packet[nheadlen + 20],packet[nheadlen + 21]);
		*/
		//packet[31],packet[32],packet[33],packet[34],packet[35],packet[36],
		//packet[37],packet[38],packet[39],packet[40],packet[41],packet[42]);
	}
	#endif
}

int arg_option(int argc, char *argv[])
{
	#if 0
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
	#endif
	if (argc != 2)
	{
		printf("para error");
		exit(155);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	arg_option(argc, argv);

	char errBuf[PCAP_ERRBUF_SIZE];//, * devStr;

	/* open a device, wait until a packet arrives */
	pcap_t * device = pcap_open_live(argv[1], 65535, 0, 0, errBuf);
	if(!device)
	{
		printf("error: pcap_open_live(): %s\n", errBuf);
		exit(1);
	}

	/* construct a filter */ 
	struct bpf_program filter;
	int i = pcap_compile(device, &filter, "wlan dst 01:00:5e:00:00:fb", 1, 0);
	printf("pcap_compile return: %d\n", i);
	pcap_setfilter(device, &filter);

	/* wait loop forever */
	int id = 0;
	pcap_loop(device, -1, getPacket, (u_char*)&id);

	pcap_close(device);

	return 0;
}

