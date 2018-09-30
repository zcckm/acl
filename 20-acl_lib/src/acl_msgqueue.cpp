//******************************************************************************
//ģ����	�� acl_msgqueue
//�ļ���	�� acl_msgqueue.c
//����	�� zcckm
//�汾	�� 1.0
//�ļ�����˵��:
//ACLʹ�õĻ�����Ϣ����
//------------------------------------------------------------------------------
//�޸ļ�¼:
//2015-01-19 zcckm ����
//******************************************************************************
#include "acl_msgqueue.h"
#include "acl_memory.h"
#include <string.h>
#include "acl_lock.h"
#include "acl_telnet.h"
#include "acl_common.h"
#define MAX_WAIT_MSGQUEUE_MEMBER 2000


//=============================================================================
//�� �� ����createAclCircleQueue
//��	    �ܣ��������ζ���
//�㷨ʵ�֣�������Ϣ��
//ע    �⣺�˺������ɵ��ǻ�����Ϣ����
//���ڴ˺������Ը��Ӳ�ͬ������
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
//�� �� ����destroyAclMsgQueue
//��	    �ܣ����ٻ��ζ���
//�㷨ʵ�֣�������Ϣ��
//ע    �⣺�˺���ֻ�ͷŻ�����Ϣ����,�����丽������Ӧ����֮ǰ���ͷŵ�
//���ǵ���ͬ��ʹ��ģʽ��
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
//�� �� ����createAclMsgQueue
//��	    �ܣ�����������Ϣ����,���ڻ��ζ��У����Ը�����Ϣ�ṹ����� TAclMessage
//ע	    �⣺
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//        pTMsgQueue:������Ϣ����
//        nSizeOfAttatch:������Ϣ���е��ڴ���С
//����� TAclMessage �����ӦΪ sizeof(TAclMessage)
//ע    ��:pContent�����NULL����Ϊ�ڲ��п������̣���˱��뱣ָ֤��ֵָ����ȷλ��
//         ��д��ȷ��dwContentLenֵ, �������ɱ���
//=============================================================================
PTMSG_QUEUE createAclMsgQueue(int nMsgNumber, E_HANDLE_MSG_MODE eMsgMode, int nSizeOfAttatch)
{
    PTMSG_QUEUE ptMsgQueue = NULL;

    //�����յĻ��ζ���
    ptMsgQueue = createAclCircleQueue(nMsgNumber);
    if (NULL == ptMsgQueue)
    {
        return NULL;
    }
    ptMsgQueue->eHandleMode = eMsgMode;

    //��ʼ��ע�����Ϣ��С
    ptMsgQueue->nQueMemberRegSize = nSizeOfAttatch;

    if (ACL_QUE_SHALW_COPYMODE == eMsgMode || ACL_QUE_DEEP_COPYMODE == eMsgMode)
    {
        //������Ҫ���价����Ϣ�������нڵ� TAclMessage�ṹ��
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
//�� �� ����createAclMsgQueue
//��	    �ܣ����ٻ�����Ϣ����
//ע	    �⣺
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//        ptAclMsgQueue:Ҫ���ٵ���Ϣ����ָ��
//ע    ��:��Ϣ�������ݵ��ͷ����ȡ������Ϣ���д���ģʽ��
//���eHandleMode�ڴ�����Ϣ���к�Ͳ����Ը�����
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
//�� �� ����insertAclMsg
//��	  �ܣ�����Ϣ���뵽��Ϣ������
//ע	  �⣺
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	  ����
//        ptAclMsgQueue:Ҫ�����Ŀ����Ϣ����
//        ptAclMsg:Ҫ�������Ϣ

//ע    ��:ptAclMsgQueue ��Ϣ����ģʽ����

//��Ϣ��Ϊ��Ϣͷ(TAclMessage)����Ϣ��(TAclMessage.m_pContent)

//ACL_QUE_NO_COPYMODE
//������ 
//�޿���ģʽ�������û�������Ϣͷ����Ϣ�壬Ȼ���ƽ��ڴ����Ȩ
//Ȼ������Ϣ���й���
//�����������Ϣ��Ƶ�����ͷţ�����ڴ���Ƭ��Ч�ʵ�����
//

//ACL_QUE_SHALW_COPYMODE
//��ǰʹ�õ�ģʽ
//ǳ����ģʽ  ��Ϣͷ���û�ά������Ϣ�����û����䲢�ƽ�����Ȩ
//����Ϣcontent ��ʹ���߷���(����malloc)����Ϣ����ά��

//ACL_QUE_DEEP_COPYMODE
//������
//���ģʽ  ��ʾ�û�ά����Ϣͷ����Ϣ�壬���ô˺������ͷ���ϢҲû��ϵ
//�û�����Ϣ������ɺ������ͷŶ�����Ӱ����Ϣ����
//��������ɶ�ο�������Ч�ʵ�����

//���ϣ�����ģʽ��ѡ��ȡ���ڲ�ͬ��Ӧ�ó�����Ҫ�ر�ע��ʹ�õ�����
//=============================================================================
int insertAclMsg(PTMSG_QUEUE ptAclMsgQueue,TAclMessage * ptAclMsg)
{
    TAclMessage * ptAclQueMsg = NULL;
    CHECK_NULL_RET_ERR_PARAM(ptAclMsgQueue);
    lockLock(ptAclMsgQueue->hQueLock);
    
    if (ptAclMsgQueue->LAEP->ptNext == ptAclMsgQueue->LACP)// loop msg is full, return 
    {
	//ģʽ��Ϊ����û�д������Ϣ���������޸ĳ�Ϊ����������
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
//�� �� ����insertAclMsg
//��	  �ܣ�����Ϣ���뵽��Ϣ������
//ע	  �⣺
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	  ����
//        ptAclMsgQueue:Ҫ�����Ŀ����Ϣ����
//        ptAclMsg:Ҫ�������Ϣ

//ע    ��:insertAclCustom �� insertAclMsg��������
//insertAclMsg �Ѿ�ָ������ϢΪ��׼�� TAclMessage ���˺������Բ��붨�����ͣ�
//��ʹ����Ϣ���й��ܵ�һ����չ,���ʹ�ô����͵�ʱ��ǳ����������ĸ��������
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

	//������Ϣ��--
	ptAclMsgQueue->m_nCurQueMembNum--;

	//�������Ϣ��++
	ptAclMsgQueue->dwHandleMsgCount++;

    unlockLock(ptAclMsgQueue->hQueLock);
    return ACL_ERROR_NOERROR;
}


//start ˫������ ��Ϣ����
//����˵��:������ҪΪ��ϢͶ����ƣ��µ���Ϣ����ͨ����������Գ����µĶ�̬�������Ϣ��Ȼ��Ͷ�ݸ����ζ���
//���������ò�Ҫ��
//��Ϣ���нṹLACP - - - LAEP LACP��Զָ���һ���ڵ㣬LAEP����ָ�����һ���ڵ�
//˫��������һ��
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
    //һ����������������Ϣ���У���ɾ��������Ϣ
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

//����˫������LACP��Զ�ǿ�ͷ��LAEP��Զ������µĽڵ�����Ƕ�̬������ڴ��
//�����ú�������ڵ���ڴ�����Ȩ�ƽ���˫������
//(˫������Ľڵ㽫����뵽instance�Ļ��������У������ɻ���������ò��ͷ��ڴ��)
ACL_API PTQUEUE_MEMBER insertAclDLList(PTMSG_QUEUE ptMsgQueue, void * pContent, int nContentLen)
{
    //Ϊ�˷�ֹ�ڴ�Ƶ�������ͷţ��Ժ���Ҫ�����ڴ�ز�
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
        //�����½ڵ㵽��󣬽�LAEP��Ϊ�²���ڵ�
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
    //lockLock(ptMsgQueue->hQueLock);
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

    //unlockLock(ptMsgQueue->hQueLock);
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

//end ˫������ ��Ϣ����
#ifdef  _MSC_VER

#endif