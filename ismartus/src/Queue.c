
#include <malloc.h>
#include <string.h>
#include "Queue.h"

/*通用数据结构队列,邓俊勇*/

/*构造一个空队列*/
Queue *InitQueue()
{
	Queue *pqueue = (Queue *)malloc(sizeof(Queue));
	if(pqueue!=NULL)
	{
		pqueue->front = NULL;
		pqueue->rear = NULL;
		pqueue->size = 0;
		pthread_mutex_init(&pqueue->mutex, NULL);
	}
	return pqueue;
}

/*销毁一个队列*/
void DestroyQueue(Queue *pqueue)
{
	if(IsEmpty(pqueue)!=1)
		ClearQueue(pqueue);
	free(pqueue);
}

/*清空一个队列*/
void ClearQueue(Queue *pqueue)
{
	while(IsEmpty(pqueue)!=1)
	{
		DeQueue(pqueue);
	}

}

/*判断队列是否为空*/
int IsEmpty(Queue *pqueue)
{
	if(pqueue->front==NULL&&pqueue->rear==NULL&&pqueue->size==0)
		return 1;
	else
		return 0;
}

/*返回队列大小*/
int GetSize(Queue *pqueue)
{
	return pqueue->size;
}

/*返回队头元素*/
PQueueNode GetFront(Queue *pqueue)
{
	if (pqueue)
	{
		return pqueue->front;
	}
	return NULL;
}

/*返回队尾元素*/

PQueueNode GetRear(Queue *pqueue)
{
	if (pqueue)
	{
		return pqueue->rear;
	}
	return NULL;
}

/*将新元素入队*/
PQueueNode EnQueue(Queue *pqueue, void *pData, unsigned int nLen)
{
	pthread_mutex_lock(&pqueue->mutex);
	PQueueNode pnode = (PQueueNode)malloc(sizeof(QueueNode));
	if(pnode != NULL)
	{
		pnode->data = malloc(nLen);
		memcpy(pnode->data, pData, nLen);
		pnode->nlen = nLen;
		pnode->next = NULL;
		
		if(IsEmpty(pqueue))
		{
			pqueue->front = pnode;
		}
		else
		{
			pqueue->rear->next = pnode;
		}
		pqueue->rear = pnode;
		pqueue->size++;
	}
	pthread_mutex_unlock(&pqueue->mutex);
	return pnode;
}

/*队头元素出队*/
PQueueNode DeQueue(Queue *pqueue)
{
	pthread_mutex_lock(&pqueue->mutex);
	PQueueNode pnode = pqueue->front;
	if(IsEmpty(pqueue)!=1&&pnode!=NULL)
	{
		pqueue->size--;
		pqueue->front = pnode->next;
		free(pnode->data);
		free(pnode);
		if(pqueue->size==0)
		{
			pqueue->rear = NULL;
		}
	}
	pthread_mutex_unlock(&pqueue->mutex);
	return pqueue->front;
}

/*遍历队列并对各数据项调用visit函数*/
void QueueTraverse(Queue *pqueue,void (*visit)())
{
	PQueueNode pnode = pqueue->front;
	int i = pqueue->size;
	while(i--)
	{
		visit(pnode->data);
		pnode = pnode->next;
	}
		
}

