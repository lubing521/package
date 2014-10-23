
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include "PublicMethod.h"
#include "NetComm.h"
#include "SpiComm.h"
#include "Protocol.h"
#include "Common.h"
#include "List.h"

#define PROTOCOL_VERSION 0x01 /*协议版本*/
#define SERVER_PORT_REG 3001 /*注册服务器端口*/
#define QUERYNODE_IP "1.2.3.4" /*该IP用来标注节点查询线程发送的节点查询包*/

#define PACKAGE_SIZE_XINTIAO PACKAGE_SIZE_HEAD
#define PACKAGE_SIZE_INITCTLSER PACKAGE_SIZE_HEAD
#define PACKAGE_SIZE_BROADDIS PACKAGE_SIZE_HEAD
#define PACKAGE_SIZE_INITSRCMAC PACKAGE_SIZE_HEAD
#define PACKAGE_SIZE_QUERYNODERET PACKAGE_SIZE_HEAD+400

#define FRE_XINTIAO 3
#define TIMEOUT_XINTIAO 10
#define FRE_INIT_SOURCEMAC 3
#define FRE_INIT_REGSERVER 3
#define FRE_QUERY_NODE 1000000 //毫秒

#define SMALL_END 1

#define PROTOCOL_FUN_QUERY 0xff /*通用查询指令,所有设备适用*/
#define PROTOCOL_FUN_QUERYRET 0xfe /*通用查询返回指令,所有设备适用*/
#define PROTOCOL_FUN_GATEWAY_QUERYNODE 0x08 /*节点状态查询，网关盒子适用*/
#define PROTOCOL_FUN_GATEWAY_QUERYNODERET 0x09 /*节点状态查询返回，网关盒子适用*/

#define PROTOCOL_TYPE_GATEWAY 0x00 /*网关盒子*/
#define PROTOCOL_TYPE_COLORLIGHT 0x10 /*彩色灯*/
#define PROTOCOL_TYPE_FOURLIGNT 0x1c /*四路灯*/
#define PROTOCOL_TYPE_THREELIGNT 0x1f /*三路灯*/
#define PROTOCOL_TYPE_TWOLIGNT 0x1e /*二路灯*/
#define PROTOCOL_TYPE_ONELIGNT 0x1d /*一路灯*/
#define PROTOCOL_TYPE_INFRARED 0x20 /*红外盒子*/
#define PROTOCOL_TYPE_ENVIRONMENT 0x30 /*环境采集*/
#define PROTOCOL_TYPE_POWERSOCKET 0x40 /*功耗插座*/
#define PROTOCOL_TYPE_CONTROLSOCKET 0x41 /*控制插座*/
#define PROTOCOL_TYPE_VOICE 0x05 /*语音*/

#define MAC_SIZE 2

#define GUOGEE_MAC_0 0x00 
#define GUOGEE_MAC_1 0x12
#define GUOGEE_MAC_2 0x4b 
#define GUOGEE_MAC_3 0x00 
#define GUOGEE_MAC_4 0x05 
#define GUOGEE_MAC_5 0x0e

enum REMOTE_SERVER_STATUS
{
	RemoteServerOn,
	RemoteServerOff
};

enum SOURCE_MAC_STATUS
{
	SourceMacNoInit,
	SourceMacInited
};

/*设备节点信息,存储在链表*/
typedef struct Item
{
	unsigned char mac[MAC_SIZE];
	unsigned char type;
	unsigned char status;
	long ntime; /*保存接收到的时间*/
}Item;


static ISmartFrame *g_iSmartNetFrame = NULL;
static int g_iSmartNetFrameSize = 0;
static struct sockaddr_in *g_iSmartNetFrameFrom = NULL;
static ISmartFrame *g_iSmartSpiFrame = NULL;
static int g_iSmartSpiFrameSize = 0;
static unsigned char g_SourceMac[8] = {0};
static int g_enSourceMacFlag = SourceMacNoInit;/*sourcemac初始化标志*/
static struct in_addr g_RegServerAddr = {0};
static struct in_addr g_CtlServerAddr = {0};
static unsigned short g_CtlServerPort = 0;
static struct in_addr g_DataServerAddr = {0};
static unsigned short g_DataServerPort = 0;
static int g_RemoteServerFlag = RemoteServerOff;
static int g_nRemoteserverTime = 0;
static pthread_t g_ptXinTiao = 0;
static pthread_t g_ptNode = 0;
static List *g_pNodeList = NULL;

