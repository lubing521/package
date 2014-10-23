
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

#define PROTOCOL_VERSION 0x01 /*Э��汾*/
#define SERVER_PORT_REG 3001 /*ע��������˿�*/
#define QUERYNODE_IP "1.2.3.4" /*��IP������ע�ڵ��ѯ�̷߳��͵Ľڵ��ѯ��*/

#define PACKAGE_SIZE_XINTIAO PACKAGE_SIZE_HEAD
#define PACKAGE_SIZE_INITCTLSER PACKAGE_SIZE_HEAD
#define PACKAGE_SIZE_BROADDIS PACKAGE_SIZE_HEAD
#define PACKAGE_SIZE_INITSRCMAC PACKAGE_SIZE_HEAD
#define PACKAGE_SIZE_QUERYNODERET PACKAGE_SIZE_HEAD+400

#define FRE_XINTIAO 3
#define TIMEOUT_XINTIAO 10
#define FRE_INIT_SOURCEMAC 3
#define FRE_INIT_REGSERVER 3
#define FRE_QUERY_NODE 1000000 //����

#define SMALL_END 1

#define PROTOCOL_FUN_QUERY 0xff /*ͨ�ò�ѯָ��,�����豸����*/
#define PROTOCOL_FUN_QUERYRET 0xfe /*ͨ�ò�ѯ����ָ��,�����豸����*/
#define PROTOCOL_FUN_GATEWAY_QUERYNODE 0x08 /*�ڵ�״̬��ѯ�����غ�������*/
#define PROTOCOL_FUN_GATEWAY_QUERYNODERET 0x09 /*�ڵ�״̬��ѯ���أ����غ�������*/

#define PROTOCOL_TYPE_GATEWAY 0x00 /*���غ���*/
#define PROTOCOL_TYPE_COLORLIGHT 0x10 /*��ɫ��*/
#define PROTOCOL_TYPE_FOURLIGNT 0x1c /*��·��*/
#define PROTOCOL_TYPE_THREELIGNT 0x1f /*��·��*/
#define PROTOCOL_TYPE_TWOLIGNT 0x1e /*��·��*/
#define PROTOCOL_TYPE_ONELIGNT 0x1d /*һ·��*/
#define PROTOCOL_TYPE_INFRARED 0x20 /*�������*/
#define PROTOCOL_TYPE_ENVIRONMENT 0x30 /*�����ɼ�*/
#define PROTOCOL_TYPE_POWERSOCKET 0x40 /*���Ĳ���*/
#define PROTOCOL_TYPE_CONTROLSOCKET 0x41 /*���Ʋ���*/
#define PROTOCOL_TYPE_VOICE 0x05 /*����*/

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

/*�豸�ڵ���Ϣ,�洢������*/
typedef struct Item
{
	unsigned char mac[MAC_SIZE];
	unsigned char type;
	unsigned char status;
	long ntime; /*������յ���ʱ��*/
}Item;


