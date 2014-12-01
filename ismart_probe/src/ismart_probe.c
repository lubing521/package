#include <pcap.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <errno.h>
#include "List.h"

#define dp(...) do { if (dbg) { fprintf(stderr, __VA_ARGS__); } } while(0)
#define ep(...) do { fprintf(stderr, __VA_ARGS__); } while(0)

int dbg = 0; //打印标志
int ctl_socket_server = 0;
char dev[32] = {0};
int bg_chans  [] = 
{
	1, 7, 2, 8, 3, 9, 4, 10, 5, 11, 6
};

//数据来源: http://standards.ieee.org/develop/regauth/oui/oui.txt 
unsigned int *router_list = NULL;
unsigned int router_list_size = 0;

typedef struct OnlineStatus
{
	int nOnline;
	unsigned int nSrc;
	unsigned int nDes;
}OnlineStatus;

List *MacList = NULL;

void *thread_channel(void *arg)
{
	pthread_detach(pthread_self());
	char cmd[64] = {0};
	int i = 0;
//	struct timeval tvs2;
//	struct timeval tve2;
	dp("channel hop \n");
	while(1)
	{
		sprintf(cmd, "iw phy0 set channel %d", bg_chans[i]);
//		gettimeofday(&tvs2, NULL);
		//system()执行实际需要24ms
		system(cmd);
//		gettimeofday(&tve2, NULL);
//		dp("%s: time %ld\n", cmd, (tve2.tv_sec - tvs2.tv_sec)*1000000 + tve2.tv_usec - tvs2.tv_usec);
		usleep(250 * 1000);
		i++;
		if(10 < i)
		{
			i = 0;
		}
	}
	return NULL;
}

void *thread_post(void *arg)
{
	pthread_detach(pthread_self());
	while(1)
	{
		sleep(2*60);
		//w+ 打开可读写文件，若文件存在则文件长度清为零，即该文件内容会消失。若文件不存在则建立该文件
		FILE *fp = fopen("/tmp/maclist.txt", "w+");
		if (NULL == fp)
		{
			break;
		}
		char buf[128] = {0};

		struct ifreq ifreq;
		int sock;
		if((sock=socket(AF_INET,SOCK_STREAM,0))<0)
        {
                perror("socket");
                break;
        }
		strcpy(ifreq.ifr_name, dev);
		if(ioctl(sock,SIOCGIFHWADDR,&ifreq)<0)
        {
                perror("ioctl");
                break;
        }
		sprintf(buf, "mac=%02x:%02x:%02x:%02x:%02x:%02x",
						(unsigned char)ifreq.ifr_hwaddr.sa_data[0],
                        (unsigned char)ifreq.ifr_hwaddr.sa_data[1],
                        (unsigned char)ifreq.ifr_hwaddr.sa_data[2],
                        (unsigned char)ifreq.ifr_hwaddr.sa_data[3],
                        (unsigned char)ifreq.ifr_hwaddr.sa_data[4],
                        (unsigned char)ifreq.ifr_hwaddr.sa_data[5]);
		fwrite(buf, 1, strlen(buf), fp);

		PNode pnode = GetListFront(MacList);
		int i = 0;
		for (; NULL != pnode; pnode = pnode->next)
		{
			i++;
			memset(buf, 0, sizeof(buf));
//			if (1 == i)
//			{
//				sprintf(buf, "%d={\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"firsttime\":\"0\",\"leavetime\":\"0\"}", i,
//					((unsigned char *)pnode->data)[0], ((unsigned char *)pnode->data)[1],
//					((unsigned char *)pnode->data)[2], ((unsigned char *)pnode->data)[3],
//					((unsigned char *)pnode->data)[4], ((unsigned char *)pnode->data)[5]);
//			}
//			else
//			{
				//多了&
				sprintf(buf, "&%d={\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"firsttime\":\"0\",\"leavetime\":\"0\"}", i,
					((unsigned char *)pnode->data)[0], ((unsigned char *)pnode->data)[1],
					((unsigned char *)pnode->data)[2], ((unsigned char *)pnode->data)[3],
					((unsigned char *)pnode->data)[4], ((unsigned char *)pnode->data)[5]);
//			}
			fwrite(buf, 1, strlen(buf), fp);
		}
		fclose(fp);
		system("maclist=$(cat /tmp/maclist.txt) && wget -O /dev/null --post-data=$maclist http://boxdata.aijee.cn/wifidetect.php");
		ClearList(MacList);
	}
	return NULL;
}

int cmp_router_list(void *mac)
{
	unsigned int i = 0;
	for(i = 0; i < router_list_size; i++)
	{
		if (((unsigned char *)mac)[0] == ((router_list[i] >> 16) & 0xff) &&
			((unsigned char *)mac)[1] == ((router_list[i] >> 8) & 0xff) &&
			((unsigned char *)mac)[2] == ((router_list[i] >> 0) & 0xff))
		{
			return 1;
		}
	}
	return 0;
}

