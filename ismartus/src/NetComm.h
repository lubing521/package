/*
*说明：通信协议tcp/ip部分
*/
#ifndef _ISMART_NET_COMM_H_
#define _ISMART_NET_COMM_H_
#include <arpa/inet.h>

#define ISMART_PORT 3000 /*通信端口*/
#define SIZE_NET_RECV_BUFF 1024 /*接收网络包的缓冲区大小*/
 
typedef int (* CALLBACK_RECV_NET_FUN)();

int StartNetService();
int StopNetService();
int SendDataToNetByCP(char *pIP, unsigned short nPort, char *pData, unsigned int nLen);
int SendDataToNetByNetOrder(struct in_addr *pIP, unsigned short nPort, char *pData, unsigned int nLen);
int SetCallBackRecvNet(CALLBACK_RECV_NET_FUN pRecvNetFun);
int SetCallBackRecvNetBuff(unsigned char *pRecvNetBuff);
int SetCallBackRecvNetBuffSize(unsigned int *pRecvNetBuffSize);
int SetCallBackRecvNetAddr(struct sockaddr_in *pRecvNetBuffFrom);
int SetNetPrint();
int SetNetNoPrint();


#endif