void* pthreadXinTiao (void* arg);
int ClearISmartFrame(ISmartFrame *pISmartFrame);
int ProtocolInit();
int InitRegServerAddr();
int InitCtlServerAddr();
int InitSourceMac();
int CheckAA55(ISmartFrame *pISMartFrame);
int CheckCRC(ISmartFrame *pISMartFrame);
int CheckExistTargetMac(ISmartFrame *pISMartFrame);
int CheckInitSourceMac();
int CheckInitServer();
int ClearRemoteServer();
int ClearInitSourceMac();
int ClearNodeList();
int FillAA55(ISmartFrame *pISMartFrame);
int CheckExistIP(ISmartFrame *pISMartFrame);
int FillIP(ISmartFrame *pISMartFrame, struct in_addr *pInAddr);
int FillPort(ISmartFrame *pISMartFrame, unsigned short nPort);
int FillSeq(ISmartFrame *pISMartFrame, unsigned short nseq);
int FillSourceMac(ISmartFrame *pISMartFrame);
int FillCRC(ISmartFrame *pISMartFrame);
struct in_addr GetIpFromFrame(ISmartFrame *pISMartFrame);
unsigned short GetPortFromFrame(ISmartFrame *pISMartFrame);
int SendFrameToCrlServer(ISmartFrame *pISMartFrame, unsigned int nLen);
int SendFrameToRegServer(ISmartFrame *pISMartFrame, unsigned int nLen);
int SendFrameToDataServer(ISmartFrame *pISMartFrame, unsigned int nLen);
int SendFrameToNet(ISmartFrame *pISMartFrame, unsigned int nLen);
int SendFrameToSpi(ISmartFrame *pISMartFrame, unsigned int nLen);
int PackXinTiao(ISmartFrame *pISMartFrame);
int PackInitCtrServer(ISmartFrame *pISMartFrame);
int PackBroadcastDiscovery(ISmartFrame *pISMartFrame);
int PackInitSourceMac(ISmartFrame *pISMartFrame);
int PackGatewayNodeRet(ISmartFrame *pISMartFrame, unsigned int *pnLen);
int PackQueryNode(ISmartFrame *pISMartFrame, PNode pnode);
//int DealNetProtocol(unsigned char *pData, int nLen, struct sockaddr_in *pNetRecvBuffFrom);
//int DealSpiProtocol(unsigned char *pData, int nLen);
int MangleSpiPackage();
int MangleNetPackage();
int FilterBroadcastDiscovery(ISmartFrame *pISMartFrame);
int FilterCtlServerGet(ISmartFrame *pISMartFrame);
int FilterXinTiaoRet(ISmartFrame *pISMartFrame);
int FilterGetSourceMac(ISmartFrame *pISMartFrame);
int FilterGatewayNode(ISmartFrame *pISMartFrame);
int FilterQueryNodeRet(ISmartFrame *pISMartFrame);
int UpdateDataToList(List *plist, Item *pitem);
int GetItemFromColorLight(ISmartFrame *pISMartFrame, Item *pitem);
int GetItemFromOneLight(ISmartFrame *pISMartFrame, Item *pitem);
int GetItemFromTwoLight(ISmartFrame *pISMartFrame, Item *pitem);
int GetItemFromThreeLight(ISmartFrame *pISMartFrame, Item *pitem);
int GetItemFromFourLight(ISmartFrame *pISMartFrame, Item *pitem);
int GetItemFromPowerSocket(ISmartFrame *pISMartFrame, Item *pitem);
int GetItemFromControlSocket(ISmartFrame *pISMartFrame, Item *pitem);
int GetItemFromAny(ISmartFrame *pISMartFrame, Item *pitem);

void *pthreadNode(void *arg)
{
	ISmartFrame QueryNode = {{0}};
	PNode pnode = g_pNodeList->front;
	while(1)
	{
		#if 1
		int nFlag = 0;
		if (pnode)
		{
			if (ReturnSuccess == PackQueryNode(&QueryNode, pnode))
			{
				nFlag = 1;
			}
		}
		else
		{
			pnode = g_pNodeList->front;
			if (pnode)
			{
				if (ReturnSuccess == PackQueryNode(&QueryNode, pnode))
				{
					nFlag = 1;
				}
			}
		}
		if (nFlag)
		{
			Item *item = pnode->data;
			TRACE("QueryNode %d:%02x%02x\n", GetListSize(g_pNodeList), item->mac[0], item->mac[1]);
			SendFrameToSpi(&QueryNode, PACKAGE_SIZE_HEAD);
			pnode = pnode->next;
		}
		else
		{
			TRACE("NodeList NULL\n");
		}
		#endif
		usleep(FRE_QUERY_NODE);
	}
}