static ISmartFrame *g_iSmartNetFrame = NULL;
static int g_iSmartNetFrameSize = 0;
static struct sockaddr_in *g_iSmartNetFrameFrom = NULL;
static ISmartFrame *g_iSmartSpiFrame = NULL;
static int g_iSmartSpiFrameSize = 0;
static unsigned char g_SourceMac[8] = {0};
static int g_enSourceMacFlag = SourceMacNoInit;/*sourcemac��ʼ����־*/
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
		/*��ʼ��Դmac*/
		while (SourceMacNoInit == CheckInitSourceMac())
		{
			TRACEERR("sourcemac init now\n");
			InitSourceMac();
			sleep(FRE_INIT_SOURCEMAC);
		}
		TRACEERR("sourcemac init success\n");

		/*���������*/
		while (1)
		{
			sleep(FRE_XINTIAO);
			if (ReturnSuccess == PackXinTiao(&XinTiao))
			{
				/*����ɹ�*/
				break;
			}
		}
		/*usbת���ڶϿ�,Դmac�����,��Ҫ���³�ʼ��Դmac*/
		while(SourceMacInited == CheckInitSourceMac())
		{
			sleep(FRE_XINTIAO);
			//InitCtlServerAddr(); /*ͬʱ��ע�������ע��,��Է�����������bug*/
			SendFrameToCrlServer(&XinTiao, PACKAGE_SIZE_XINTIAO);
			/*������(TIMEOUT_XINTIAO*FRE_XINTIAO)��û���յ����أ���Ҫ������ע�����������*/
			g_nRemoteserverTime++;
			if (g_nRemoteserverTime > TIMEOUT_XINTIAO) /*�յ��������ذ�,����ռ���*/
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
	/*������ѯ�ڵ�״̬�߳�*/
	if (0 != pthread_create(&g_ptNode, NULL, pthreadNode, NULL))
	{
		return ReturnError;
	}
	/*��ʼ��sourcemac*/
	/*�豸�γ����µ��豸��sourcemac���ܲ�һ��������sourcemac�Ƕ�̬�ģ����ٿ�ʼ��ʼ��
	while (SourceMacNoInit == CheckInitSourceMac())
	{
		TRACE("sourcemac init now\n");
		InitSourceMac();
		sleep(FRE_INIT_SOURCEMAC);
	}
	TRACE("sourcemac init success\n");
	*/
	/*���������߳�*/
	if (0 != pthread_create(&g_ptXinTiao, NULL, pthreadXinTiao, NULL))
	{
		return ReturnError;
	}

	/*��ʼ��ע�������*/
	/*
	while(ReturnError == InitRegServerAddr())
	{
		sleep(FRE_INIT_REGSERVER);
	}
	*/
	/*��ʼ�����Ʒ�����*/
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

/*����ע�������,������з���������*/
int ClearRemoteServer()
{
	g_RemoteServerFlag = RemoteServerOff;
	g_RegServerAddr.s_addr = 0;
	TRACEERR("Remote Server clear success\n");
	return ReturnSuccess;
}

/*���Դmac*/
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

/*����б�*/
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
		/*ǰ�ĸ��ֽ�ֱ�����*/
		memcpy((void *)(pISMartFrame->data + 5 * i), (void *)(pnode->data), 4);
		/*������ֽ�,��Ҫ�õ�ǰʱ���ȥ�洢��ʱ��,�Ի�ȡʱ���*/
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

	/*���Դmac*/
	if (SourceMacNoInit == CheckInitSourceMac())
	{
		return ReturnError;
	}
	FillSourceMac(pISMartFrame);

	/*���Ŀ��mac*/
	pISMartFrame->targetMac[0] = GUOGEE_MAC_0;
	pISMartFrame->targetMac[1] = GUOGEE_MAC_1;
	pISMartFrame->targetMac[2] = GUOGEE_MAC_2;
	pISMartFrame->targetMac[3] = GUOGEE_MAC_3;
	pISMartFrame->targetMac[4] = GUOGEE_MAC_4;
	pISMartFrame->targetMac[5] = GUOGEE_MAC_5;
	pISMartFrame->targetMac[6] = pitem->mac[0];
	pISMartFrame->targetMac[7] = pitem->mac[1];
	
	/*����ض�IP������ʶ��*/
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
		/*IP������,ת�������Ʒ�����*/
		TRACE("frame ip not exist!!!!!\n");
		if (RemoteServerOff == CheckInitServer())
		{
			return ReturnError;
		}
		return SendFrameToCrlServer(g_iSmartNetFrame, g_iSmartNetFrameSize);
	}
	else if (0 == memcmp(g_iSmartSpiFrame->ip, (void *)&addr, sizeof (struct in_addr)))
	{
		/*���IP����QUERYNODE_IP:1.2.3.4,˵�������ط����Ĳ�ѯ�ڵ���ķ���,������*/
		TRACE("frame ip 1.2.3.4!!!!!\n");
		return ReturnSuccess;
	}

	/*IP�Ϸ�,��֡�����IP*/
	TRACE("frame ip exist!!!!!\n");
	return SendFrameToNet(g_iSmartSpiFrame, g_iSmartSpiFrameSize);
}

