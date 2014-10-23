
#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#define TRACE_DEBUG_LEVEL0   	0 //打印级别:不打印任何信息
#define TRACE_DEBUG_LEVEL1   	1 //打印级别:错误,仅打印错误信息
#define TRACE_DEBUG_LEVEL2	   	2 //打印级别:警告,打印错误和警告信息
#define TRACE_DEBUG_LEVEL3		3 //打印级别:所有,打印所有信息

/*是否打印调试信息标志*/
extern int printf_Enable;

/*打印级别是所有,才打印的信息*/
#define TRACE(format, argument...)	\
		{ \
		if(printf_Enable >=TRACE_DEBUG_LEVEL3) \
			{\
				printf(format, ##argument ); \
			}\
		}\
									 
/*打印级别是警告,才打印的信息*/
#define TRACEWARN(format, argument...)	\
		{ \
		if(printf_Enable >=TRACE_DEBUG_LEVEL2) \
			{\
				printf(format, ##argument ); \
			}\
		}\

/*打印级别是错误,才打印的信息*/
#define TRACEERR(format, argument...)	\
		{ \
		if(printf_Enable >=TRACE_DEBUG_LEVEL1) \
			{\
				printf(format, ##argument ); \
			}\
		}\

/*函数返回值*/
enum ReturnStatus
{
	ReturnError = -1,
	ReturnSuccess = 0
};

/*自己实现的创建守护进程函数*/
void init_daemon(void);
/*获取系统开机时间*/
long GetUptime();

int PrintDataBy16(unsigned char *pData, unsigned int nLen);

#endif

