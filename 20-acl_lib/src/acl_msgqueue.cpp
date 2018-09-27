//******************************************************************************
//模块名	： acl_msgqueue
//文件名	： acl_msgqueue.c
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL使用的环形消息队列
//------------------------------------------------------------------------------
//修改记录:
//2015-01-19 zcckm 创建
//******************************************************************************
#include "acl_msgqueue.h"
#include "acl_memory.h"
#include <string.h>
#include "acl_lock.h"
#include "acl_telnet.h"
#include "acl_common.h"
#define MAX_WAIT_MSGQUEUE_MEMBER 2000


//=============================================================================
//函 数 名：createAclCircleQueue
//功	    能：创建环形队列
//算法实现：环形消息链
//注    意：此函数生成的是环形消息空链
//基于此函数可以附加不同的内容
//=============================================================================
PTMSG_QUEUE createAclCircleQueue(int nMsgNumber)
{
    PTMSG_QUEUE ptMsgQueue = NULL;
    PTQUEUE_MEMBER ptNewQueueMember = NULL, ptLastQueueMember = NULL;
    PTQUEUE_MEMBER ptFirst = NULL;
    int i,nSetMsgNumber;

    CHECK_NULL_RET_NULL( ptMsgQueue = (PTMSG_QUEUE)aclMallocClr(sizeof(TMSG_QUEUE)) )

    nSetMsgNumber = nMsgNumber;
    if (nMsgNumber > MAX_WAIT_MSGQUEUE_MEMBER)
    {
        nSetMsgNumber = MAX_WAIT_MSGQUEUE_MEMBER;
    }
    ptMsgQueue->nSetMaxQueueNum = nSetMsgNumber;
    for (i = 0; i < nSetMsgNumber; i++)
    {
        CHECK_NULL_RET_NULL(  ptNewQueueMember = (PTQUEUE_MEMBER)aclMallocClr(sizeof(TQUEUE_MEMBER)) )

        if (NULL == ptLastQueueMember)
        {
            ptLastQueueMember = ptNewQueueMember;
        }
        else
        {
            ptLastQueueMember->ptNext = ptNewQueueMember;
            ptLastQueueMember = ptLastQueueMember->ptNext;
        }

        if (0 == i)
        {
            ptFirst = ptNewQueueMember;
        }
    }
    ptLastQueueMember->ptNext = ptFirst;

    ptMsgQueue->LACP = ptFirst;
    ptMsgQueue->LAEP = ptFirst;    

    if (ACL_ERROR_INVALID == aclCreateLock(&ptMsgQueue->hQueLock,NULL))
    {
        return NULL;
    }

    if (ACL_ERROR_INVALID == aclCreateSem(&ptMsgQueue->hQueSem,MAX_WAIT_MSGQUEUE_MEMBER))
    {
        return NULL;
    }
    
    return ptMsgQueue;
}


//=============================================================================
//函 数 名：destroyAclMsgQueue
//功	    能：销毁环形队列
//算法实现：环形消息链
//注    意：此函数只释放环形消息空链,对于其附加内容应该在之前就释放掉
//考虑到不同的使用模式，
//=============================================================================
void destroyAclCircleQueue(PTMSG_QUEUE ptAclMsgQueue)
{
    PTQUEUE_MEMBER ptNextQueueMember = NULL, ptCureQueueMember = NULL;
    if (NULL == ptAclMsgQueue)
    {
        return;
    }
    ptAclMsgQueue->LACP = ptAclMsgQueue->LAEP;

    ptCureQueueMember = ptAclMsgQueue->LACP;
    {
        int i = 0;
        for (i = 0; i < ptAclMsgQueue->nSetMaxQueueNum; i++)
        {
            ptNextQueueMember = ptCureQueueMember->ptNext;
            aclFree(ptCureQueueMember);
            ptCureQueueMember = ptNextQueueMember;
        }
    }
    aclDestoryLock(ptAclMsgQueue->hQueLock);
    aclDestorySem(&ptAclMsgQueue->hQueSem);
    aclFree(ptAclMsgQueue);
}

