#ifndef LIST_H
#define LIST_H 

/*ͨ�����ݽṹ����,�˿���*/

#include <pthread.h>
#include <unistd.h>

typedef struct node
{
	void *data; /*�κ����ݽṹ��ʹ��ָ��,����ͨ��*/
	struct node *next;
}Node, *PNode;

typedef struct
{
	PNode front;
	int size;
	pthread_mutex_t mutex;
}List;

/*����һ�����б�*/
List *InitList();

/*����һ���б�*/
void DestroyList(List *plist);

/*���һ���б�*/
void ClearList(List *plist);

/*�ж��б��Ƿ�Ϊ��*/
int ListIsEmpty(List *plist);

/*�����б��С*/
int GetListSize(List *plist);

/*����Ԫ�����б�ͷ��,��Ҫָ������nLen,Ϊ�����ݽṹͨ��*/
PNode EnListFront(List *plist, void *pData, unsigned int nLen);

/*�б�ͷ��Ԫ�س��б�*/
PNode DeListFront(List *plist);

/*�б�ָ��Ԫ�س��б�*/
void DeListNode(List *plist, PNode pnode);

/*�б�ָ��λ�ó��б�*/
void DeListIndex(List *plist, unsigned int nIndex);

/*��ȡ�б�ͷ���ڵ�*/
PNode GetListFront(List *plist);

/*��ȡָ���ڵ����һ���ڵ�*/
PNode GetListNext(PNode pnode);
#endif 
