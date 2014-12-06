
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include "Common.h"
#include "Queue.h"
#include "SerialComm.h"
#include "Protocol.h"


static int g_hSerialDevFd = 0;
static CALLBACK_RECV_SERIAL_FUN g_pRecvSerialFun = NULL;
static pthread_t g_ptListen = 0;
static unsigned char *g_pRecvSerialBuff = NULL;
static unsigned int *g_pRecvSerialBuffSize = NULL;
static pthread_mutex_t g_hRdWrMutex = PTHREAD_MUTEX_INITIALIZER;
static int g_PrintSirial = 0; /*串口通信帧,16进制打印*/
static Queue *g_WriteQueue = NULL;
static int g_nReadErr = 0;
static pthread_t g_ptWriteSerial = 0;
	//struct timeval tvs2;
	//struct timeval tve2;


int ReadChar(unsigned char *pChar)
{
	
	if (g_hSerialDevFd <= 0)
	{
		TRACEERR("dev not open\n");
		return ReturnError;
	}
	//TRACE("ReadChar before\n");
	//pthread_mutex_lock(&g_hRdWrMutex);
	//tcflush(g_hSerialDevFd, TCIFLUSH);
		//gettimeofday(&tvs2, NULL);
	/*read 只有一个线程执行，不需要锁*/
	if (0 >= read(g_hSerialDevFd, pChar, 1))
	{
		g_nReadErr++;
		TRACEERR("read return err\n");
		if (READ_ERR_NUM <= g_nReadErr)
		{
			TRACEERR("us unlink\n");
			SerialClose();
			ClearInitSourceMac(); /*清除网关mac和远程服务器信息,换网关需要重新注册才可用*/
			//ClearNodeList(); /*不清除节点信息*/
		}
		return ReturnError;
	}
	g_nReadErr = 0;
		//gettimeofday(&tve2, NULL);
		//TRACE("Read char : %u\n", (tve2.tv_sec - tvs2.tv_sec)*1000000 + tve2.tv_usec - tvs2.tv_usec);
	//pthread_mutex_unlock(&g_hRdWrMutex);
	//TRACE("ReadChar:%02x\n", *pChar);
	return ReturnSuccess;
}