//=============================================================================
//函 数 名：createAclMsgQueue
//功	    能：创建环形消息队列,基于环形队列，可以附加消息结构体比如 TAclMessage
//注	    意：
//算法实现：
//全局变量：
//参	    数：
//        pTMsgQueue:基础消息队列
//        nSizeOfAttatch:贴附消息队列的内存块大小
//如果是 TAclMessage 则参数应为 sizeof(TAclMessage)
//注    意:pContent如果非NULL，因为内部有拷贝过程，因此必须保证指针值指向正确位置
//         填写正确的dwContentLen值, 否则会造成崩溃
//=============================================================================
PTMSG_QUEUE createAclMsgQueue(int nMsgNumber, E_HANDLE_MSG_MODE eMsgMode, int nSizeOfAttatch)
{
    PTMSG_QUEUE ptMsgQueue = NULL;

    //创建空的环形队列
    ptMsgQueue = createAclCircleQueue(nMsgNumber);
    if (NULL == ptMsgQueue)
    {
        return NULL;
    }
    ptMsgQueue->eHandleMode = eMsgMode;

    //初始化注册的消息大小
    ptMsgQueue->nQueMemberRegSize = nSizeOfAttatch;

    if (ACL_QUE_SHALW_COPYMODE == eMsgMode || ACL_QUE_DEEP_COPYMODE == eMsgMode)
    {
        //这里需要分配环形消息队列所有节点 TAclMessage结构体
        PTQUEUE_MEMBER ptMsgMb = NULL;
        for (ptMsgMb = ptMsgQueue->LACP;ptMsgMb->ptNext != ptMsgQueue->LACP; ptMsgMb = ptMsgMb->ptNext)
        {
            ptMsgMb->pContent = aclMallocClr(nSizeOfAttatch);
            ptMsgMb->nContentLen = nSizeOfAttatch;
        }
        //only left last not malloc
        ptMsgMb->pContent = aclMallocClr(nSizeOfAttatch);
        ptMsgMb->nContentLen = nSizeOfAttatch;
    }
    return ptMsgQueue;
}

//=============================================================================
//函 数 名：createAclMsgQueue
//功	    能：销毁环形消息队列
//注	    意：
//算法实现：
//全局变量：
//参	    数：
//        ptAclMsgQueue:要销毁的消息队列指针
//注    意:消息队列内容的释放与否取决于消息队列处理模式，
//因此eHandleMode在创建消息队列后就不可以更改了
//=============================================================================
int destoryAclMsgQueue(PTMSG_QUEUE ptAclMsgQueue)
{
    if (NULL == ptAclMsgQueue)
    {
        return ACL_ERROR_PARAM;
    }

    if (ACL_QUE_SHALW_COPYMODE == ptAclMsgQueue->eHandleMode||
        ACL_QUE_DEEP_COPYMODE == ptAclMsgQueue->eHandleMode)
    {
        PTQUEUE_MEMBER ptMsgMb = NULL;
        for (ptMsgMb = ptAclMsgQueue->LACP;ptMsgMb->ptNext != ptAclMsgQueue->LACP; ptMsgMb = ptMsgMb->ptNext)
        {
            if (ptMsgMb->pContent)
            {
                aclFree(ptMsgMb->pContent);
                ptMsgMb->nContentLen = 0;
            }
        }
        if (ptMsgMb->nContentLen)
        {
            aclFree(ptMsgMb->pContent);
            ptMsgMb->nContentLen = 0;
        }
    }
    destroyAclCircleQueue(ptAclMsgQueue);
    return ACL_ERROR_NOERROR;
}

//=============================================================================
//函 数 名：insertAclMsg
//功	  能：将消息插入到消息队列中
//注	  意：
//算法实现：
//全局变量：
//参	  数：
//        ptAclMsgQueue:要插入的目标消息队列
//        ptAclMsg:要插入的消息