void PrintMacList()
{
	PNode pnode = GetListFront(MacList);
	int i = 0;
	for (; NULL != pnode; pnode = pnode->next)
	{
		ep("%4d %02x:%02x:%02x:%02x:%02x:%02x\n", i++, 
				((unsigned char *)pnode->data)[0], ((unsigned char *)pnode->data)[1],
				((unsigned char *)pnode->data)[2], ((unsigned char *)pnode->data)[3],
				((unsigned char *)pnode->data)[4], ((unsigned char *)pnode->data)[5]);
	}
}

void AddMacToList(void *mac)
{
	PNode pnode = GetListFront(MacList);
	if (((unsigned char *)mac)[0] & 0x01)
	{
		//如果第一个 bit (LSB)为 1,该地址代表一组实际工作站,称为组播(多点传播[multicast])地址。
		//如果所有 bit (LSB)均为 1,该帧即属广播(broadcast),因此会传送给连接至无线介质的所有工作站。
		//01-80-C2-00-00-00,LSB顺序(比特序)就是：1000 0000 0000 0001 0100 0011 0000 0000 0000 0000 0000 0000
		//比特序是”Little Endian”（也就是最低位先传送）,以太网线路上按“Big Endian”字节序传送报文
		return ;
	}
	if (0 != cmp_router_list(mac))
	{
		//过滤路由器的mac
		return;
	}
	for (; NULL != pnode; pnode = pnode->next)
	{
		if(0 == memcmp(pnode->data, mac, 6))
		{
			return;
		}
	}
	EnListTail(MacList, mac, 6);
	dp("%4d %02x:%02x:%02x:%02x:%02x:%02x\n", MacList->size, ((unsigned char *)mac)[0], ((unsigned char *)mac)[1],
				((unsigned char *)mac)[2], ((unsigned char *)mac)[3],
				((unsigned char *)mac)[4], ((unsigned char *)mac)[5]);
	return ;
}

void *thread_ctl(void*arg)
{
	pthread_detach(pthread_self());
	int fd = 0;
	struct  sockaddr_un sa_un = {0};
	socklen_t len = sizeof(sa_un);
	char request[128] = {0};
	char *sock_name = "/tmp/probectl.sock";

	ctl_socket_server = socket(PF_UNIX, SOCK_STREAM, 0);	
	unlink(sock_name);
	sa_un.sun_family = AF_UNIX;
	strncpy(sa_un.sun_path, sock_name, sizeof(sa_un.sun_path));
	if (bind(ctl_socket_server, (struct sockaddr *)&sa_un, strlen(sa_un.sun_path) + sizeof(sa_un.sun_family)))
	{
		ep("%s", strerror(errno));
		pthread_exit(NULL);
	}
	if (listen(ctl_socket_server, 5))
	{
		ep("%s", strerror(errno));
		pthread_exit(NULL);
	}
	while(1)
	{
		if ((fd = accept(ctl_socket_server, (struct sockaddr *)&sa_un, &len)) == -1)
		{
			ep("%s", strerror(errno));
		}
		else
		{
			memset(request, 0, sizeof(request));
			read(fd, request, sizeof(request));
			ep("request: %s\n", request);
			if (strncmp(request, "print", 5) == 0)
			{
				PrintMacList();
			}
			else if (strncmp(request, "size", 4) == 0)
			{
				ep("all mac number: %d\n", MacList->size);
			}
			else
			{
				ep("unknow request cmd\n");
			}
		}
	}
}

void getPacket(u_char * arg, const struct pcap_pkthdr * pkthdr, const u_char * packet)
{
#if 0 
  int * id = (int *)arg;
  dp("id: %d\n", ++(*id));
  dp("Packet length: %d\n", pkthdr->len);
  dp("Number of bytes: %d\n", pkthdr->caplen);
  dp("Recieved time: %s", ctime((const time_t *)&pkthdr->ts.tv_sec)); 
#endif  
	unsigned char nheadlen = packet[2];
	//type=0 管理帧, type=1 控制帧, type=2 控制帧, type=3 未定义
	unsigned char type = (packet[nheadlen] >> 2) & 0x03;
	unsigned char subtype = (packet[nheadlen] >> 4) & 0x0f;
	//DS=0 所有管理与控制帧, DS=1 to AP数据帧, DS=2 from AP数据帧, DS=3 from AP to AP数据帧(4地址WDS)
	unsigned char DS = packet[nheadlen + 1] & 0x03;
	//dp("type:%02x subtype:%02x DS:%02x\n", type, subtype, DS);
	if (0x00 == type)
	{
		//所有管理帧的 MAC 标头都一样,这与帧的次类型无关
		//DA SA BSSID
		AddMacToList((void *)(packet + nheadlen + 4));
		AddMacToList((void *)(packet + nheadlen + 10));
	}
	else if (0x01 == type)
	{
		if (0x0a == subtype)
		{
			//power save poll
			AddMacToList((void *)(packet + nheadlen + 10));
		}
		else if (0x0b == subtype)
		{
			//RTS
			AddMacToList((void *)(packet + nheadlen + 4));
			AddMacToList((void *)(packet + nheadlen + 10));
		}
		else if (0x0c == subtype)
		{
			//CTS
			AddMacToList((void *)(packet + nheadlen + 4));
		}
		else if (0x0d == subtype)
		{
			//ACK
			AddMacToList((void *)(packet + nheadlen + 4));
		}
	}
	else if (0x02 == type)
	{
		if (1 == DS)
		{
			//BSSID SA DA
			AddMacToList((void *)(packet + nheadlen + 10));
			AddMacToList((void *)(packet + nheadlen + 16));
		}
		else if (2 == DS)
		{
			//DA BSSID SA
			AddMacToList((void *)(packet + nheadlen + 4));
			AddMacToList((void *)(packet + nheadlen + 16));
		}
		else if (0 == DS)
		{
			//DA SA BSSID(没抓到过DS=0的数据帧)
			AddMacToList((void *)(packet + nheadlen + 4));
			AddMacToList((void *)(packet + nheadlen + 10));
		}
		else
		{
			//WDS
		}
	}
}