void *pthreadXinTiao (void *arg)
{
	ISmartFrame XinTiao = {{0}};
	while(1)
	{
		/*初始化源mac*/
		while (SourceMacNoInit == CheckInitSourceMac())
		{
			TRACEERR("sourcemac init now\n");
			InitSourceMac();
			sleep(FRE_INIT_SOURCEMAC);
		}
		TRACEERR("sourcemac init success\n");

		/*打包心跳包*/
		while (1)
		{
			sleep(FRE_XINTIAO);
			if (ReturnSuccess == PackXinTiao(&XinTiao))
			{
				/*打包成功*/
				break;
			}
		}
		/*usb转串口断开,源mac被清除,需要重新初始化源mac*/
		while(SourceMacInited == CheckInitSourceMac())
		{
			sleep(FRE_XINTIAO);
			//InitCtlServerAddr(); /*同时向注册服务器注册,针对服务器曾经的bug*/
			SendFrameToCrlServer(&XinTiao, PACKAGE_SIZE_XINTIAO);
			/*心跳包(TIMEOUT_XINTIAO*FRE_XINTIAO)秒没有收到返回，需要重新向注册服务器申请*/
			g_nRemoteserverTime++;
			if (g_nRemoteserverTime > TIMEOUT_XINTIAO) /*收到心跳返回包,将清空计数*/
			{
				ClearRemoteServer();
				g_nRemoteserverTime = 0;
			}
		}
	}
	return NULL;
}

int ClearISmartFrame(ISmartFrame *pISmartFrame)
{
	if (NULL == pISmartFrame)
	{
		return ReturnError;
	}	
	memset(pISmartFrame,0,sizeof(ISmartFrame));
	return ReturnSuccess;
}

int ProtocolInit()
{
	g_pNodeList = InitList();
	/*创建查询节点状态线程*/
	if (0 != pthread_create(&g_ptNode, NULL, pthreadNode, NULL))
	{
		return ReturnError;
	}
	/*初始化sourcemac*/
	/*设备拔除后，新的设备的sourcemac可能不一样，所以sourcemac是动态的，不再开始初始化
	while (SourceMacNoInit == CheckInitSourceMac())
	{
		TRACE("sourcemac init now\n");
		InitSourceMac();
		sleep(FRE_INIT_SOURCEMAC);
	}
	TRACE("sourcemac init success\n");
	*/
	/*创建心跳线程*/
	if (0 != pthread_create(&g_ptXinTiao, NULL, pthreadXinTiao, NULL))
	{
		return ReturnError;
	}

	/*初始化注册服务器*/
	/*
	while(ReturnError == InitRegServerAddr())
	{
		sleep(FRE_INIT_REGSERVER);
	}
	*/
	/*初始化控制服务器*/
	//InitCtlServerAddr();


	return ReturnSuccess;
}

int InitRegServerAddr()
{
	struct hostent *pHostReg = NULL;
	char str[32] = {0};
	if(NULL == (pHostReg = gethostbyname(REG_SERVRE_DOMAIN)))
	{
		TRACEERR("gethostbyname error for host:%s\n", REG_SERVRE_DOMAIN);
		return ReturnError; 
	}
	if (AF_INET !=  pHostReg->h_addrtype)
	{
		return ReturnError;	
	}
	TRACE("RegServer address:%s\n", inet_ntop(pHostReg->h_addrtype, *pHostReg->h_addr_list, str, sizeof(str)));
	memcpy (&g_RegServerAddr, (struct in_addr *)(*pHostReg->h_addr_list), sizeof(struct in_addr));
	return ReturnSuccess;
}

int InitCtlServerAddr()
{
	ISmartFrame oSendFrame = {{0}};
	if (ReturnError == PackInitCtrServer(&oSendFrame))
	{
		TRACE("PackInitCtrServer fail!!\n");
		return ReturnError;
	}
	TRACE("PackInitCtrServer success!!\n");
	SendFrameToRegServer(&oSendFrame, PACKAGE_SIZE_INITCTLSER);
	return ReturnSuccess;	
}

int InitSourceMac()
{
	ISmartFrame oSendFrame = {{0}};
	if (ReturnError == PackInitSourceMac(&oSendFrame))
	{
		return ReturnError;
	}
	SendFrameToSpi(&oSendFrame, PACKAGE_SIZE_INITSRCMAC);
	return ReturnSuccess;
}

int CheckAA55(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	//printf("%2x\n", pISMartFrame->heda[0]);
	//printf("%2x\n", pISMartFrame->heda[1]);
	if (PACKAGE_HEDAH != pISMartFrame->heda[0]
		|| PACKAGE_HEDAL != pISMartFrame->heda[1])
	{
		TRACEERR("CheckAA55 function \n");
		return ReturnError;
	}
	return ReturnSuccess;
	
}

int CheckCRC(ISmartFrame *pISMartFrame)
{
	unsigned short nCRC = 0;
	unsigned short nCRCFrame = 0;
	nCRC = getCheckSum((const unsigned char *)&pISMartFrame->frameLength, 42, SMALL_END);
	nCRCFrame = ntohs(pISMartFrame->crc);
	if (nCRC != nCRCFrame)
	{
		return ReturnError;
	}
	return ReturnSuccess;
}

int CheckExistTargetMac(ISmartFrame *pISMartFrame)
{
	//char tmp[8] = {0};
	//return memcmp(tmp, pISMartFrame->targetMac, 8);	
	int i = 0;
	for (i = 0; i < 8; i++)
	{
		if (0 != pISMartFrame->targetMac[i])
		{
			return ReturnSuccess;
		}
	}
	return ReturnError;
}