//注    意:ptAclMsgQueue 消息队列模式问题

//消息分为消息头(TAclMessage)和消息体(TAclMessage.m_pContent)

//ACL_QUE_NO_COPYMODE
//不常用 
//无拷贝模式，就是用户分配消息头和消息体，然后移交内存管理权
//然后由消息队列管理
//这样会造成消息体频繁的释放，造成内存碎片和效率等问题
//

//ACL_QUE_SHALW_COPYMODE
//当前使用的模式
//浅拷贝模式  消息头由用户维护，消息体由用户分配并移交管理权
//而消息content 由使用者分配(必须malloc)，消息队列维护

//ACL_QUE_DEEP_COPYMODE
//不常用
//深拷贝模式  表示用户维护消息头，消息体，调用此函数后释放消息也没关系
//用户将消息插入完成后，自行释放而不会影响消息处理
//这样会造成多次拷贝降低效率的问题

//以上，各个模式的选区取决于不同的应用场景，要特别注意使用的区别
//=============================================================================
int insertAclMsg(PTMSG_QUEUE ptAclMsgQueue,TAclMessage * ptAclMsg)
{
    TAclMessage * ptAclQueMsg = NULL;
    CHECK_NULL_RET_ERR_PARAM(ptAclMsgQueue);
    lockLock(ptAclMsgQueue->hQueLock);
    
    if (ptAclMsgQueue->LAEP->ptNext == ptAclMsgQueue->LACP)// loop msg is full, return 
    {
	//模式改为覆盖没有处理的消息，后续会修改成为可配置类型
		ptAclMsgQueue->dwDropMsgCount++;
       unlockLock(ptAclMsgQueue->hQueLock);
       return ACL_ERROR_OVERFLOW;
        ptAclMsgQueue->LACP = ptAclMsgQueue->LACP->ptNext;
		aclPrintf(TRUE, FALSE, "[insertAclMsg] overflow, Msg SRC_ID: %X DST_ID: %X\n", ptAclMsg->m_dwSrcIID, ptAclMsg->m_dwDstIID);
    }
    if (sizeof(TAclMessage) != ptAclMsgQueue->nQueMemberRegSize)
    {
        ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[insertAclCustom] reg msg size[%d] and insert msg size[%d] is not matched\n",
            ptAclMsgQueue->nQueMemberRegSize,sizeof(ptAclMsg));
		unlockLock(ptAclMsgQueue->hQueLock);
        return ACL_ERROR_CONFLICT;
    }

    switch(ptAclMsgQueue->eHandleMode)
    {
        
    case ACL_QUE_NO_COPYMODE: 
        {
            ptAclMsgQueue->LAEP->nContentLen = sizeof(TAclMessage);
            ptAclMsgQueue->LAEP->pContent = ptAclMsg;
        }
        break;

    case ACL_QUE_SHALW_COPYMODE:
        {
            ptAclQueMsg = (TAclMessage *)ptAclMsgQueue->LAEP->pContent;
            memcpy(ptAclQueMsg,ptAclMsg,sizeof(TAclMessage));
        }
        break;

    case ACL_QUE_DEEP_COPYMODE:
        {
            ACL_DEBUG(E_MOD_MSG, E_TYPE_NOTIF, "[insertAclMsg] insert message mode:CopyMode\n");
            ptAclQueMsg = (TAclMessage *)ptAclMsgQueue->LAEP->pContent;
            memcpy(ptAclQueMsg,ptAclMsg,sizeof(TAclMessage));
            if (NULL != ptAclQueMsg->m_pContent)
            {
                aclFree(ptAclQueMsg->m_pContent);
            }
            ptAclQueMsg->m_pContent = (u8 *)aclMallocClr(ptAclMsg->m_dwContentLen);
            ptAclQueMsg->m_dwContentLen = ptAclMsg->m_dwContentLen;
            if (NULL == ptAclQueMsg->m_pContent)
            {
                unlockLock(ptAclMsgQueue->hQueLock);
                return ACL_ERROR_MALLOC;
            }
            memcpy(ptAclQueMsg->m_pContent,ptAclMsg->m_pContent,ptAclMsg->m_dwContentLen);
        }
        break;
    default:
        ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[insertAclMsg] unknown CopyMode %d\n", ptAclMsgQueue->eHandleMode);

    }
    ptAclMsgQueue->LAEP = ptAclMsgQueue->LAEP->ptNext;
	ptAclMsgQueue->m_nCurQueMembNum++;
    aclReleaseSem(&ptAclMsgQueue->hQueSem);//insert one msg,so release one sem
    unlockLock(ptAclMsgQueue->hQueLock);
    return ACL_ERROR_NOERROR;
}


