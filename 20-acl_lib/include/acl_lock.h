//******************************************************************************
//模块名	： acl_lock
//文件名	： acl_lock.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//封装线程锁
//------------------------------------------------------------------------------
//修改记录:
//2015-01-16 zcckm 创建
//******************************************************************************
#ifndef _ACL_LOCK_H_
#define _ACL_LOCK_H_

#include "acl_c.h"
#ifdef WIN32
#include <windows.h>
#endif

// #ifdef __cplusplus
// extern "C" {
// #endif //extern "C"

//mutex
int aclCreateLock(H_ACL_LOCK * phAclLock, ACL_LOCK_ATTR * pAclLockAttr);

int aclDestoryLock(H_ACL_LOCK  hAclLock);

int lockLock_t(H_ACL_LOCK  hAclLock,u32 dwMaxWaitTime);

int lockLock(H_ACL_LOCK  hAclLock);

int unlockLock(H_ACL_LOCK  hAclLock);


//Semaphore
int aclCreateSem(H_ACL_SEM * phAclSem, int nMaxSemNum);

int aclDestorySem(H_ACL_SEM * phAclSem);

int aclCreateSem_i(H_ACL_SEM * phAclSem,int bInitSemNum, int nMaxSemNum);

int aclCreateSem_1(H_ACL_SEM * phAclSem);

int aclCreateSem_b(H_ACL_SEM * phAclSem, ACL_SEM_ATTR * pAclSemAttr,int bInitSemNum, int nMaxSemNum, const char * pSemName);

int aclCheckGetSem_b(H_ACL_SEM * phAclSem,u32 nMaxWaitTime);

int alcCheckGetSem(H_ACL_SEM * phAclSem);

int aclDestorySem(H_ACL_SEM * phAclSem);

int aclReleaseSem_b(H_ACL_SEM * phAclSem,int nReleaseSemNum);

int aclReleaseSem(H_ACL_SEM * phAclSem);

// #ifdef __cplusplus
// }
// #endif //extern "C"

#define DEF_NAME(n) #n

#endif

