
#include <unistd.h> 
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/param.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <linux/limits.h>
#include <sys/sysinfo.h>
#include "Common.h"

/*默认不打印任何信息*/
int printf_Enable = TRACE_DEBUG_LEVEL0;
int printf_Net = 0; /*网络通信帧,16进制打印*/

void init_daemon(void) 
{ 
	int pid; 
	int i;
	pid = fork();
	if(pid)
	{
		exit(0);//是父进程，结束父进程 
	}
	else if(pid < 0)
	{
		exit(1);//fork失败，退出 
	}
	//是第一子进程，后台继续执行 
	setsid();//第一子进程成为新的会话组长和进程组长
	//并与控制终端分离
	pid = fork();
	if(pid)
	{
		exit(0);//是第一子进程，结束第一子进程
	}
	else if(pid < 0)
	{
		exit(1);//fork失败，退出
	}
	//是第二子进程，继续
	//第二子进程不再是会话组长

	for(i = 0; i < NOFILE; ++i)//关闭打开的文件描述符
	{
		//close(i);
	}
	//close(0);
	//close(1);

	chdir("/tmp");//改变工作目录到/tmp
	umask(0);//重设文件创建掩模
	return;
} 


/*获取系统开机时间*/
long GetUptime()
{
	struct sysinfo info;
	if (sysinfo(&info))
	{
		return ReturnError;
	}
	return info.uptime;
}

int PrintDataBy16(unsigned char *pData, unsigned int nLen)
{
	if (NULL == pData)
	{
		return ReturnError;
	}
	unsigned int i = 0;
	for (i = 0; i < nLen; i++)
	{
		TRACE("%02x ", pData[i]);
	}
	TRACE("\n");

	return ReturnSuccess;
}

