
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "List.h"


/*����һ�����б�*/
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

/*����һ���б�*/
void DestroyList(List *plist)
{
	if(ListIsEmpty(plist)!=1)
		ClearList(plist);
	free(plist);
}

/*���һ���б�*/
void ClearList(List *plist)
{
	while(ListIsEmpty(plist)!=1)
	{
		DeListFront(plist);
	}

}

/*�ж��б��Ƿ�Ϊ��*/
int ListIsEmpty(List *plist)
{
	if(plist->front==NULL && plist->size==0)
		return 1;
	else
		return 0;
}

/*�����б��С*/
int GetListSize(List *plist)
{
	return plist->size;
}

/*����Ԫ�����б�,��ӵ��б�ͷ��*/
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

/*��ͷԪ�س��б�*/
PNode DeListFront(List *plist)
{
	DeListIndex(plist, 0);
	return plist->front;
}

/*ɾ��ָ���ڵ�*/
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
	/*����*/
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

/*��ȡ�б�ͷ���ڵ�*/
PNode GetListFront(List *plist)
{
	if (plist)
	{
		return plist->front;
	}
	return NULL;
}

/*��ȡָ���ڵ����һ���ڵ�*/
PNode GetListNext(PNode pnode)
{
	if (pnode)
	{
		return pnode->next;
	}
	return NULL;
}

