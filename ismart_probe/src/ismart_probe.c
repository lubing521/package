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
#include "radiotap_iter.h"
#include "wireless.h"

#define dp(...) do { if (dbg) { fprintf(stderr, __VA_ARGS__); } } while(0)
#define ep(...) do { fprintf(stderr, __VA_ARGS__); } while(0)

#define FILE_NAME_DATA_POST "/tmp/probe_data_post.txt"
#define FILE_NAME_CONF_POST "/tmp/probe_conf_post.txt"
#define FILE_NAME_CONF_RET "/tmp/probe_conf_ret.txt"
#define FILE_NAME_ROUTER_MAC "/etc/router.txt"

static int dbg = 0; //打印标志
static int ctl_socket_server = 0;
static char dev_name[32] = {0};
static int bg_chans  [] = 
{
	1, 7, 2, 8, 3, 9, 4, 10, 5, 11, 6
};
static int8_t dbm_limit = (int8_t)-100;

//数据来源: http://standards.ieee.org/develop/regauth/oui/oui.txt 
static unsigned int *router_list = NULL;
static unsigned int router_list_size = 0;

//static int fcshdr = 0;

static const struct radiotap_align_size align_size_000000_00[] = {
	[0] = { .align = 1, .size = 4, },
	[52] = { .align = 1, .size = 4, },
};

static const struct ieee80211_radiotap_namespace vns_array[] = {
	{
		.oui = 0x000000,
		.subns = 0,
		.n_bits = sizeof(align_size_000000_00),
		.align_size = align_size_000000_00,
	},
};

static const struct ieee80211_radiotap_vendor_namespaces vns = {
	.ns = vns_array,
	.n_ns = sizeof(vns_array)/sizeof(vns_array[0]),
};

#if 0
static void print_radiotap_namespace(struct ieee80211_radiotap_iterator *iter)
{
	switch (iter->this_arg_index) {
	case IEEE80211_RADIOTAP_TSFT:
		printf("\tTSFT: %llu\n", le64toh(*(unsigned long long *)iter->this_arg));
		break;
	case IEEE80211_RADIOTAP_FLAGS:
		printf("\tflags: %02x\n", *iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_RATE:
		printf("\trate: %lf\n", (double)*iter->this_arg/2);
		break;
	case IEEE80211_RADIOTAP_CHANNEL:
	case IEEE80211_RADIOTAP_FHSS:
	case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
	case IEEE80211_RADIOTAP_DBM_ANTNOISE:
	case IEEE80211_RADIOTAP_LOCK_QUALITY:
	case IEEE80211_RADIOTAP_TX_ATTENUATION:
	case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
	case IEEE80211_RADIOTAP_DBM_TX_POWER:
	case IEEE80211_RADIOTAP_ANTENNA:
	case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
	case IEEE80211_RADIOTAP_DB_ANTNOISE:
	case IEEE80211_RADIOTAP_TX_FLAGS:
		break;
	case IEEE80211_RADIOTAP_RX_FLAGS:
		if (fcshdr) {
			printf("\tFCS in header: %.8x\n",
				le32toh(*(uint32_t *)iter->this_arg));
			break;
		}
		printf("\tRX flags: %#.4x\n",
			le16toh(*(uint16_t *)iter->this_arg));
		break;
	case IEEE80211_RADIOTAP_RTS_RETRIES:
	case IEEE80211_RADIOTAP_DATA_RETRIES:
		break;
		break;
	default:
		printf("\tBOGUS DATA\n");
		break;
	}
}
#endif

static void get_signal(struct ieee80211_radiotap_iterator *iter, int8_t *dbm_antsignal)
{
	switch (iter->this_arg_index) {
	case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
        *dbm_antsignal = *(int8_t *)iter->this_arg;
//        dp("dbm_antsignal:%d\n", *dbm_antsignal);
        break;
    default:
        break;
    }
}

#if 0
static void print_test_namespace(struct ieee80211_radiotap_iterator *iter)
{
	switch (iter->this_arg_index) {
	case 0:
	case 52:
		printf("\t00:00:00-00|%d: %.2x/%.2x/%.2x/%.2x\n",
			iter->this_arg_index,
			*iter->this_arg, *(iter->this_arg + 1),
			*(iter->this_arg + 2), *(iter->this_arg + 3));
		break;
	default:
		printf("\tBOGUS DATA - vendor ns %d\n", iter->this_arg_index);
		break;
	}
}
#endif

static const struct radiotap_override overrides[] = {
	{ .field = 14, .align = 4, .size = 4, }
};
 
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
//	char cmd[64] = {0};
	int i = 0;
    int ret = 0;
//    FILE *fp;

//	struct timeval tvs2;
//	struct timeval tve2;
	dp("channel hop \n");
	while(1)
	{
//		sprintf(cmd, "iw phy0 set channel %d", bg_chans[i]);
//		gettimeofday(&tvs2, NULL);
		//system()执行实际需要24ms
//		ret = system(cmd);
//        fp = popen(cmd, "w");
//        if (NULL == fp)
//        {
//            perror("popen");
//            continue;
//        }
//		fputs(cmd, fp);  
//        ret = pclose(fp);
//		printf("iw phy0 set channel %d : ret %d\n", bg_chans[i], ret);
//        perror("system");
//        perror("popen");
//		gettimeofday(&tve2, NULL);
//		dp("%s: time %ld\n", cmd, (tve2.tv_sec - tvs2.tv_sec)*1000000 + tve2.tv_usec - tvs2.tv_usec);
        ret = wireless_set_channel("phy0", bg_chans[i]);
		printf("wireless_set_channel %d : ret %d\n", bg_chans[i], ret);
		usleep(250 * 1000);
		i++;
		if(10 < i)
		{
			i = 0;
		}
	}
	return NULL;
}