int MangleNetPackage()
{ 
	TRACE("funCode(N):%02x,%02x,%02x\n", g_iSmartNetFrame->funCode.dev, g_iSmartNetFrame->funCode.ver, 
		g_iSmartNetFrame->funCode.fun);
	/*�����˹㲥���ְ���ֱ����ײ�ת��
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

	/*�յ������������������ʱ����*/
	g_nRemoteserverTime = 0;

	return ReturnSuccess;
}


int FilterGetSourceMac(ISmartFrame *pISMartFrame)
{
	
	if (NULL == pISMartFrame)
	{
		return ReturnError;
	}
	/*sourcemac��ʼ���ɹ����ٹ��˹㲥���ذ�*/
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
	/*ֻ���˲�ѯ����*/
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
		/*���ز���ȡ�ڵ���Ϣ*/
		return ReturnError;
	}
	else
	{
		/*�豸״̬��ȡ��ͨ�÷���*/
		GetItemFromAny(pISMartFrame, &item);
	}
  
	UpdateDataToList(g_pNodeList, &item);

	/*�����˸ð�,ֻ�Ƕ�ȡ�ð�����Ϣ*/
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
			/*����Ѿ����ڸ�mac�ڵ�,��ôֻ�Ǹ���*/
			TRACE("UPDATE NODE **********\n");
			memcpy(pnode->data, (void *)pitem, sizeof(Item));
			nNewFlag = 0;
			break;
		}
	}
	if (nNewFlag)
	{
		/*��������ڸ�mac�ڵ�,��ô�������б�*/
		TRACE("ADD NODE **********\n");
		EnListFront(plist, pitem, sizeof(Item));
	}

	return ReturnSuccess;
}

int GetItemFromColorLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*����*/
	pitem->type = pISMartFrame->funCode.dev;
	/*״̬*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	/*ʱ��*/
	pitem->ntime = GetUptime();

	return ReturnSuccess;
}
int GetItemFromOneLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*����*/
	pitem->type = pISMartFrame->funCode.dev;
	/*״̬*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	/*ʱ��*/
	pitem->ntime = GetUptime();

	return ReturnSuccess;
}
int GetItemFromTwoLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*����*/
	pitem->type = pISMartFrame->funCode.dev;
	/*״̬*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	if (1 == pISMartFrame->data[1])
	{
		pitem->status |= 0x02;
	}
	/*ʱ��*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}
int GetItemFromThreeLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*����*/
	pitem->type = pISMartFrame->funCode.dev;
	/*״̬*/
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
	/*ʱ��*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}
int GetItemFromFourLight(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*����*/
	pitem->type = pISMartFrame->funCode.dev;
	/*״̬*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01; /*��1λ*/
	}
	if (1 == pISMartFrame->data[1])
	{
		pitem->status |= 0x02; /*��2λ*/
	}
	if (1 == pISMartFrame->data[2])
	{
		pitem->status |= 0x04; /*��3λ*/
	}
	if (1 == pISMartFrame->data[3])
	{
		pitem->status |= 0x08; /*��4λ*/
	}
	/*ʱ��*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}
int GetItemFromPowerSocket(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*����*/
	pitem->type = pISMartFrame->funCode.dev;
	/*״̬*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	/*ʱ��*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}
int GetItemFromControlSocket(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*����*/
	pitem->type = pISMartFrame->funCode.dev;
	/*״̬*/
	if (1 == pISMartFrame->data[0])
	{
		pitem->status |= 0x01;
	}
	/*ʱ��*/
	pitem->ntime = GetUptime();
	return ReturnSuccess;
}

int GetItemFromAny(ISmartFrame *pISMartFrame, Item *pitem)
{
	/*mac*/
	memcpy((void*)pitem->mac, &pISMartFrame->sourceMac[6], MAC_SIZE);
	/*����*/
	pitem->type = pISMartFrame->funCode.dev;
	/*״̬*/
	pitem->status = pISMartFrame->data[0];
	/*ʱ��*/
	pitem->ntime = GetUptime();

	return ReturnSuccess;
}