int CheckInitSourceMac()
{
	//char tmp[8] = {0};
	//return memcmp(tmp, g_SourceMac, 8);
	return g_enSourceMacFlag;
}

/*重新注册服务器,清除现有服务器设置*/
int ClearRemoteServer()
{
	g_RemoteServerFlag = RemoteServerOff;
	g_RegServerAddr.s_addr = 0;
	TRACEERR("Remote Server clear success\n");
	return ReturnSuccess;
}

/*清除源mac*/
int ClearInitSourceMac()
{
	memset(g_SourceMac, 0, sizeof(g_SourceMac));
	g_enSourceMacFlag = SourceMacNoInit;
	TRACEERR("sourcemac clear success\n");
	//g_RemoteServerFlag = RemoteServerOff;
	//g_RegServerAddr.s_addr = 0;

	ClearRemoteServer();
	return ReturnSuccess;
}

/*清空列表*/
int ClearNodeList()
{
	ClearList(g_pNodeList);
	return ReturnSuccess;
}

int CheckInitServer()
{
	return g_RemoteServerFlag;
}

int CheckExistIP(ISmartFrame *pISMartFrame)
{
	char tmp[4] = {0};
	return memcmp(tmp, pISMartFrame->ip, 4);
}

int FillAA55(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	pISMartFrame->heda[0] = PACKAGE_HEDAH;
	pISMartFrame->heda[1] = PACKAGE_HEDAL;
	return ReturnSuccess;
}

int FillIP(ISmartFrame *pISMartFrame, struct in_addr *pInAddr)
{
	if (NULL == pISMartFrame || NULL == pInAddr)
	{
		return ReturnError;
	}
	
	memcpy(pISMartFrame->ip, pInAddr, sizeof(struct in_addr));
	return ReturnSuccess;
}

int FillPort(ISmartFrame *pISMartFrame, unsigned short nPort)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	pISMartFrame->port = nPort;
	return ReturnSuccess;
}

int FillSeq(ISmartFrame *pISMartFrame, unsigned short nSeq)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	pISMartFrame->seq = nSeq;
	return ReturnSuccess;
}

int FillSourceMac(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;	
	}
	
	if (SourceMacNoInit == CheckInitSourceMac())
	{
		InitSourceMac();
		return ReturnError;
	}
	memcpy(pISMartFrame->sourceMac, g_SourceMac, 8);
	return ReturnSuccess;
}

int FillSourceMacToTarget(ISmartFrame *pISMartFrame)
{
        if (NULL == pISMartFrame)
        {
                return ReturnError;
        }
        if (SourceMacNoInit == CheckInitSourceMac())
        {
			InitSourceMac();
			return ReturnError;
        }
        memcpy(pISMartFrame->targetMac, g_SourceMac, 8);
        return ReturnSuccess;
}

int FillCRC(ISmartFrame *pISMartFrame)
{
	unsigned short nCRC = 0;	
	if (NULL == pISMartFrame)
	{
		return ReturnError;	
	}
	nCRC = getCheckSum((const unsigned char *)&pISMartFrame->frameLength, 42, SMALL_END);
	nCRC = htons(nCRC);
	memcpy(&pISMartFrame->crc, &nCRC, 2);
	return ReturnSuccess;
}

struct in_addr GetIpFromFrame(ISmartFrame *pISMartFrame)
{
	struct in_addr oRetIP;
	memcpy(&oRetIP, pISMartFrame->ip, 4);
	return oRetIP;
}

unsigned short GetPortFromFrame(ISmartFrame *pISMartFrame)
{
	unsigned short nRetPort;
	nRetPort = pISMartFrame->port;
	return nRetPort;
}


int SendFrameToCrlServer(ISmartFrame *pISMartFrame, unsigned int nLen)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	if (RemoteServerOff == CheckInitServer())
	{
		InitCtlServerAddr();
		return ReturnError;
	}
	
	return SendDataToNetByNetOrder(&g_CtlServerAddr, g_CtlServerPort, 
		(char *)pISMartFrame, nLen);
}

int SendFrameToDataServer(ISmartFrame *pISMartFrame, unsigned int nLen)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	if (RemoteServerOff == CheckInitServer())
	{
		InitCtlServerAddr();
		return ReturnError;
	}
	
	return SendDataToNetByNetOrder(&g_DataServerAddr, g_DataServerPort, 
		(char *)pISMartFrame, nLen);
}


int SendFrameToRegServer(ISmartFrame *pISMartFrame, unsigned int nLen)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	if (0 == g_RegServerAddr.s_addr)
	{
		if (ReturnError == InitRegServerAddr())
		{
			return ReturnError;
		}
	}
	return SendDataToNetByNetOrder(&g_RegServerAddr, htons(SERVER_PORT_REG), 
		(char *)pISMartFrame, nLen);
}

