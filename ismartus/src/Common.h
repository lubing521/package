
#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#define TRACE_DEBUG_LEVEL0   	0 //��ӡ����:����ӡ�κ���Ϣ
#define TRACE_DEBUG_LEVEL1   	1 //��ӡ����:����,����ӡ������Ϣ
#define TRACE_DEBUG_LEVEL2	   	2 //��ӡ����:����,��ӡ����;�����Ϣ
#define TRACE_DEBUG_LEVEL3		3 //��ӡ����:����,��ӡ������Ϣ

/*�Ƿ��ӡ������Ϣ��־*/
extern int printf_Enable;

/*��ӡ����������,�Ŵ�ӡ����Ϣ*/
#define TRACE(format, argument...)	\
		{ \
		if(printf_Enable >=TRACE_DEBUG_LEVEL3) \
			{\
				printf(format, ##argument ); \
			}\
		}\
									 
/*��ӡ�����Ǿ���,�Ŵ�ӡ����Ϣ*/
#define TRACEWARN(format, argument...)	\
		{ \
		if(printf_Enable >=TRACE_DEBUG_LEVEL2) \
			{\
				printf(format, ##argument ); \
			}\
		}\

/*��ӡ�����Ǵ���,�Ŵ�ӡ����Ϣ*/
#define TRACEERR(format, argument...)	\
		{ \
		if(printf_Enable >=TRACE_DEBUG_LEVEL1) \
			{\
				printf(format, ##argument ); \
			}\
		}\

/*��������ֵ*/
enum ReturnStatus
{
	ReturnError = -1,
	ReturnSuccess = 0
};

/*�Լ�ʵ�ֵĴ����ػ����̺���*/
void init_daemon(void);
/*��ȡϵͳ����ʱ��*/
long GetUptime();

int PrintDataBy16(unsigned char *pData, unsigned int nLen);

#endif