//=============================================================================
//函 数 名：insertAclMsg
//功	  能：将消息插入到消息队列中
//注	  意：
//算法实现：
//全局变量：
//参	  数：
//        ptAclMsgQueue:要插入的目标消息队列
//        ptAclMsg:要插入的消息

//注    意:insertAclCustom 和 insertAclMsg区别在于
//insertAclMsg 已经指定好消息为标准的 TAclMessage 而此函数可以插入定制类型，
//是使用消息队列功能的一个扩展,因此使用此类型的时候，浅拷贝，深拷贝的概念不再适用
//=============================================================================
int insertAclCustom(PTMSG_QUEUE ptAclMsgQueue,void * ptAclMsg, int nMsgLen)
{
    TAclMessage * ptAclQueMsg = NULL;
    CHECK_NULL_RET_INVALID(ptAclMsgQueue)
    lockLock(ptAclMsgQueue->hQueLock);

    if (ptAclMsgQueue->LAEP->ptNext == ptAclMsgQueue->LACP)// loop msg is full, return 
    {
        unlockLock(ptAclMsgQueue->hQueLock);
        return ACL_ERROR_OVERFLOW;
    }
    if (nMsgLen != ptAclMsgQueue->nQueMemberRegSize)
    {
        ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[insertAclCustom] reg msg size[%d] and insert msg size[%d] is not matched\n",
            ptAclMsgQueue->nQueMemberRegSize,nMsgLen);
		unlockLock(ptAclMsgQueue->hQueLock);
        return ACL_ERROR_CONFLICT;
    }
    switch(ptAclMsgQueue->eHandleMode)
    {

    case ACL_QUE_NO_COPYMODE: 
        {
            ptAclMsgQueue->LAEP->nContentLen = nMsgLen;
            ptAclMsgQueue->LAEP->pContent = ptAclMsg;
        }
        break;

    case ACL_QUE_SHALW_COPYMODE:
        {
            ptAclQueMsg = (TAclMessage *)ptAclMsgQueue->LAEP->pContent;
            memcpy(ptAclQueMsg,ptAclMsg,sizeof(nMsgLen));
        }
        break;

    case ACL_QUE_DEEP_COPYMODE:
        {
            ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[insertAclMsg] this function is not support ACL_QUE_DEEP_COPYMODE\n");
        }
        break;
    default:
        ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[insertAclMsg] unknown CopyMode %d\n", ptAclMsgQueue->eHandleMode);
    }
    ptAclMsgQueue->LAEP = ptAclMsgQueue->LAEP->ptNext;

    aclReleaseSem(&ptAclMsgQueue->hQueSem);//insert one msg,so release one sem
    unlockLock(ptAclMsgQueue->hQueLock);
    return 0;
}