int SendFrameToNet(ISmartFrame *pISMartFrame, unsigned int nLen)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	if(0 == CheckExistIP(pISMartFrame))
	{
		return ReturnError;
	}
	return SendDataToNetByNetOrder((struct in_addr *)pISMartFrame->ip, pISMartFrame->port, 
		(char *)pISMartFrame, nLen);
}

int SendFrameToSpi(ISmartFrame *pISMartFrame, unsigned int nLen)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	return SendDataToSpi((char *)pISMartFrame, nLen);
}

int PackXinTiao(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;	
	}
	FillAA55(pISMartFrame);
	pISMartFrame->frameLength = htons(PACKAGE_SIZE_XINTIAO);
	pISMartFrame->funCode.dev = 0x00;
	pISMartFrame->funCode.ver = 0x00;
	pISMartFrame->funCode.fun = 0x03;
	if (SourceMacNoInit == CheckInitSourceMac())
	{
		return ReturnError;
	}
	FillSourceMac(pISMartFrame);
	FillCRC(pISMartFrame);
	return ReturnSuccess;
}

int PackInitCtrServer(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	
	FillAA55(pISMartFrame);
	pISMartFrame->frameLength = htons(PACKAGE_SIZE_INITCTLSER);
	pISMartFrame->funCode.dev = 0x00;
	pISMartFrame->funCode.ver = 0x01;
	pISMartFrame->funCode.fun = 0x01;

	if (SourceMacNoInit == CheckInitSourceMac())
	{
		return ReturnError;
	}
	FillSourceMac(pISMartFrame);

	FillCRC(pISMartFrame);
	return ReturnSuccess;
}

int PackBroadcastDiscovery(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	
	FillAA55(pISMartFrame);
	pISMartFrame->frameLength = htons(PACKAGE_SIZE_BROADDIS);
	pISMartFrame->funCode.dev = 0x00;
	pISMartFrame->funCode.ver = 0x01;
	pISMartFrame->funCode.fun = 0xfe;
	if (SourceMacNoInit == CheckInitSourceMac())
	{
		return ReturnError;
	}
	FillSourceMac(pISMartFrame);
	FillCRC(pISMartFrame);
	return ReturnSuccess;
	
}

int PackInitSourceMac(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	
	FillAA55(pISMartFrame);
	pISMartFrame->frameLength = htons(PACKAGE_SIZE_INITSRCMAC);
	pISMartFrame->funCode.dev = 0x00;
	pISMartFrame->funCode.ver = 0x01;
	pISMartFrame->funCode.fun = 0xff;
	FillCRC(pISMartFrame);
	return ReturnSuccess;
	
}

int PackGatewayNodeRet(ISmartFrame *pISMartFrame, unsigned int *pnLen)
{
	if (NULL == pISMartFrame || NULL == pnLen)
	{
		return ReturnError;
	}
	FillAA55(pISMartFrame);
	pISMartFrame->frameLength = htons(PACKAGE_SIZE_QUERYNODERET);
	pISMartFrame->dataLength = htons(PACKAGE_SIZE_QUERYNODERET - PACKAGE_SIZE_HEAD);
	*pnLen = PACKAGE_SIZE_QUERYNODERET;
	pISMartFrame->funCode.dev = PROTOCOL_TYPE_GATEWAY;
	pISMartFrame->funCode.ver = PROTOCOL_VERSION;
	pISMartFrame->funCode.fun = PROTOCOL_FUN_GATEWAY_QUERYNODERET;
	
	if (SourceMacNoInit == CheckInitSourceMac())
	{
		return ReturnError;
	}
	FillSourceMac(pISMartFrame);

	FillCRC(pISMartFrame);
	
	PNode pnode = g_pNodeList->front;
	unsigned int i = 0;
	for (; pnode && i < 80; pnode = pnode->next, i++)
	{
		/*前四个字节直接填充*/
		memcpy((void *)(pISMartFrame->data + 5 * i), (void *)(pnode->data), 4);
		/*第五个字节,需要用当前时间减去存储的时间,以获取时间差*/
		*(pISMartFrame->data + 5*i + 4) = (char)(GetUptime() - ((Item *)(pnode->data))->ntime);
	}
	
	return ReturnSuccess;
	
}