void get_mac(struct ifreq *pifreq)
{
    int sock;
    if (NULL == pifreq)
    {
        return;
    }
    if((sock=socket(AF_INET,SOCK_STREAM,0))<0)
    {
            perror("socket");
            return;
    }
    strcpy(pifreq->ifr_name, dev_name);
    if(ioctl(sock,SIOCGIFHWADDR, pifreq)<0)
    {
            perror("ioctl");
            return;
    }
    return;
}

void post_data()
{
    //w+ 打开可读写文件，若文件存在则文件长度清为零，即该文件内容会消失。若文件不存在则建立该文件
    FILE *fp = fopen(FILE_NAME_DATA_POST, "w+");
    if (NULL == fp)
    {
        return;
    }
    char buf[128] = {0};

    struct ifreq ifreq;
    get_mac(&ifreq);
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
//		if (1 == i)
//		{
//			sprintf(buf, "%d={\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"firsttime\":\"0\",\"leavetime\":\"0\"}", i,
//				((unsigned char *)pnode->data)[0], ((unsigned char *)pnode->data)[1],
//				((unsigned char *)pnode->data)[2], ((unsigned char *)pnode->data)[3],
//				((unsigned char *)pnode->data)[4], ((unsigned char *)pnode->data)[5]);
//		}
//		else
//		{
            //多了&
            sprintf(buf, "&%d={\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"db\":\"%d\"}", i,
                ((unsigned char *)pnode->data)[0], ((unsigned char *)pnode->data)[1],
                ((unsigned char *)pnode->data)[2], ((unsigned char *)pnode->data)[3],
                ((unsigned char *)pnode->data)[4], ((unsigned char *)pnode->data)[5],
                ((int8_t *)pnode->data)[6]);
//		}
        fwrite(buf, 1, strlen(buf), fp);
    }
    fclose(fp);
    system("maclist=$(cat "FILE_NAME_DATA_POST") && wget -O /dev/null --post-data=$maclist http://boxdata.aijee.cn/wifidetect.php");
    ClearList(MacList);

    return;
}