//
ACL_API int setAclMsg(PTMSG_QUEUE ptAclMsgQueue,u16 wMsgType,int nInfo)
{
    TAclMessage * ptAclQueMsg = NULL;
    PTQUEUE_MEMBER ptFirst = NULL,ptEnd = NULL;
    CHECK_NULL_RET_INVALID(ptAclMsgQueue)
    lockLock(ptAclMsgQueue->hQueLock);

    ptFirst = ptAclMsgQueue->LACP;
    ptEnd = ptAclMsgQueue->LAEP;

    if (MSGMASK_THROW_AWAY == nInfo)//ask for throw away wMsgType 
    {
        for (;ptFirst != ptEnd;ptFirst = ptFirst->ptNext)
        {
            ptAclQueMsg = (TAclMessage *)ptFirst->pContent;
            if (wMsgType == ptAclQueMsg->m_wMsgType)
            {
                ptAclQueMsg->m_wMsgStatus = MSGMASK_THROW_AWAY;
            }
        }
    }

    unlockLock(ptAclMsgQueue->hQueLock);
    return 0;
}

ACL_API int getAclMsg(PTMSG_QUEUE ptAclMsgQueue,TAclMessage * ptMsg,int * pnMsgLen,u32 nCheckMsgTime)
{
    // check aclsem
    int nRet = 0;
    CHECK_NULL_RET_ERR_PARAM(ptAclMsgQueue);
    CHECK_NULL_RET_ERR_PARAM(ptMsg);
    CHECK_NULL_RET_ERR_PARAM(pnMsgLen);
    nRet = aclCheckGetSem_b(&ptAclMsgQueue->hQueSem, nCheckMsgTime);
    if (ACL_ERROR_TIMEOUT != nRet && ACL_ERROR_NOERROR != nRet)
    {
        ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR,"[getAclMsg] MsgQue getSem unknown error %d",nRet);
        return nRet;
    }
    if (ACL_ERROR_TIMEOUT == nRet)
    {
        ACL_DEBUG(E_MOD_MSG, E_TYPE_WARNNING, "[getAclMsg] get message time out\n");
        return ACL_ERROR_TIMEOUT;
    }
    lockLock(ptAclMsgQueue->hQueLock);

    if (ptAclMsgQueue->LAEP->ptNext == ptAclMsgQueue->LACP)// loop msg is full,handle hurry
    {
//        unlockLock(ptAclMsgQueue->hQueLock);
//        return ACL_ERROR_OVERFLOW;
    }
    if (ptAclMsgQueue->LAEP == ptAclMsgQueue->LACP)// no msg in loop
    {
        ACL_DEBUG(E_MOD_MSG, E_TYPE_DEBUG, "[getAclMsg] no msg in loop\n");
        unlockLock(ptAclMsgQueue->hQueLock);
        return ACL_ERROR_EMPTY;
    }
    if (NULL == pnMsgLen)// registered callback function is null, bored
    {
//        unlockLock(ptAclMsgQueue->hQueLock);
//        return ACL_ERROR_INVALID;
    }

    //get current msg,now message need thread manage ,rember after handle it, free it
    memcpy(ptMsg, ptAclMsgQueue->LACP->pContent, ptAclMsgQueue->LACP->nContentLen);
    * pnMsgLen = ptAclMsgQueue->LACP->nContentLen;

    ptAclMsgQueue->LACP = ptAclMsgQueue->LACP->ptNext;

	//队列消息号--
	ptAclMsgQueue->m_nCurQueMembNum--;

	//处理的消息号++
	ptAclMsgQueue->dwHandleMsgCount++;

    unlockLock(ptAclMsgQueue->hQueLock);
    return ACL_ERROR_NOERROR;
}


