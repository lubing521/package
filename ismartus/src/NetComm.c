
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include "NetComm.h"
#include "Common.h"

static CALLBACK_RECV_NET_FUN g_pCallBackRecvNetFun = NULL;
static unsigned char *g_pRecvNetBuff = NULL;
static unsigned int *g_pRecvNetBuffSize = NULL;
static struct sockaddr_in *g_pRecvNetBuffFrom = NULL;
static int g_nSocketFd = 0;
static pthread_t g_ptNetListen = 0;
static int g_PrintNet = 0;

static void* pthreadNetListen (void* arg) 
{
	socklen_t FromAddrLen = sizeof(struct sockaddr);
	int ret = 0;
	unsigned nNum = 0;
	struct timeval tvs2;
	struct timeval tve2;
	pthread_detach(pthread_self());
	while(1)
	{
		ret = recvfrom(g_nSocketFd, g_pRecvNetBuff, SIZE_NET_RECV_BUFF, 0, (struct sockaddr *)g_pRecvNetBuffFrom, &FromAddrLen);
		if (-1 == ret)
		{
			continue;
		}
		if (NULL != g_pRecvNetBuffSize)
		{
			*g_pRecvNetBuffSize = ret;
		}
		if (NULL != g_pRecvNetBuffFrom)
		{
			TRACE("recvfrom %d: IP:%s Port:%d size:%d\n", nNum, inet_ntoa(g_pRecvNetBuffFrom->sin_addr),
				 ntohs(g_pRecvNetBuffFrom->sin_port), ret);
		}
		if (g_PrintNet)
		{
			TRACE("recvfrom \n");
			PrintDataBy16(g_pRecvNetBuff, ret);
		}

		nNum++;
		gettimeofday(&tvs2, NULL);
		(*g_pCallBackRecvNetFun)();
		gettimeofday(&tve2, NULL);
		TRACE("Deal Net : %ld####\n", (tve2.tv_sec - tvs2.tv_sec)*1000000 + tve2.tv_usec - tvs2.tv_usec);
	}
	return NULL;
}

int StartNetService()
{
	if (0 != g_nSocketFd)
	{
		TRACEERR("socket create fail,it is exist!\n");
		return ReturnError;
	}
	struct sockaddr_in addr;
	g_nSocketFd = socket(AF_INET, SOCK_DGRAM, 0);
	//setsockopt(g_nSocketFd, SOL_SOCKET,SO_BROADCAST,&so_broadcast,sizeof(so_broadcast));
	memset((void *)(&addr), 0, sizeof(struct sockaddr_in));
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=htonl(INADDR_ANY);
	addr.sin_port=htons(ISMART_PORT);
	if(bind(g_nSocketFd, (struct sockaddr *)&addr,sizeof(struct sockaddr_in))<0)
	{
		perror("bind");
		g_nSocketFd = 0;
		return ReturnError;
	}
	if (0 != pthread_create(&g_ptNetListen, NULL, pthreadNetListen, NULL))
	{
		perror("pthread_create");
		return ReturnError;
	}
	TRACE("start net server success!\n");

	return ReturnSuccess;
}

int StopNetService()
{
	if (0 == g_nSocketFd)
	{
		return ReturnError;
	}
	close(g_nSocketFd);
	return ReturnSuccess;
}

int SendDataToNet(struct sockaddr_in *pSockAddr, char *pData, unsigned int nLen)
{
	if (0 == g_nSocketFd || NULL == pSockAddr || NULL == pData)
	{
		return ReturnError;	
	}
	TRACE("sendto : IP:%s Port:%d size:%d\n", inet_ntoa(pSockAddr->sin_addr), ntohs(pSockAddr->sin_port), nLen);
	if (g_PrintNet)
	{
		TRACE("sendto \n");
		PrintDataBy16((unsigned char *)pData, nLen);
	}
	return sendto(g_nSocketFd, pData, nLen, 0, (struct sockaddr *)pSockAddr, sizeof(struct sockaddr));
}

/*pIP格式：192.168.3.123，nPort为本地字节序*/
int SendDataToNetByCP(char * pIP, unsigned short nPort, char *pData, unsigned int nLen)
{
	if (0 == g_nSocketFd || NULL == pIP || NULL == pData)
	{
		return ReturnError;	
	}
	struct sockaddr_in ToAddr;
	memset((void *)(&ToAddr), 0, sizeof(struct sockaddr_in));
	ToAddr.sin_family=AF_INET;
	ToAddr.sin_port=htons(nPort);
	ToAddr.sin_addr.s_addr=inet_addr(pIP);
	return SendDataToNet(&ToAddr, pData, nLen);
}

/*pIP和nPort为网络字节序*/
int SendDataToNetByNetOrder(struct in_addr *pIP, unsigned short nPort, char *pData, unsigned int nLen)
{
	if (0 == g_nSocketFd)
	{
		return ReturnError;	
	}
	struct sockaddr_in ToAddr;
	memset((void *)(&ToAddr), 0, sizeof(struct sockaddr_in));
	ToAddr.sin_family=AF_INET;
	ToAddr.sin_port=nPort;    //nPort直接赋值，因为已经是网络字节序
	ToAddr.sin_addr.s_addr=pIP->s_addr;
	return SendDataToNet(&ToAddr, pData, nLen);
}

int SetCallBackRecvNet(CALLBACK_RECV_NET_FUN pRecvNetFun)
{
	if (NULL == pRecvNetFun)
	{
		return ReturnError;
	}
	g_pCallBackRecvNetFun = pRecvNetFun;
	return ReturnSuccess;
}

int SetCallBackRecvNetBuff(unsigned char *pRecvNetBuff)
{
	if (NULL == pRecvNetBuff)
	{
		return ReturnError;
	}
	g_pRecvNetBuff = pRecvNetBuff;
	return ReturnSuccess;
}

int SetCallBackRecvNetBuffSize(unsigned int *pRecvNetBuffSize)
{
	if (NULL == pRecvNetBuffSize)
	{
		return ReturnError;
	}
	g_pRecvNetBuffSize = pRecvNetBuffSize;
	return ReturnSuccess;
}

int SetCallBackRecvNetAddr(struct sockaddr_in *pRecvNetBuffFrom)
{
	if (NULL == pRecvNetBuffFrom)
	{
		return ReturnError;
	}

	g_pRecvNetBuffFrom = pRecvNetBuffFrom;
	return ReturnSuccess;
}


int SetNetPrint()
{
	g_PrintNet = 1;
	return ReturnSuccess;
}
int SetNetNoPrint()
{
	g_PrintNet = 0;
	return ReturnSuccess;
}