void post_conf(unsigned int *interval_data, int8_t *dblimit)
{
    if (NULL == interval_data || NULL == dblimit)
    {
        return;
    }
    int ret = 0;
    char buf[128] = {0};
    struct ifreq ifreq;
    get_mac(&ifreq);
    FILE *fp = fopen(FILE_NAME_CONF_POST, "w+");
    if (NULL == fp)
    {
        return;
    }
    sprintf(buf, "mac=%02x:%02x:%02x:%02x:%02x:%02x&upfreq=%u&dblimit=%d",
                    (unsigned char)ifreq.ifr_hwaddr.sa_data[0],
                    (unsigned char)ifreq.ifr_hwaddr.sa_data[1],
                    (unsigned char)ifreq.ifr_hwaddr.sa_data[2],
                    (unsigned char)ifreq.ifr_hwaddr.sa_data[3],
                    (unsigned char)ifreq.ifr_hwaddr.sa_data[4],
                    (unsigned char)ifreq.ifr_hwaddr.sa_data[5],
                    *interval_data, *dblimit);
    fwrite(buf, 1, strlen(buf), fp);
    fclose(fp);
    unlink(FILE_NAME_CONF_RET);
    ret = system("maclist=$(cat "FILE_NAME_CONF_POST") && wget -O "FILE_NAME_CONF_RET
                " --post-data=$maclist http://boxdata.aijee.cn/wifidetectconf.php");
    if (ret < 0)
    {
        perror("system");
        return;
    }
    fp = fopen(FILE_NAME_CONF_RET, "r");
    if (NULL == fp)
    {
        perror("fopen");
        return;
    }
	ssize_t read = 0;
	size_t len = 0;
	char *line = NULL;
    printf("interval_data:%u, dblimit:%d\n", *interval_data, *dblimit);
	while(-1 != (read = getline(&line, &len, fp)))
	{
        int dblimit_int = 0;
        int sscanf_ret = 0;
//        sscanf_ret = sscanf(line, "{\"upfreq\":\"%u\",\"dblimit\":\"%d\"}", interval_data, &dblimit_int);
        sscanf_ret = sscanf(line, "{\"upfreq\":%u,\"dblimit\":%d}", interval_data, &dblimit_int);
        if (2 == sscanf_ret) //2代表2个参数获取成功
        {
            *dblimit = dblimit_int;
            printf("sscanf:%d, interval_data:%u, dblimit:%d\n", sscanf_ret, *interval_data, *dblimit);
        }
	}
    fclose(fp);
	if (line)
	{
		free(line);
		line = NULL;
	}
    return;
}

void *thread_post(void *arg)
{
	pthread_detach(pthread_self());
    unsigned int interval_data = 2*60;
    const unsigned int interval_conf = 60*60;
    unsigned int conf_time = 0;

    //一开始就获取一次配置
    post_conf(&interval_data, &dbm_limit);
	while(1)
	{
		sleep(interval_data);
        conf_time += interval_data;
        post_data();
        if (conf_time >= interval_conf)
        {
            //每1小时重新获取一次配置
            conf_time = 0;
            post_conf(&interval_data, &dbm_limit);
        }
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
		ep("%4d %02x:%02x:%02x:%02x:%02x:%02x,%d\n", i++, 
				((unsigned char *)pnode->data)[0], ((unsigned char *)pnode->data)[1],
				((unsigned char *)pnode->data)[2], ((unsigned char *)pnode->data)[3],
				((unsigned char *)pnode->data)[4], ((unsigned char *)pnode->data)[5],
                ((int8_t *)pnode->data)[6]);
	}
}