//start 双向链表 消息队列
//链表说明:链表主要为消息投递设计，新的消息到来通过此链表可以承载新的动态分配的消息，然后投递给环形队列
//其他情况最好不要用
//消息队列结构LACP - - - LAEP LACP永远指向第一个节点，LAEP总是指向最后一个节点
//双向链表是一个
ACL_API PTMSG_QUEUE createAclDLList(int nMsgNumber,E_HANDLE_MSG_MODE eHandleMode)
{
    
    PTMSG_QUEUE ptMsgQueue = NULL;
//    PTQUEUE_MEMBER ptFirst;
    int nSetMsgNumber;

    CHECK_NULL_RET_NULL( ptMsgQueue = (PTMSG_QUEUE)aclMallocClr(sizeof(TMSG_QUEUE)) )
    memset(ptMsgQueue,0,sizeof(TMSG_QUEUE));

    ptMsgQueue->LACP = NULL;
    ptMsgQueue->LAEP = NULL;

    nSetMsgNumber = nMsgNumber;

    if (nMsgNumber > MAX_WAIT_MSGQUEUE_MEMBER)
    {
        nSetMsgNumber = MAX_WAIT_MSGQUEUE_MEMBER;
    }
    ptMsgQueue->nSetMaxQueueNum = nSetMsgNumber;

    ptMsgQueue->eHandleMode = eHandleMode;// handle mode

    if (ACL_ERROR_INVALID == aclCreateLock(&ptMsgQueue->hQueLock,NULL))
    {
        ACL_DEBUG(E_MOD_LOCK, E_TYPE_ERROR,"[createAclDLList] create lock error\n");
        return NULL;
    }

    if (ACL_ERROR_INVALID == aclCreateSem(&ptMsgQueue->hQueSem,nMsgNumber))
    {
        ACL_DEBUG(E_MOD_LOCK, E_TYPE_ERROR,"[createAclDLList] create semaphore error\n");
        return NULL;
    }

    return ptMsgQueue;
}

ACL_API int destroyAclDLList(PTMSG_QUEUE ptAclMsgQueue)
{
    //一旦调用销毁链表消息队列，则删除所有消息
    int i = 0;
    if (NULL == ptAclMsgQueue)
    {
        return ACL_ERROR_INVALID;
    }

    if (ACL_QUE_DEEP_COPYMODE == ptAclMsgQueue->eHandleMode)
    {
        lockLock(ptAclMsgQueue->hQueLock);
        for (i = 0; i < ptAclMsgQueue->m_nCurQueMembNum; i++)
        {
            aclFree(ptAclMsgQueue->LACP->pContent);
            aclFree(ptAclMsgQueue);
            ptAclMsgQueue->LACP = ptAclMsgQueue->LACP->ptPrev;
        }
        unlockLock(ptAclMsgQueue->hQueLock);
    }

    aclDestoryLock(ptAclMsgQueue->hQueLock);
    aclDestorySem(&ptAclMsgQueue->hQueSem);
    aclFree(ptAclMsgQueue);
    return ACL_ERROR_NOERROR;
}

