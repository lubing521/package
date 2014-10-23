
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include "Common.h"
#include "Queue.h"
#include "SpiComm.h"
#include "Protocol.h"


static int g_hSpiDevFd = 0;
static CALLBACK_RECV_SPI_FUN g_pRecvSpiFun = NULL;
static pthread_t g_ptListen = 0;
static unsigned char *g_pRecvSpiBuff = NULL;
static unsigned int *g_pRecvSpiBuffSize = NULL;
static pthread_mutex_t g_hRdWrMutex = PTHREAD_MUTEX_INITIALIZER;
static int g_PrintSirial = 0; /*串口通信帧,16进制打印*/
static Queue *g_WriteQueue = NULL;
static int g_nReadErr = 0;
static pthread_t g_ptWriteSerial = 0;
	//struct timeval tvs2;
	//struct timeval tve2;


int ReadChar(unsigned char *pChar)
{
	
	if (g_hSpiDevFd <= 0)
	{
		TRACEERR("dev not open\n");
		return ReturnError;
	}
	//TRACE("ReadChar before\n");
	//pthread_mutex_lock(&g_hRdWrMutex);
	//tcflush(g_hSpiDevFd, TCIFLUSH);
		//gettimeofday(&tvs2, NULL);
	/*read 只有一个线程执行，不需要锁*/
	if (0 >= read(g_hSpiDevFd, pChar, 1))
	{
		g_nReadErr++;
		TRACEERR("read return err\n");
		if (READ_ERR_NUM <= g_nReadErr)
		{
			TRACEERR("us unlink\n");
			SpiClose();
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
		//memset(g_pRecvSpiBuff, 0, SIZE_SPI_RECV_BUFF);
		//TRACE("start read\n");
		if (g_hSpiDevFd <= 0)
		{
			TRACEERR("spi dev not open\n");
			if(access(SPI_DEV, R_OK | W_OK)==0)
			{
				if (0 >= SpiOpen())
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
			g_pRecvSpiBuff[0] = 0;
			ReadChar(g_pRecvSpiBuff + nWrite);
			//TRACE("g_pRecvSpiBuff[0]:%u\n", (unsigned int)g_pRecvSpiBuff[0]);
			//TRACE("%u\n", PACKAGE_HEDAH);
			if (PACKAGE_HEDAH == g_pRecvSpiBuff[0])
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
			ReadChar(g_pRecvSpiBuff + nWrite);
			if (0x55 == g_pRecvSpiBuff[1])
			{
				nWrite = 2;
			}
			else if (0xaa == g_pRecvSpiBuff[1])
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
		ReadChar(g_pRecvSpiBuff + nWrite);
		nWrite++;
		ReadChar(g_pRecvSpiBuff + nWrite);
		unsigned int nLen = ntohs(*(uint16_t *)(g_pRecvSpiBuff + 2));
		//TRACE("nLen:%d\n", nLen);
		if (nLen >= sizeof(ISmartFrame))
		{
			nWrite = 0;
			continue;
		}

		unsigned int i = 0;
		for (i = 0; i < nLen - 4; i++)
		{
			ReadChar(g_pRecvSpiBuff + 4 + i);
		}

		//ReadChars(g_pRecvSpiBuff, nLen - 4);
		gettimeofday(&tve, NULL);
		TRACE("Read us zhen %u: %ld####\n", nNum, (tve.tv_sec - tvs.tv_sec)*1000000 + tve.tv_usec - tvs.tv_usec);
		nNum++;
		nWrite = 0;
		*g_pRecvSpiBuffSize = nLen;
		if (g_PrintSirial)
		{
			TRACE("read serial####\n");
			PrintDataBy16(g_pRecvSpiBuff, nLen);
		}
		gettimeofday(&tvs, NULL);
		(*g_pRecvSpiFun)();
		gettimeofday(&tve, NULL);
		TRACE("Deal us zhen: %ld####\n", (tve.tv_sec - tvs.tv_sec)*1000000 + tve.tv_usec - tvs.tv_usec);



#if 0
		//pthread_mutex_lock(&g_hRdWrMutex);
		int ret = read(g_hSpiDevFd, g_pRecvSpiBuff, 1);
		//pthread_mutex_unlock(&g_hRdWrMutex);
		if (ret >= PACKAGE_SIZE_HEAD)
		{
			TRACE("spi read frame:%d!\n", ret);
			*g_pRecvSpiBuffSize = ret;
			/*int i = 0;
			for (i = 0; i < ret; i++)
			{
				TRACE("%02X", (unsigned char)g_pRecvSpiBuff[i]);
			}*/
			(*g_pRecvSpiFun)();
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
			if (g_hSpiDevFd > 0)
			{
				TRACE("write serial####%d\n", pnode->nlen);
				if (g_PrintSirial)
				{
					PrintDataBy16((unsigned char *)pnode->data, pnode->nlen);
				}
				write(g_hSpiDevFd, pnode->data, pnode->nlen);
				tcflush(g_hSpiDevFd, TCOFLUSH);
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

int SetCallbackRecvSpiFun(CALLBACK_RECV_SPI_FUN pRecvSpiFun)
{
	if (NULL == pRecvSpiFun)
	{
		return ReturnError;
	}
	g_pRecvSpiFun = pRecvSpiFun;
	return ReturnSuccess;
}

int SetCallbackRecvSpiBuff(unsigned char *pRecvSpiBuff)
{
	if (NULL == pRecvSpiBuff)
	{
		return ReturnError;
	}
	g_pRecvSpiBuff = pRecvSpiBuff;
	return ReturnSuccess;
}

int SetCallbackRecvSpiBuffSize(unsigned int *pRecvSpiBuffSize)
{
	if (NULL == pRecvSpiBuffSize)
	{
		return ReturnError;
	}
	g_pRecvSpiBuffSize = pRecvSpiBuffSize;
	return ReturnSuccess;
	
}


int SendDataToSpi(char *pData, unsigned int nLen)
{
	if (NULL == pData)
	{
		return ReturnError;
	}
	if (g_hSpiDevFd <= 0)
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

int SpiStartListen()
{
	if (NULL == g_pRecvSpiFun || NULL == g_pRecvSpiBuff /*|| g_hSpiDevFd <= 0*/)
	{
		return ReturnError;
	}
	if (0 != pthread_create(&g_ptListen, NULL, pthreadListen, NULL))
	{
		return ReturnError;
	}
	TRACE("spi listen pthread create\n");
	return ReturnSuccess;
	
}

int SpiInit()
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

int SpiOpen() 
{
	if (g_hSpiDevFd <= 0)
	{
		g_hSpiDevFd = open(SPI_DEV, O_RDWR | O_NOCTTY | O_NDELAY);
		if (g_hSpiDevFd <= 0)
		{
			//perror("open");
			return ReturnError;
		}
	}
	struct termios options;
	tcgetattr(g_hSpiDevFd, &options);
    fcntl(g_hSpiDevFd, F_SETFL, 0); /*阻塞*/
    //options.c_cflag |= (CLOCAL | CREAD);
    /*fcntl(g_hSpiDevFd, F_SETFL, FNDELAY);*/ /*非阻塞*/
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

    tcsetattr(g_hSpiDevFd, TCSANOW, &options);
	return g_hSpiDevFd;
}

int SpiClose()
{
	if (g_hSpiDevFd <= 0)
	{
		return ReturnError;
	}
	close(g_hSpiDevFd);
	g_hSpiDevFd = 0;
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

