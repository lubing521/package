
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

/*Ĭ�ϲ���ӡ�κ���Ϣ*/
int printf_Enable = TRACE_DEBUG_LEVEL0;
int printf_Net = 0; /*����ͨ��֡,16���ƴ�ӡ*/

void init_daemon(void) 
{ 
	int pid; 
	int i;
	pid = fork();
	if(pid)
	{
		exit(0);//�Ǹ����̣����������� 
	}
	else if(pid < 0)
	{
		exit(1);//forkʧ�ܣ��˳� 
	}
	//�ǵ�һ�ӽ��̣���̨����ִ�� 
	setsid();//��һ�ӽ��̳�Ϊ�µĻỰ�鳤�ͽ����鳤
	//��������ն˷���
	pid = fork();
	if(pid)
	{
		exit(0);//�ǵ�һ�ӽ��̣�������һ�ӽ���
	}
	else if(pid < 0)
	{
		exit(1);//forkʧ�ܣ��˳�
	}
	//�ǵڶ��ӽ��̣�����
	//�ڶ��ӽ��̲����ǻỰ�鳤

	for(i = 0; i < NOFILE; ++i)//�رմ򿪵��ļ�������
	{
		//close(i);
	}
	//close(0);
	//close(1);

	chdir("/tmp");//�ı乤��Ŀ¼��/tmp
	umask(0);//�����ļ�������ģ
	return;
} 


/*��ȡϵͳ����ʱ��*/
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