static void* pthreadListen (void* arg)
{
	pthread_detach(pthread_self());
	unsigned int nWrite = 0;
	struct timeval tvs;
	struct timeval tve;
	unsigned int nNum = 0;
	while(1)
	{
		//memset(g_pRecvSerialBuff, 0, SIZE_serial_RECV_BUFF);
		//TRACE("start read\n");
		if (g_hSerialDevFd <= 0)
		{
			TRACEERR("serial dev not open\n");
			if(access(SERIAL_DEV, R_OK | W_OK)==0)
			{
				if (0 >= SerialOpen())
				{
					sleep(ACCESS_DEV);
					continue;
				}
				TRACEERR("US open success\n");
			}
			else
			{
				sleep(ACCESS_DEV);
				continue;
			}
		}
		if (0 == nWrite)
		{
			//TRACE("all new\n");
			g_pRecvSerialBuff[0] = 0;
			ReadChar(g_pRecvSerialBuff + nWrite);
			//TRACE("g_pRecvSerialBuff[0]:%u\n", (unsigned int)g_pRecvSerialBuff[0]);
			//TRACE("%u\n", PACKAGE_HEDAH);
			if (PACKAGE_HEDAH == g_pRecvSerialBuff[0])
			{
				gettimeofday(&tvs, NULL);
				nWrite = 1;
			}
			else
			{
				TRACEERR("no aa\n");
				nWrite = 0;
				continue;
			}
		}
		if (1 == nWrite)
		{
			ReadChar(g_pRecvSerialBuff + nWrite);
			if (0x55 == g_pRecvSerialBuff[1])
			{
				nWrite = 2;
			}
			else if (0xaa == g_pRecvSerialBuff[1])
			{
				nWrite = 1;
				continue;
			}
			else
			{
				nWrite = 0;
				continue;
			}
		}
		ReadChar(g_pRecvSerialBuff + nWrite);
		nWrite++;
		ReadChar(g_pRecvSerialBuff + nWrite);
		unsigned int nLen = ntohs(*(uint16_t *)(g_pRecvSerialBuff + 2));
		//TRACE("nLen:%d\n", nLen);
		if (nLen >= sizeof(ISmartFrame))
		{
			nWrite = 0;
			continue;
		}

		unsigned int i = 0;
		for (i = 0; i < nLen - 4; i++)
		{
			ReadChar(g_pRecvSerialBuff + 4 + i);
		}

		//ReadChars(g_pRecvSerialBuff, nLen - 4);
		gettimeofday(&tve, NULL);
		TRACE("Read us zhen %u: %ld####\n", nNum, (tve.tv_sec - tvs.tv_sec)*1000000 + tve.tv_usec - tvs.tv_usec);
		nNum++;
		nWrite = 0;
		*g_pRecvSerialBuffSize = nLen;
		if (g_PrintSirial)
		{
			TRACE("read serial####\n");
			PrintDataBy16(g_pRecvSerialBuff, nLen);
		}
		gettimeofday(&tvs, NULL);
		(*g_pRecvSerialFun)();
		gettimeofday(&tve, NULL);
		TRACE("Deal us zhen: %ld####\n", (tve.tv_sec - tvs.tv_sec)*1000000 + tve.tv_usec - tvs.tv_usec);



#if 0
		//pthread_mutex_lock(&g_hRdWrMutex);
		int ret = read(g_hSerialDevFd, g_pRecvSerialBuff, 1);
		//pthread_mutex_unlock(&g_hRdWrMutex);
		if (ret >= PACKAGE_SIZE_HEAD)
		{
			TRACE("serial read frame:%d!\n", ret);
			*g_pRecvSerialBuffSize = ret;
			/*int i = 0;
			for (i = 0; i < ret; i++)
			{
				TRACE("%02X", (unsigned char)g_pRecvSerialBuff[i]);
			}*/
			(*g_pRecvSerialFun)();
		}
#endif
		//usleep(READ_FREQUENCY);

	}
	return NULL;
}

static void* pthreadWriteSerial (void* arg)
{
	PQueueNode pnode = {0};
	while (1)
	{
		pnode = GetFront(g_WriteQueue);
		if (pnode)
		{
			if (g_hSerialDevFd > 0)
			{
				TRACE("write serial####%d\n", pnode->nlen);
				if (g_PrintSirial)
				{
					PrintDataBy16((unsigned char *)pnode->data, pnode->nlen);
				}
				write(g_hSerialDevFd, pnode->data, pnode->nlen);
                //ar9331 /dev/ttyATH0不能使用该函数,否则数据发送不完整
				//tcflush(g_hSerialDevFd, TCOFLUSH);
				/*出队列,入队列,不能同时执行*/
				pthread_mutex_lock(&g_hRdWrMutex);
				DeQueue(g_WriteQueue);
				pthread_mutex_unlock(&g_hRdWrMutex);
				TRACE("DeQueue !!!:%d\n", GetSize(g_WriteQueue));
			}
		}
		/*30ms内不允许再次write,cc2530处理能力有限*/
		usleep(50000);
	}
	return NULL;
}

int SetCallbackRecvSerialFun(CALLBACK_RECV_SERIAL_FUN pRecvSerialFun)
{
	if (NULL == pRecvSerialFun)
	{
		return ReturnError;
	}
	g_pRecvSerialFun = pRecvSerialFun;
	return ReturnSuccess;
}

int SetCallbackRecvSerialBuff(unsigned char *pRecvSerialBuff)
{
	if (NULL == pRecvSerialBuff)
	{
		return ReturnError;
	}
	g_pRecvSerialBuff = pRecvSerialBuff;
	return ReturnSuccess;
}