int PackQueryNode(ISmartFrame *pISMartFrame, PNode pnode)
{
	if (NULL == pISMartFrame || NULL == pnode)
	{
		return ReturnError;
	}
	Item *pitem = (Item *)(pnode->data);
	FillAA55(pISMartFrame);
	pISMartFrame->frameLength = htons(PACKAGE_SIZE_HEAD);
	pISMartFrame->funCode.dev = pitem->type;
	pISMartFrame->funCode.ver = PROTOCOL_VERSION;
	pISMartFrame->funCode.fun = PROTOCOL_FUN_QUERY;
	FillCRC(pISMartFrame);

	/*填充源mac*/
	if (SourceMacNoInit == CheckInitSourceMac())
	{
		return ReturnError;
	}
	FillSourceMac(pISMartFrame);

	/*填充目标mac*/
	pISMartFrame->targetMac[0] = GUOGEE_MAC_0;
	pISMartFrame->targetMac[1] = GUOGEE_MAC_1;
	pISMartFrame->targetMac[2] = GUOGEE_MAC_2;
	pISMartFrame->targetMac[3] = GUOGEE_MAC_3;
	pISMartFrame->targetMac[4] = GUOGEE_MAC_4;
	pISMartFrame->targetMac[5] = GUOGEE_MAC_5;
	pISMartFrame->targetMac[6] = pitem->mac[0];
	pISMartFrame->targetMac[7] = pitem->mac[1];
	
	/*填充特定IP，用于识别*/
	struct in_addr addr = {0};
	inet_aton(QUERYNODE_IP, &addr);
	FillIP(pISMartFrame, &addr);

	return ReturnSuccess;
}

int DealNetProtocol(unsigned char *pData, int nLen, struct sockaddr_in *pNetRecvBuffFrom)
{
	TRACE("DealNetProtocol\n");
	if (NULL == pData || nLen < PACKAGE_SIZE_HEAD || NULL == pNetRecvBuffFrom)
	{
		TRACEERR("%p,%d,%p\n", pData, nLen, pNetRecvBuffFrom);
		return ReturnError;
	}
	g_iSmartNetFrame = (ISmartFrame *)pData;
	g_iSmartNetFrameSize = nLen;
	g_iSmartNetFrameFrom = pNetRecvBuffFrom;
	if(ReturnError == CheckAA55(g_iSmartNetFrame))
	{
		TRACEERR("CheckAA55 error\n");
		return ReturnError;
	}
	if(ReturnError == CheckCRC(g_iSmartNetFrame))
	{
		//return ReturnError;
	}
	FillIP(g_iSmartNetFrame, &pNetRecvBuffFrom->sin_addr);
	FillPort(g_iSmartNetFrame, pNetRecvBuffFrom->sin_port);

	return MangleNetPackage();
}

int DealSpiProtocol(unsigned char *pData, int nLen)
{
	TRACE("DealSpiProtocol\n");
	if (NULL == pData || nLen < PACKAGE_SIZE_HEAD)
	{
		return ReturnError;
	}
	g_iSmartSpiFrame = (ISmartFrame *)pData;
	g_iSmartSpiFrameSize = nLen;
	if(ReturnError == CheckAA55(g_iSmartSpiFrame))
	{
		TRACEERR("CheckAA55 fail\n");
		return ReturnError;
	}
	if(ReturnError == CheckCRC(g_iSmartSpiFrame))
	{
		//return ReturnError;
	}
	
	return MangleSpiPackage();	
}

int MangleSpiPackage()
{
	TRACE("funCode(S):%02x,%02x,%02x\n", g_iSmartSpiFrame->funCode.dev, g_iSmartSpiFrame->funCode.ver, g_iSmartSpiFrame->funCode.fun);
	if (ReturnSuccess == FilterGetSourceMac(g_iSmartSpiFrame))
	{
		TRACE("Filter GetSourceMac success\n");
		return ReturnSuccess;
	}
	if (ReturnSuccess == FilterQueryNodeRet(g_iSmartSpiFrame))
	{
		TRACE("Filter NodeStatus success\n");
		return ReturnSuccess;
	}
	struct in_addr addr = {0};
	inet_aton(QUERYNODE_IP, &addr);
	if (0 == CheckExistIP(g_iSmartSpiFrame))
	{
		/*IP不存在,转发给控制服务器*/
		TRACE("frame ip not exist!!!!!\n");
		if (RemoteServerOff == CheckInitServer())
		{
			return ReturnError;
		}
		return SendFrameToCrlServer(g_iSmartNetFrame, g_iSmartNetFrameSize);
	}
	else if (0 == memcmp(g_iSmartSpiFrame->ip, (void *)&addr, sizeof (struct in_addr)))
	{
		/*如果IP等于QUERYNODE_IP:1.2.3.4,说明是网关发出的查询节点包的返回,不处理*/
		TRACE("frame ip 1.2.3.4!!!!!\n");
		return ReturnSuccess;
	}

	/*IP合法,将帧发向该IP*/
	TRACE("frame ip exist!!!!!\n");
	return SendFrameToNet(g_iSmartSpiFrame, g_iSmartSpiFrameSize);
}

