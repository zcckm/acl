//******************************************************************************
//模块名	： acl_msgqueue
//文件名	： acl_msgqueue.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL使用的环形消息队列
//------------------------------------------------------------------------------
//修改记录:
//2015-01-19 zcckm 创建
//******************************************************************************
#ifndef _ACL_MSGQUEUE_H_
#define _ACL_MSGQUEUE_H_

#include "acl_c.h"


// #ifdef __cplusplus
// extern "C" {
// #endif //extern "C"

typedef struct tagQueueMember
{
    void * pContent;
    int nContentLen;
    struct tagQueueMember * ptNext;
    struct tagQueueMember * ptPrev;
}TQUEUE_MEMBER,* PTQUEUE_MEMBER;


typedef enum
{
    ACL_QUE_NO_COPYMODE = 0,
    ACL_QUE_SHALW_COPYMODE = 1,//means already set TACLMessage In Queue
    ACL_QUE_DEEP_COPYMODE = 2,
}E_HANDLE_MSG_MODE;

typedef struct tagMsgQueue
{
    PTQUEUE_MEMBER LACP;//loop array begin position
    PTQUEUE_MEMBER LAEP;//loop array current insert position
    int nQueMemberRegSize;
    u32 dwDropMsgCount;
    u32 dwHandleMsgCount;
    H_ACL_LOCK hQueLock;
    H_ACL_SEM hQueSem;
    int nSetMaxQueueNum;
    int m_nCurQueMembNum;
    E_HANDLE_MSG_MODE eHandleMode;//0 copy data when insert (malloc free)
    //1 set  data when insert (=          )
}TMSG_QUEUE,* PTMSG_QUEUE;



//环形栈链表 消息队列
ACL_API PTMSG_QUEUE createAclCircleQueue(int nMsgNumber);

ACL_API void destroyAclCircleQueue(PTMSG_QUEUE ptAclMsgQueue);

ACL_API PTMSG_QUEUE createAclMsgQueue(int nMsgNumber, E_HANDLE_MSG_MODE eMsgMode, int nSizeOfAttatch);

ACL_API int destoryAclMsgQueue(PTMSG_QUEUE ptAclMsgQueue);

ACL_API int insertAclMsg(PTMSG_QUEUE ptAclMsgQueue,TAclMessage * ptAclMsg);

ACL_API int insertAclCustom(PTMSG_QUEUE ptAclMsgQueue,void * ptAclMsg, int nMsgLen);

ACL_API int getAclMsg(PTMSG_QUEUE ptAclMsgQueue,TAclMessage * ptMsg,int * pnMsgLen,u32 nCheckMsgTime);

//双向链表 消息队列

ACL_API PTMSG_QUEUE createAclDLList(int nMsgNumber,E_HANDLE_MSG_MODE eHandleMode);
ACL_API int destroyAclDLList(PTMSG_QUEUE ptAclMsgQueue);

ACL_API PTQUEUE_MEMBER insertAclDLList(PTMSG_QUEUE ptMsgQueue, void * pContent, int nContentLen);

ACL_API int deletAclDLList(PTMSG_QUEUE ptMsgQueue, PTQUEUE_MEMBER ptQueMember);

ACL_API int setAclMsg(PTMSG_QUEUE ptAclMsgQueue,u16 wMsgType,int nInfo);

ACL_API PTQUEUE_MEMBER getAclDLListFirstMember(PTMSG_QUEUE ptMsgQueue);

ACL_API PTQUEUE_MEMBER getAclDLListEndMember(PTMSG_QUEUE ptMsgQueue);
// #ifdef __cplusplus
// }
// #endif //extern "C"

#endif

