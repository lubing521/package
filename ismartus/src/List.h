#ifndef LIST_H
#define LIST_H 

/*通用数据结构链表,邓俊勇*/

#include <pthread.h>
#include <unistd.h>

typedef struct node
{
	void *data; /*任何数据结构都使用指针,所以通用*/
	struct node *next;
}Node, *PNode;

typedef struct
{
	PNode front;
	int size;
	pthread_mutex_t mutex;
}List;

/*构造一个空列表*/
List *InitList();

/*销毁一个列表*/
void DestroyList(List *plist);

/*清空一个列表*/
void ClearList(List *plist);

/*判断列表是否为空*/
int ListIsEmpty(List *plist);

/*返回列表大小*/
int GetListSize(List *plist);

/*将新元素入列表头部,需要指定长度nLen,为了数据结构通用*/
PNode EnListFront(List *plist, void *pData, unsigned int nLen);

/*列表头部元素出列表*/
PNode DeListFront(List *plist);

/*列表指定元素出列表*/
void DeListNode(List *plist, PNode pnode);

/*列表指定位置出列表*/
void DeListIndex(List *plist, unsigned int nIndex);

/*获取列表头部节点*/
PNode GetListFront(List *plist);

/*获取指定节点的下一个节点*/
PNode GetListNext(PNode pnode);
#endif 
