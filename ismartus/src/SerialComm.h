
#ifndef _ISMART_SERIAL_COMM_H_
#define _ISMART_SERIAL_COMM_H_

#define SERIAL_DEV "/dev/ttyUSB0"
#define SIZE_SERIAL_RECV_BUFF 1024
#define READ_FREQUENCY 10000 /*10毫秒*/
#define WRITE_FREQUENCY 100000 /*100ms*/
#define READ_ERR_NUM 10 //连续read失败10次,认为设备已经断开
#define ACCESS_DEV 3 //每个1s探测设备是否连接

typedef int (* CALLBACK_RECV_SERIAL_FUN)();

int SetCallbackRecvSerialFun(CALLBACK_RECV_SERIAL_FUN pRecvSerialFun);
int SetCallbackRecvSerialBuff(unsigned char *pRecvSerialBuff);
int SetCallbackRecvSerialBuffSize(unsigned int *pRecvSerialBuffSize);
int SendDataToSerial(char *pData, unsigned int nLen);
int SerialStartListen();
int SerialStopListen();
int SerialSetListenOff();
int SerialSetListenOn();
int SerialInit();
int SerialOpen();
int SerialClose();
int SetSerialPrint();
int SetSerialNoPrint();

#endif