void AddMacToList(void *mac, int8_t dbm_antsignal)
{
    if (dbm_antsignal < dbm_limit)
    {
        //小于最小信号强度限制,抛弃
        return;
    }
    int8_t buf[7] = {0};
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
            ((int8_t *)pnode->data)[6] = dbm_antsignal;
			return;
		}
	}
    memcpy(buf, mac, 6);
    buf[6] = dbm_antsignal;
	EnListTail(MacList, buf, 7);
	dp("%4d %02x:%02x:%02x:%02x:%02x:%02x, %d\n", MacList->size, 
                ((unsigned char *)mac)[0], ((unsigned char *)mac)[1],
				((unsigned char *)mac)[2], ((unsigned char *)mac)[3],
				((unsigned char *)mac)[4], ((unsigned char *)mac)[5],
                dbm_antsignal);
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
	struct ieee80211_radiotap_iterator iter;
    void *data = (void *)packet;
    int err = 0;
    int i = 0;
    int8_t dbm_antsignal = 0;

	unsigned char nheadlen = packet[2];
	//type=0 管理帧, type=1 控制帧, type=2 数据帧, type=3 未定义
	unsigned char type = (packet[nheadlen] >> 2) & 0x03;
	unsigned char subtype = (packet[nheadlen] >> 4) & 0x0f;
	//DS=0 所有管理与控制帧, DS=1 to AP数据帧, DS=2 from AP数据帧, DS=3 from AP to AP数据帧, DS=4 WDS(4地址)
	unsigned char DS = packet[nheadlen + 1] & 0x03;
	//dp("type:%02x subtype:%02x DS:%02x\n", type, subtype, DS);

	err = ieee80211_radiotap_iterator_init(&iter, data, pkthdr->caplen, &vns);
	if (err) {
		printf("malformed radiotap header (init returns %d)\n", err);
		return;
	}
	while (!(err = ieee80211_radiotap_iterator_next(&iter))) {
		if (iter.this_arg_index == IEEE80211_RADIOTAP_VENDOR_NAMESPACE) {
			printf("\tvendor NS (%.2x-%.2x-%.2x:%d, %d bytes)\n",
				iter.this_arg[0], iter.this_arg[1],
				iter.this_arg[2], iter.this_arg[3],
				iter.this_arg_size - 6);
			for (i = 6; i < iter.this_arg_size; i++) {
				if (i % 8 == 6)
					printf("\t\t");
				else
					printf(" ");
				printf("%.2x", iter.this_arg[i]);
			}
			printf("\n");
		} else if (iter.is_radiotap_ns)
        {
//			print_radiotap_namespace(&iter);
            get_signal(&iter, &dbm_antsignal);
        }
//		else if (iter.current_namespace == &vns_array[0])
//		{
//			print_test_namespace(&iter);
//		}
	}
    if (0 == dbm_antsignal)
    {
//        printf("dbm_antsignal:%d\n", dbm_antsignal);
        return;
    }

	if (0x00 == type)
	{
		//所有管理帧的 MAC 标头都一样,这与帧的次类型无关(80211无线网络权威指南)
		//DA SA BSSID
		//AddMacToList((void *)(packet + nheadlen + 4), dbm_antsignal);
		AddMacToList((void *)(packet + nheadlen + 10), dbm_antsignal);
	}
	else if (0x01 == type)
	{
		if (0x0a == subtype)
		{
			//power save poll, TransmitterAddress
			AddMacToList((void *)(packet + nheadlen + 10), dbm_antsignal);
		}
		else if (0x0b == subtype)
		{
			//RTS, ReceiverAddress TransmitterAddress
			//AddMacToList((void *)(packet + nheadlen + 4), dbm_antsignal);
			AddMacToList((void *)(packet + nheadlen + 10), dbm_antsignal);
		}
		else if (0x0c == subtype)
		{
			//CTS, ReceiverAddress
			//AddMacToList((void *)(packet + nheadlen + 4), dbm_antsignal);
		}
		else if (0x0d == subtype)
		{
			//ACK, ReceiverAddress
			//AddMacToList((void *)(packet + nheadlen + 4), dbm_antsignal);
		}
	}
	else if (0x02 == type)
	{
		if (1 == DS)
		{
			//BSSID SA DA
			AddMacToList((void *)(packet + nheadlen + 10), dbm_antsignal);
			//AddMacToList((void *)(packet + nheadlen + 16), dbm_antsignal);
		}
		else if (2 == DS)
		{
			//DA BSSID SA
			//AddMacToList((void *)(packet + nheadlen + 4), dbm_antsignal);
			AddMacToList((void *)(packet + nheadlen + 16), dbm_antsignal);
		}
		else if (0 == DS)
		{
			//DA SA BSSID(没抓到过DS=0的数据帧)
			//AddMacToList((void *)(packet + nheadlen + 4), dbm_antsignal);
			AddMacToList((void *)(packet + nheadlen + 10), dbm_antsignal);
		}
		else
		{
            //RA TA DA SA
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
	FILE *fd = fopen(FILE_NAME_ROUTER_MAC, "r");
	if (NULL == fd)
	{
		perror("fopen "FILE_NAME_ROUTER_MAC);
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
    fclose(fd);
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
				memcpy(dev_name, optarg, strlen(optarg) > sizeof(dev_name) ? sizeof(dev_name) : strlen(optarg));
				break;
			}
			default:
			{
				break;
			}
			
		}
	}
	#endif
	if (dev_name[0] == 0)
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
	pcap_t * device = pcap_open_live(dev_name, 65535, 0, 0, errBuf);
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