int MangleNetPackage()
{ 
	TRACE("funCode(N):%02x,%02x,%02x\n", g_iSmartNetFrame->funCode.dev, g_iSmartNetFrame->funCode.ver, 
		g_iSmartNetFrame->funCode.fun);
	/*不过滤广播发现包，直接向底层转发
	if (ReturnSuccess == FilterBroadcastDiscovery(g_iSmartNetFrame))
	{
		return ReturnSuccess;
	}
	else
	*/ 
	if (ReturnSuccess == FilterCtlServerGet(g_iSmartNetFrame))
	{
		TRACE("Filter CtlServer success\n");
		return ReturnSuccess;
	}
	else if (ReturnSuccess == FilterXinTiaoRet(g_iSmartNetFrame))
	{
		TRACE("Filter XinTiao success\n");
		return ReturnSuccess;
	}
	else if (ReturnSuccess == FilterGatewayNode(g_iSmartNetFrame))
	{
		TRACE("Filter GatewayNode success\n");
		return ReturnSuccess;
	}

	return SendFrameToSpi(g_iSmartNetFrame, g_iSmartNetFrameSize);
}

int FilterBroadcastDiscovery(ISmartFrame *pISMartFrame)
{
	ISmartFrame SendFrame = {{0}};
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	
	if (0x00 != pISMartFrame->funCode.dev //|| 0x00 != pISMartFrame->funCode.ver
		|| 0xff != pISMartFrame->funCode.fun)
	{
		return ReturnError;
	}
	TRACE("BroadcastDiscovery\n");
	if (ReturnError == PackBroadcastDiscovery(&SendFrame))
	{
		return ReturnError;
	}
	FillIP(&SendFrame, &g_iSmartNetFrameFrom->sin_addr);
	FillPort(&SendFrame, g_iSmartNetFrameFrom->sin_port);
	FillSeq(&SendFrame, pISMartFrame->seq);
	SendFrameToNet(&SendFrame, PACKAGE_SIZE_BROADDIS);
	
	return ReturnSuccess;
}



int FilterCtlServerGet(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	
	if (0x00 != pISMartFrame->funCode.dev || 0x01 != pISMartFrame->funCode.ver
		|| 0x02 != pISMartFrame->funCode.fun)
	{
		return ReturnError;
	}
	TRACE("CtlServerIP:%s\n", inet_ntoa(*((struct in_addr *)pISMartFrame->data)));
	TRACE("CrlServerPort:%d\n", ntohs(*((unsigned short *)(pISMartFrame->data + 4))));
	TRACE("DataServerIP:%s\n", inet_ntoa(*((struct in_addr *)(pISMartFrame->data + 6))));
	TRACE("DataServerPort:%d\n", ntohs(*((unsigned short *)(pISMartFrame->data + 10))));
	memcpy(&g_CtlServerAddr, pISMartFrame->data, 4);
	memcpy(&g_CtlServerPort, pISMartFrame->data + 4, 2);
	memcpy(&g_DataServerAddr, pISMartFrame->data + 6, 4);
	memcpy(&g_DataServerPort, pISMartFrame->data + 10, 2);
	g_RemoteServerFlag = RemoteServerOn;
	return ReturnSuccess;

}

int FilterXinTiaoRet(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}

	if (0x00 != pISMartFrame->funCode.dev || 0x00 != pISMartFrame->funCode.ver
		|| 0x04 != pISMartFrame->funCode.fun)
	{
		return ReturnError;
	}

	/*收到心跳包，清空心跳延时计数*/
	g_nRemoteserverTime = 0;

	return ReturnSuccess;
}


int FilterGetSourceMac(ISmartFrame *pISMartFrame)
{
	
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	/*sourcemac初始化成功后不再过滤广播返回包*/
	if (SourceMacInited == g_enSourceMacFlag)
	{
		return ReturnError;
	}
	
	if (0x00 != pISMartFrame->funCode.dev || 0x01 != pISMartFrame->funCode.ver
		|| 0xfe != pISMartFrame->funCode.fun)
	{
		return ReturnError;
	}
	memcpy(&g_SourceMac, pISMartFrame->sourceMac, 8);
	g_enSourceMacFlag = SourceMacInited;
	return ReturnSuccess;
}

int FilterGatewayNode(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	if (PROTOCOL_TYPE_GATEWAY != pISMartFrame->funCode.dev
		|| PROTOCOL_FUN_GATEWAY_QUERYNODE != pISMartFrame->funCode.fun)
	{
		return ReturnError;
	}
	
	unsigned int nLen;
	ISmartFrame QueryNodeRet = {{0}};
	if (ReturnSuccess == PackGatewayNodeRet(&QueryNodeRet, &nLen))
	{
		FillIP(&QueryNodeRet, &g_iSmartNetFrameFrom->sin_addr);
		FillPort(&QueryNodeRet, g_iSmartNetFrameFrom->sin_port);
		FillSeq(&QueryNodeRet, pISMartFrame->seq);
		SendFrameToNet(&QueryNodeRet, nLen);
	}
	
	return ReturnSuccess;
}