//对于双向链表，LACP永远是开头，LAEP永远是最后，新的节点必须是动态分配的内存块
//当调用函数插入节点后，内存块管理权移交给双向链表
//(双向链表的节点将会插入到instance的环形链表中，最终由环形链表调用并释放内存块)
ACL_API PTQUEUE_MEMBER insertAclDLList(PTMSG_QUEUE ptMsgQueue, void * pContent, int nContentLen)
{
    //为了防止内存频繁申请释放，以后需要引入内存池层
    PTQUEUE_MEMBER ptNewQueueMember = NULL;
    void * p = NULL;

    lockLock(ptMsgQueue->hQueLock);
    if (ptMsgQueue->nSetMaxQueueNum <= ptMsgQueue->m_nCurQueMembNum)// had reach max member
    {
        unlockLock(ptMsgQueue->hQueLock);
        return NULL;
    }

    CHECK_NULL_RET_NULL(  ptNewQueueMember = (PTQUEUE_MEMBER)aclMallocClr(sizeof(TQUEUE_MEMBER)) )

    memset(ptNewQueueMember, 0, sizeof(TQUEUE_MEMBER));

    //add one member end the queue, must be at the end if dlist now
    
    if (ptMsgQueue->m_nCurQueMembNum >= ptMsgQueue->nSetMaxQueueNum)// have reached limit memb num
    {
		unlockLock(ptMsgQueue->hQueLock);
        return NULL;
    }
    if(NULL == ptMsgQueue->LACP)// means list have no node
    {
        ptMsgQueue->LACP = ptNewQueueMember;
        ptMsgQueue->LAEP = ptNewQueueMember;
        ptNewQueueMember->ptNext = NULL;
        ptNewQueueMember->ptPrev = NULL;
    }
    else// new node insert at the end of the list
    {
        //插入新节点到最后，将LAEP设为新插入节点
        PTQUEUE_MEMBER pPreMember = NULL;
        pPreMember = ptMsgQueue->LAEP;
        pPreMember->ptNext = ptNewQueueMember;

        ptNewQueueMember->ptPrev = pPreMember;
        ptNewQueueMember->ptNext = NULL;
        ptMsgQueue->LAEP = ptNewQueueMember;
    }
    
    ptMsgQueue->m_nCurQueMembNum++;

    //dllist ACL_QUE_INST_MSG_MODE is working for inst msg queue,just malloc insert, but free by inst queue
    if (ACL_QUE_DEEP_COPYMODE == ptMsgQueue->eHandleMode && NULL != pContent)
    {
        p = aclMallocClr(nContentLen);
        memset(p, 0, nContentLen);
        memcpy(p,pContent,nContentLen);
    }
    else
    {
        p = pContent;
    }

    ptNewQueueMember->nContentLen = nContentLen;
    ptNewQueueMember->pContent = p;
    
    unlockLock(ptMsgQueue->hQueLock);
    return ptNewQueueMember;
    
}

ACL_API int deletAclDLList(PTMSG_QUEUE ptMsgQueue, PTQUEUE_MEMBER ptQueMember)
{
    PTQUEUE_MEMBER ptTmpQueMember = NULL;
    CHECK_NULL_RET_INVALID(ptMsgQueue)
    CHECK_NULL_RET_INVALID(ptQueMember)

    ptTmpQueMember = ptQueMember;
    lockLock(ptMsgQueue->hQueLock);
    ptMsgQueue->m_nCurQueMembNum--;

    if (NULL == ptQueMember->ptPrev && NULL == ptQueMember->ptNext)//this is only left node
    {
        ptMsgQueue->LAEP = NULL;
        ptMsgQueue->LACP = NULL;
    }
    else if(NULL == ptQueMember->ptPrev)// this is the first node
    {
        ptMsgQueue->LACP = ptMsgQueue->LACP->ptNext;
        ptMsgQueue->LACP->ptPrev = NULL;
    }
    else if (NULL == ptQueMember->ptNext)//this is the end node
    {
        ptMsgQueue->LAEP = ptMsgQueue->LAEP->ptPrev;
        ptMsgQueue->LAEP->ptNext = NULL;
    }
    else//general node
    {
        ptQueMember->ptPrev->ptNext = ptQueMember->ptNext;
        ptQueMember->ptNext->ptPrev = ptQueMember->ptPrev;
    }

    unlockLock(ptMsgQueue->hQueLock);
    if (NULL != ptTmpQueMember->pContent)
    {
        aclFree(ptTmpQueMember->pContent);
    }
    
    if(ACL_QUE_DEEP_COPYMODE == ptMsgQueue->eHandleMode ||
       ACL_QUE_SHALW_COPYMODE == ptMsgQueue->eHandleMode)
    {
        aclFree(ptQueMember);
    }
    return ACL_ERROR_NOERROR;
}

ACL_API PTQUEUE_MEMBER getAclDLListFirstMember(PTMSG_QUEUE ptMsgQueue)
{
    CHECK_NULL_RET_NULL(ptMsgQueue)
    return ptMsgQueue->LACP;
}

ACL_API PTQUEUE_MEMBER getAclDLListEndMember(PTMSG_QUEUE ptMsgQueue)
{
    CHECK_NULL_RET_NULL(ptMsgQueue)
    return ptMsgQueue->LAEP;
}

//end 双向链表 消息队列
#ifdef  _MSC_VER

#endif