int check_mac_char(char c)
{
	if((c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F'))
	{
		return 0;
	}
	return -1;
}

int check_mac(char *pc)
{
	if (NULL == pc)
	{
		return -1;
	}
	int i = 0;
	for(; i < 6; i++)
	{
		if (-1 == check_mac_char(pc[i]))
		{
			return -1;
		}
	}
	return 0;
}

int cal_char_int(char c)
{
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}
	else if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 10;
	}
	else if (c >= 'A' && c <= 'F')
	{
		return c - 'A' + 10;
	}
	return -1;
}

unsigned int cal_mac_int(char *pc)
{
	if (NULL == pc)
	{
		return 0;
	}
	int ret = 0;
	int i = 0;
	int tmp;
	for(; i < 6; i++)
	{
		if(-1 == (tmp = cal_char_int(pc[i])))
		{
			return 0;
		}
		ret = (ret << 4) + tmp;
	}
	return ret;
}

void init_router_list()
{
	FILE *fd = fopen("/etc/router.txt", "r");
	if (NULL == fd)
	{
		perror("fopen /etc/router.txt");
		return;
	}
	ssize_t read = 0;
	size_t len = 0;
	char *line = NULL;
	//统计有效行数
	while(-1 != (read = getline(&line, &len, fd)))
	{
		if (read == 7 && 0 == check_mac(line))
		{
			dp("line OK\n");
			router_list_size++;
		}
		else
		{
			dp("line ER\n");
		}
	}
	if (line)
	{
		free(line);
		line = NULL;
	}
	//分配内存用于存储mac
	dp("router size:%u\n", router_list_size);
	router_list = (unsigned int *)malloc(router_list_size * sizeof(unsigned int));
	//把mac读取到内存
	int i = 0;
	fseek(fd, 0, SEEK_SET);
	while(-1 != (read = getline(&line, &len, fd)))
	{
		if (read == 7 && 0 == check_mac(line))
		{
			router_list[i] = cal_mac_int(line);
			dp("router_list_size:%d, %06x\n", i, router_list[i]);
			i++;
		}
	}
	if (line)
	{
		free(line);
	}
	return;
}

int arg_option(int argc, char *argv[])
{
	#if 1
	int opt;
	while((opt=getopt(argc,argv, "vdi:")) != -1)
	{
		switch(opt)
		{
			case 'd':
			{
				daemon(0, 0);
				break;
			}
			case 'v':
			{
				dbg = 1;
				break;
			}
			case 'i':
			{
				memcpy(dev, optarg, strlen(optarg) > sizeof(dev) ? sizeof(dev) : strlen(optarg));
				break;
			}
			default:
			{
				break;
			}
			
		}
	}
	#endif
	if (dev[0] == 0)
	{
		dp("parameter error\n");
		exit(155);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	arg_option(argc, argv);

	char errBuf[PCAP_ERRBUF_SIZE];//, * devStr;

	/* open a device, wait until a packet arrives */
	pcap_t * device = pcap_open_live(dev, 65535, 0, 0, errBuf);
	if(!device)
	{
		ep("error: pcap_open_live(): %s\n", errBuf);
		exit(1);
	}

	//初始化mac列表
	MacList = InitList();

	//初始化路由器列表,用于过滤
	init_router_list();

	//创建改变channel的线程
	pthread_t threadid;
	if(0 != pthread_create(&threadid, NULL, thread_channel, NULL))
	{
		perror("pthread_create");
		exit(1);
	}

	if(0 != pthread_create(&threadid, NULL, thread_post, NULL))
	{
		perror("pthread_create");
		exit(1);
	}

	//创建控制线程
	if(0 != pthread_create(&threadid, NULL, thread_ctl, NULL))
	{
		perror("pthread_create");
		exit(1);
	}

	/* wait loop forever */
	int id = 0;
	pcap_loop(device, -1, getPacket, (u_char*)&id);

	pcap_close(device);

	return 0;
}

