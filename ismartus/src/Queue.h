
#ifndef Queue_H
#define Queue_H

/*通用数据结构链表,邓俊勇*/

#include <unistd.h>
#include <pthread.h>

typedef struct queuenode * PQueueNode;
typedef struct queuenode
{
	void *data;
	unsigned int nlen;
	PQueueNode next;
}QueueNode;

typedef struct
{
	PQueueNode front;
	PQueueNode rear;
	int size;
	pthread_mutex_t mutex;
}Queue;

/*构造一个空队列*/
Queue *InitQueue();

/*销毁一个队列*/
void DestroyQueue(Queue *pqueue);

/*清空一个队列*/
void ClearQueue(Queue *pqueue);

/*判断队列是否为空*/
int IsEmpty(Queue *pqueue);

/*返回队列大小*/
int GetSize(Queue *pqueue);

/*返回队头元素*/
PQueueNode GetFront(Queue *pqueue);

/*返回队尾元素*/
PQueueNode GetRear(Queue *pqueue);

/*将新元素入队*/
PQueueNode EnQueue(Queue *pqueue, void *pData, unsigned int nLen);

/*队头元素出队*/
PQueueNode DeQueue(Queue *pqueue);

/*遍历队列并对各数据项调用visit函数*/
void QueueTraverse(Queue *pqueue,void (*visit)());

#endif