int FilterQueryNodeRet(ISmartFrame *pISMartFrame)
{
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	/*只过滤查询返回*/
	if (PROTOCOL_FUN_QUERYRET != pISMartFrame->funCode.fun)
	{
		return ReturnError;
	}

	Item item = {{0}};
	if (PROTOCOL_TYPE_COLORLIGHT == pISMartFrame->funCode.dev)
	{
		GetItemFromColorLight(pISMartFrame, &item);
	}
	else if (PROTOCOL_TYPE_ONELIGNT == pISMartFrame->funCode.dev)
	{
		GetItemFromOneLight(pISMartFrame, &item);
	}
	else if (PROTOCOL_TYPE_TWOLIGNT == pISMartFrame->funCode.dev)
	{
		GetItemFromTwoLight(pISMartFrame, &item);
	}
	else if (PROTOCOL_TYPE_THREELIGNT == pISMartFrame->funCode.dev)
	{
		GetItemFromThreeLight(pISMartFrame, &item);
	}
	else if (PROTOCOL_TYPE_FOURLIGNT == pISMartFrame->funCode.dev)
	{
		GetItemFromFourLight(pISMartFrame, &item);
	}
	else if (PROTOCOL_TYPE_POWERSOCKET == pISMartFrame->funCode.dev)
	{
		GetItemFromPowerSocket(pISMartFrame, &item);
	}
	else if (PROTOCOL_TYPE_CONTROLSOCKET == pISMartFrame->funCode.dev)
	{
		GetItemFromControlSocket(pISMartFrame, &item);
	}
	else if (PROTOCOL_TYPE_GATEWAY == pISMartFrame->funCode.dev)
	{
		/*网关不提取节点信息*/
		return ReturnError;
	}
	else
	{
		/*设备状态获取的通用方法*/
		GetItemFromAny(pISMartFrame, &item);
	}
  
	UpdateDataToList(g_pNodeList, &item);

	/*不过滤该包,只是读取该包的信息*/
	return ReturnError;
}

int UpdateDataToList(List *plist, Item *pitem)
{
	if (NULL == plist || NULL == pitem)
	{
		return ReturnError;
	}
	PNode pnode = plist->front;
	int nNewFlag = 1;
	for (pnode = plist->front; NULL != pnode; pnode = pnode->next)
	{
		if (0 == memcmp(pnode->data, (void *)pitem, MAC_SIZE))
		{
			/*如果已经存在该mac节点,那么只是更新*/
			TRACE("UPDATE NODE **********\n");
			memcpy(pnode->data, (void *)pitem, sizeof(Item));
			nNewFlag = 0;
			break;
		}
	}
	if (nNewFlag)
	{
		/*如果不存在该mac节点,那么新增到列表*/
		TRACE("ADD NODE **********\n");
		EnListFront(plist, pitem, sizeof(Item));
	}

	return ReturnSuccess;
}

int GetItemFromColorLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*类型*/
	pitem->type = pISMartFrame->funCode.dev;
	/*状态*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	/*时间*/
	pitem->ntime = GetUptime();

	return ReturnSuccess;
}
int GetItemFromOneLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*类型*/
	pitem->type = pISMartFrame->funCode.dev;
	/*状态*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	/*时间*/
	pitem->ntime = GetUptime();

	return ReturnSuccess;
}
int GetItemFromTwoLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*类型*/
	pitem->type = pISMartFrame->funCode.dev;
	/*状态*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	if (1 == pISMartFrame->data[1])
	{
		pitem->status |= 0x02;
	}
	/*时间*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}
int GetItemFromThreeLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*类型*/
	pitem->type = pISMartFrame->funCode.dev;
	/*状态*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	if (1 == pISMartFrame->data[1])
	{
		pitem->status |= 0x02;
	}
	if (1 == pISMartFrame->data[2])
	{
		pitem->status |= 0x04;
	}
	/*时间*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}
int GetItemFromFourLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*类型*/
	pitem->type = pISMartFrame->funCode.dev;
	/*状态*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01; /*低1位*/
	}
	if (1 == pISMartFrame->data[1])
	{
		pitem->status |= 0x02; /*低2位*/
	}
	if (1 == pISMartFrame->data[2])
	{
		pitem->status |= 0x04; /*低3位*/
	}
	if (1 == pISMartFrame->data[3])
	{
		pitem->status |= 0x08; /*低4位*/
	}
	/*时间*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}
int GetItemFromPowerSocket(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*类型*/
	pitem->type = pISMartFrame->funCode.dev;
	/*状态*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	/*时间*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}
int GetItemFromControlSocket(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*类型*/
	pitem->type = pISMartFrame->funCode.dev;
	/*状态*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	/*时间*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}

int GetItemFromAny(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*类型*/
	pitem->type = pISMartFrame->funCode.dev;
	/*状态*/
	pitem->status = pISMartFrame->data[0];
	/*时间*/
	pitem->ntime = GetUptime();

	return ReturnSuccess;
}

