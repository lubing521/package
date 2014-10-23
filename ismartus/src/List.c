
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "List.h"


/*构造一个空列表*/
List *InitList()
{
	List *plist = (List *)malloc(sizeof(List));
	if(plist!=NULL)
	{
		plist->front = NULL;
		plist->size = 0;
		pthread_mutex_init(&plist->mutex,NULL);
	}
	return plist;
}

/*销毁一个列表*/
void DestroyList(List *plist)
{
	if(ListIsEmpty(plist)!=1)
		ClearList(plist);
	free(plist);
}

/*清空一个列表*/
void ClearList(List *plist)
{
	while(ListIsEmpty(plist)!=1)
	{
		DeListFront(plist);
	}

}

/*判断列表是否为空*/
int ListIsEmpty(List *plist)
{
	if(plist->front==NULL && plist->size==0)
		return 1;
	else
		return 0;
}

/*返回列表大小*/
int GetListSize(List *plist)
{
	return plist->size;
}

/*将新元素入列表,添加到列表头部*/
PNode EnListFront(List *plist, void *pData, unsigned int nLen)
{
	if (NULL == plist || NULL == pData)
	{
		return NULL;
	}
	pthread_mutex_lock(&plist->mutex);
	PNode pnode = (PNode)malloc(sizeof(Node));
	if(pnode != NULL)
	{
		pnode->data = malloc(nLen);
		memcpy(pnode->data, pData, nLen);
		pnode->next = plist->front;
		plist->front = pnode;
		plist->size++;
	}
	pthread_mutex_unlock(&plist->mutex);
	return pnode;
}

/*队头元素出列表*/
PNode DeListFront(List *plist)
{
	DeListIndex(plist, 0);
	return plist->front;
}

/*删除指定节点*/
void DeListNode(List *plist, PNode pnode)
{
	if(ListIsEmpty(plist) == 1 || pnode == NULL)
	{
		return;
	}
	PNode ptmp = plist->front;
	unsigned int nIndex = 0;
	for (; ptmp; ptmp = ptmp->next, nIndex++)
	{
		if (ptmp == pnode)
		{
			DeListIndex(plist, nIndex);
			break;
		}
	}
	return;
}

void DeListIndex(List *plist, unsigned int nIndex)
{
	if(ListIsEmpty(plist) == 1 || nIndex >= GetListSize(plist))
	{
		return;
	}
	/*上锁*/
	pthread_mutex_lock(&plist->mutex);
	PNode freenode = plist->front;
	PNode lastnode = NULL;
	if (0 == nIndex)
	{
		plist->front = plist->front->next;
	}
	else
	{
		unsigned i = 0;
		for (; i < nIndex; i++)
		{
			lastnode = freenode;
			freenode = freenode->next;
		}
		lastnode->next = freenode->next;
	}
	free(freenode->data);
	free(freenode);
	plist->size--;
	pthread_mutex_unlock(&plist->mutex);
	return;
}

/*获取列表头部节点*/
PNode GetListFront(List *plist)
{
	if (plist)
	{
		return plist->front;
	}
	return NULL;
}

/*获取指定节点的下一个节点*/
PNode GetListNext(PNode pnode)
{
	if (pnode)
	{
		return pnode->next;
	}
	return NULL;
}

