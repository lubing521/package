
#ifndef _ISMART_PROTOCOL_H_
#define _ISMART_PROTOCOL_H_
#include <arpa/inet.h>
#define REG_SERVRE_DOMAIN "regsrv1.guogee.com" //115.28.165.93
//#define REG_SERVRE_DOMAIN "115.28.165.93" //115.28.165.93

#define PACKAGE_SIZE_HEAD 46
#define PACKAGE_SIZE_DATA 500
#define PACKAGE_SIZE_ALL PACKAGE_SIZE_HEAD + PACKAGE_SIZE_DATA

#define PACKAGE_HEDAH 0xAA
#define PACKAGE_HEDAL 0x55

typedef struct FunCode
{
	unsigned char dev;
	unsigned char ver;
	unsigned char fun;
}FunCode;

typedef struct ISmartFrame
{
	unsigned char heda[2];
	unsigned short frameLength;
	unsigned char sourceMac[8];				        // 8
	unsigned char targetMac[8];				        // 8
	unsigned char ip[4];							// 4	
	unsigned short port;
	unsigned char mac[6];
	unsigned short seq;
	unsigned char cipherKey;
	unsigned char reserve[4];					//4
	FunCode  funCode;
	unsigned short dataLength;
	unsigned short crc;
	unsigned char data[500];	
}__attribute__((aligned (1))) ISmartFrame;

int ProtocolInit();
int DealNetProtocol(unsigned char *pData, int nLen, struct sockaddr_in *pNetRecvBuffFrom);
int DealSerialProtocol(unsigned char *pData, int nLen);
int ClearInitSourceMac();
int ClearNodeList();








#endif
