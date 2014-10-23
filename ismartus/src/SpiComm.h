
#ifndef _ISMART_SPI_COMM_H_
#define _ISMART_SPI_COMM_H_

#define SPI_DEV "/dev/ttyUSB0"
#define SIZE_SPI_RECV_BUFF 1024
#define READ_FREQUENCY 10000 /*10����*/
#define WRITE_FREQUENCY 100000 /*100ms*/
#define READ_ERR_NUM 10 //����readʧ��10��,��Ϊ�豸�Ѿ��Ͽ�
#define ACCESS_DEV 3 //ÿ��1s̽���豸�Ƿ�����

typedef int (* CALLBACK_RECV_SPI_FUN)();

int SetCallbackRecvSpiFun(CALLBACK_RECV_SPI_FUN pRecvSpiFun);
int SetCallbackRecvSpiBuff(unsigned char *pRecvSpiBuff);
int SetCallbackRecvSpiBuffSize(unsigned int *pRecvSpiBuffSize);
int SendDataToSpi(char *pData, unsigned int nLen);
int SpiStartListen();
int SpiStopListen();
int SpiSetListenOff();
int SpiSetListenOn();
int SpiInit();
int SpiOpen();
int SpiClose();
int SetSerialPrint();
int SetSerialNoPrint();

#endif

