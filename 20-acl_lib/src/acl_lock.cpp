//******************************************************************************
//ģ����	�� acl_lock
//�ļ���	�� acl_lock.c
//����	�� zcckm
//�汾	�� 1.0
//�ļ�����˵��:
//��װ�߳���
//------------------------------------------------------------------------------
//�޸ļ�¼:
//2015-01-16 zcckm ����
//2015-01-21 �����ź�������֧��
//******************************************************************************

#include "acltype.h"
#include "acl_lock.h"
#include "acl_telnet.h"
#include "acl_common.h"
#ifdef _LINUX_
#include <pthread.h>
#include <semaphore.h>
#endif


int aclCreateLock(H_ACL_LOCK * phAclLock, ACL_LOCK_ATTR * pAclLockAttr)
{
    if (NULL == phAclLock)
    {
        return -1;
    }
#ifdef _LINUX_
        pthread_mutex_init(phAclLock,NULL);
#elif defined WIN32
    * phAclLock = CreateMutex(pAclLockAttr, FALSE, NULL);
#endif
    return ACL_ERROR_NOERROR;
}

int aclDestoryLock(H_ACL_LOCK  & hAclLock)
{
#ifdef WIN32
    if (NULL == hAclLock)
    {
        return ACL_ERROR_INVALID;
    }
#endif

    
#ifdef WIN32
    return CloseHandle(hAclLock);
#elif defined( _LINUX_)
    return pthread_mutex_destroy(&hAclLock);
#endif
    
}

//=============================================================================
//�� �� ����lockLock_t
//��	  �ܣ� ��������
//�㷨ʵ�֣�windows ���� WaitForSingleObject 
//         linux������û�еȴ����ƣ���ǰ������
//ע    ��:
//=============================================================================
int lockLock_t(H_ACL_LOCK  & hAclLock,u32 dwMaxWaitTime)
{    
#ifdef WIN32
    if (NULL == hAclLock)
    { 
        return ACL_ERROR_INVALID;
    }
    return  WaitForSingleObject(hAclLock,dwMaxWaitTime);
#elif defined (_LINUX_)
    if(ACL_ERROR_NOERROR != pthread_mutex_trylock(&hAclLock))
    {
        usleep(dwMaxWaitTime*1000);
        return pthread_mutex_trylock(&hAclLock);
    }
    return ACL_ERROR_NOERROR;
#endif
}

int lockLock(H_ACL_LOCK   & hAclLock)
{
#ifdef WIN32
    if (NULL == hAclLock)
    {
        return ACL_ERROR_INVALID;
    }
    /*
    dwRet = WaitForSingleObject(hAclLock,1000);
    if (WAIT_TIMEOUT == dwRet)// time is up
    {
        aclPrintf(TRUE,FALSE,"[lock lock] lock time Out\n");
    }
    return ACL_ERROR_NOERROR;
    */
    return  WaitForSingleObject(hAclLock,INFINITE);
#elif defined (_LINUX_)
    return pthread_mutex_lock(&hAclLock);
#endif

	return ACL_ERROR_NOERROR;
}

int unlockLock(H_ACL_LOCK  & hAclLock)
{
#ifdef WIN32
    if (NULL == hAclLock)
    {
        return ACL_ERROR_INVALID;
    }
    return ReleaseMutex(hAclLock);
#elif defined (_LINUX_)
    return pthread_mutex_unlock(&hAclLock);
#endif
}

//�ź���

//����һ���ź���������ֻ��һ���ź�
int aclCreateSem_1(H_ACL_SEM * phAclSem)
{
    return aclCreateSem_b(phAclSem,NULL,0,1,NULL);
}


int aclCreateSem(H_ACL_SEM * phAclSem, int nMaxSemNum)
{
    return aclCreateSem_b(phAclSem,NULL,0,nMaxSemNum,NULL);
}

int aclCreateSem_i(H_ACL_SEM * phAclSem,int bInitSemNum, int nMaxSemNum)
{
    return aclCreateSem_b(phAclSem,NULL,bInitSemNum,nMaxSemNum,NULL);
}

