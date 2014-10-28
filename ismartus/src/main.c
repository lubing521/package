#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "Common.h"
#include "SerialComm.h"
#include "NetComm.h"
#include "Protocol.h"

static unsigned char g_szRecvSerialBuff[SIZE_SERIAL_RECV_BUFF] = {0};
static unsigned int g_nRecvSerialBuffSize = 0;

static unsigned char g_szRecvNetBuff[SIZE_NET_RECV_BUFF] = {0};
static unsigned int g_nRecvNetBuffSize = 0;
static struct sockaddr_in g_oRecvNetBuffFrom = {0,0,{0},{0}};

int CallBackRecvSerialFun()
{
	return DealSerialProtocol(g_szRecvSerialBuff, g_nRecvSerialBuffSize);
}

int CallBackRecvNetFun()
{
	return DealNetProtocol(g_szRecvNetBuff, g_nRecvNetBuffSize, &g_oRecvNetBuffFrom);
}


int main(int argc, char *argv[])
{
	//根据参数，创建守护进程
	int nDaemon = 0;
	int opt;
	while((opt=getopt(argc,argv, "dnsv:")) != -1)
	{
		switch(opt)
		{
			case 'd':
			{
				nDaemon = 1;
				break;
			}
			case 'v':
			{
				printf_Enable = *optarg - '0';
				break;
			}
			case 's':
			{
				SetSerialPrint();
				break;
			}
			case 'n':
			{
				SetNetPrint();
				break;
			}
			default:
			{
				break;
			}
			
		}
	}
	if (nDaemon)
	{
		//daemon(0, 0);
		init_daemon();
	}

	//serial相关初始化
	//设备尚未连接的时候打不开,需要实时检测设备是否连接.
	/*
	if(0 >= SerialOpen())
	{
		TRACEERR("serial dev open fail\n");
		exit(ReturnError);
	}
	*/
	SerialInit();
	SetCallbackRecvSerialFun(CallBackRecvSerialFun);
	SetCallbackRecvSerialBuff(g_szRecvSerialBuff);
	SetCallbackRecvSerialBuffSize(&g_nRecvSerialBuffSize);
	if (ReturnError == SerialStartListen())
	{
		TRACEERR("serial start listen fail\n");
		exit(ReturnError);
	}

	//网络相关初始化
	SetCallBackRecvNet(CallBackRecvNetFun);	
	SetCallBackRecvNetBuff(g_szRecvNetBuff);
	SetCallBackRecvNetBuffSize(&g_nRecvNetBuffSize);
	SetCallBackRecvNetAddr(&g_oRecvNetBuffFrom);
	if (ReturnError == StartNetService())
	{
		TRACEERR("start net service fail\n");
		exit(ReturnError);
	}

	//协议相关初始化
	if (ReturnError == ProtocolInit())
	{
		TRACEERR("Protocol init fail\n");
		exit(ReturnError);
	}

	while(1)
	{
		sleep(3);
	}

	return ReturnSuccess;
}