int SetCallbackRecvSerialBuffSize(unsigned int *pRecvSerialBuffSize)
{
	if (NULL == pRecvSerialBuffSize)
	{
		return ReturnError;
	}
	g_pRecvSerialBuffSize = pRecvSerialBuffSize;
	return ReturnSuccess;
	
}


int SendDataToSerial(char *pData, unsigned int nLen)
{
	if (NULL == pData)
	{
		return ReturnError;
	}
	if (g_hSerialDevFd <= 0)
	{
		/*设备移除,不允许加入队列*/
		return ReturnError;
	}

	/*多个线程调用,需要锁*/
	pthread_mutex_lock(&g_hRdWrMutex);
	/*加入队列尾部*/
	EnQueue(g_WriteQueue, pData, nLen);
	pthread_mutex_unlock(&g_hRdWrMutex);
	TRACE("EnQueue !!!:%d\n", GetSize(g_WriteQueue));
	//usleep(WRITE_FREQUENCY);
	return ReturnSuccess;
}

int SerialStartListen()
{
	if (NULL == g_pRecvSerialFun || NULL == g_pRecvSerialBuff /*|| g_hSerialDevFd <= 0*/)
	{
		return ReturnError;
	}
	if (0 != pthread_create(&g_ptListen, NULL, pthreadListen, NULL))
	{
		return ReturnError;
	}
	TRACE("serial listen pthread create\n");
	return ReturnSuccess;
	
}

int SerialInit()
{
	g_WriteQueue = InitQueue();
	pthread_mutex_init(&g_hRdWrMutex,NULL);

	if (0 != pthread_create(&g_ptWriteSerial, NULL, pthreadWriteSerial, NULL))
	{
		perror("pthread_create");
		return ReturnError;
	}
	return ReturnSuccess;
}

int SerialOpen() 
{
	if (g_hSerialDevFd <= 0)
	{
		g_hSerialDevFd = open(SERIAL_DEV, O_RDWR | O_NOCTTY | O_NDELAY);
		if (g_hSerialDevFd <= 0)
		{
			//perror("open");
			return ReturnError;
		}
	}
	struct termios options;
	tcgetattr(g_hSerialDevFd, &options);
    fcntl(g_hSerialDevFd, F_SETFL, 0); /*阻塞*/
    //options.c_cflag |= (CLOCAL | CREAD);
    /*fcntl(g_hSerialDevFd, F_SETFL, FNDELAY);*/ /*非阻塞*/
    /*baud tates*/
    //cfsetispeed(&options,B38400);
    //cfsetospeed(&options,B38400);
    //options.c_cflag &= ~PARENB; /*无奇偶校验*/
    //options.c_cflag &= ~CSTOPB; /*停止位1 */
    //options.c_cflag &= ~CSIZE;
    //options.c_cflag |= CS8;
	options.c_cflag = B38400 | CS8 | CREAD | CLOCAL;
    /*newtio.c_cc[VTIME] =0;*/
    /*newtio.c_cc[VMIN]=0;*/

    //options.c_lflag &=  ~(ICANON | ECHO | ECHOE); /*原始输入*/
	options.c_lflag = 0;
    //options.c_oflag &= ~OPOST; /*原始输出方式*/
	options.c_oflag = 0;
	options.c_iflag = IGNPAR; /*无奇偶校验*/

    tcsetattr(g_hSerialDevFd, TCSANOW, &options);
	return g_hSerialDevFd;
}

int SerialClose()
{
	if (g_hSerialDevFd <= 0)
	{
		return ReturnError;
	}
	close(g_hSerialDevFd);
	g_hSerialDevFd = 0;
	return ReturnSuccess;
}

 
int SetSerialPrint()
{
	g_PrintSirial = 1;
	return ReturnSuccess;
}
int SetSerialNoPrint()
{
	g_PrintSirial = 0;
	return ReturnSuccess;
}