//=============================================================================
//�� �� ����aclCheckGetSem_b
//��	    �ܣ� �����źŵ�
//�㷨ʵ�֣�
//��    ���� int bInitSemNum ��ʼ���ж����źŵ����ź�
//           int nMaxSemNum  ���ڵ�����źŵ�
//
//ע    �⣺
//=============================================================================
int aclCreateSem_b(H_ACL_SEM * phAclSem, ACL_SEM_ATTR * pAclSemAttr,int bInitSemNum, int nMaxSemNum, const char * pSemName)
{
    u32 nRet = ACL_ERROR_NOERROR;
    int i = 0;
    CHECK_NULL_RET_INVALID(phAclSem);

#ifdef WIN32
    * phAclSem = CreateSemaphore(pAclSemAttr, bInitSemNum, nMaxSemNum, (LPCTSTR)pSemName);
#elif defined (_LINUX_)
    if (NULL == pSemName)
    {
        //sem_init support anonymous object
    }
    nRet = sem_init(phAclSem, 0, nMaxSemNum);
    if (ACL_ERROR_NOERROR != nRet)
    {
        ACL_DEBUG(E_MOD_LOCK, E_TYPE_ERROR, "[aclCreateSem_b] aclCreateSem_b failed in _LINUX_ mode\n");
        return nRet;
    }
    //���ճ�ʼ�������ص��źŵ�
    for (i = 0; i < nMaxSemNum - bInitSemNum; i++)
    {
        sem_wait(phAclSem);
    }
#endif
    return ACL_ERROR_NOERROR;
}


int aclDestorySem(H_ACL_SEM * phAclSem)
{
#ifdef WIN32
    CHECK_NULL_RET_INVALID(phAclSem)
    return CloseHandle(* phAclSem);
#elif defined(_LINUX_)
    return sem_destroy(phAclSem);
#endif
}


//=============================================================================
//�� �� ����aclCheckGetSem_b
//��	  �ܣ� ���Ի���ź���������޷������ �ȴ� nMaxWaitTime ���� Ȼ��ʱ����
//�㷨ʵ�֣�windows ���� WaitForSingleObject 
//         linux   ���� sem_timedwait
//ע    ��:
//=============================================================================
int aclCheckGetSem_b(H_ACL_SEM * phAclSem,u32 nMaxWaitTime)
{
    u32 dwRet = 0;
#ifdef _LINUX_
    struct timespec ts;
    int nRet = 0;
    struct timeval tv;
#endif

#ifdef WIN32
    CHECK_NULL_RET_INVALID(* phAclSem)
    dwRet = WaitForSingleObject(* phAclSem,nMaxWaitTime);
    switch (dwRet)
    {
    case WAIT_OBJECT_0:
        {
            return ACL_ERROR_NOERROR;
        }
        break;
    case WAIT_TIMEOUT:
        {
            return ACL_ERROR_TIMEOUT;
        }
        break;
    default:
        ACL_DEBUG(E_MOD_LOCK, E_TYPE_ERROR,"[aclCheckGetSem_b]unknown Wait Signal %d", dwRet);
        return ACL_ERROR_INVALID;
    }
#elif defined(_LINUX_)
    //ϣ�����õȴ��ź�
    if (INFINITE == nMaxWaitTime)
    {
        dwRet = sem_wait(phAclSem);
        return ACL_ERROR_NOERROR;
    }
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = (tv.tv_usec  + 990 * 1000) * 1000;
    ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
    ts.tv_nsec %= 1000 * 1000 * 1000;

    nRet = sem_timedwait(phAclSem, &ts);
    switch (nRet)
    {
    case 0:
        {
            return ACL_ERROR_NOERROR;
        }
        break;
    case -1:
        {
            return ACL_ERROR_TIMEOUT;
        }
        break;
    default:
        ACL_DEBUG(E_MOD_LOCK, E_TYPE_ERROR, "[acl_lock]unknown Wait Signal %d\n", dwRet);
        return ACL_ERROR_INVALID;
    }
#endif
    return ACL_ERROR_TIMEOUT;
}


//=============================================================================
//�� �� ����alcCheckGetSem
//��	  �ܣ� ���Ի���ź���������޷������ �����ȴ�
//�㷨ʵ�֣�aclCheckGetSem_b
//ע    ��:
//=============================================================================
int alcCheckGetSem(H_ACL_SEM * phAclSem)
{
    return aclCheckGetSem_b(phAclSem,INFINITE);
}


int aclReleaseSem_b(H_ACL_SEM * phAclSem,int nReleaseSemNum)
{
    int i = 0;
#ifdef WIN32
    CHECK_NULL_RET_INVALID(phAclSem)
    ReleaseSemaphore(* phAclSem,nReleaseSemNum,NULL);
    return ACL_ERROR_NOERROR;
#elif defined (_LINUX_)
    for (i = 0; i < nReleaseSemNum; i++)
    {
        sem_post(phAclSem);
    }
    return ACL_ERROR_NOERROR;
#endif

}

int aclReleaseSem(H_ACL_SEM * phAclSem)
{
    return aclReleaseSem_b(phAclSem,1);
}