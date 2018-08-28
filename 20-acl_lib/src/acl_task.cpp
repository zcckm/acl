//******************************************************************************
//模块名	： acl_task
//文件名	： acl_task.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//对线程接口进行封装,在移植到相应OS时需要实现列表定义的函数
//------------------------------------------------------------------------------
//修改记录:
//2014-12-30 zcckm 创建
//******************************************************************************
#include "acl_task.h"
#include "acl_msgqueue.h"

#ifdef WIN32
#include<process.h>
#endif


typedef struct tagTaskParam
{
    void * pContent;
    u32 dwContentLen;
    u32 dwReserved;
    void *(*pTaskRunning)(void *);
}TTASK_PARAM;


ACL_API int aclCreateThread_b(PTACL_THREAD ptAclThread, PTTHREAD_PARAM ptThreadParam, PF_THREAD_ENTRY pfTaskEntry,void * pTaskParam)
{
    CHECK_NULL_RET_INVALID(ptAclThread)

#ifdef WIN32
    // Create thread
    ptAclThread->hThread  = (HANDLE)_beginthreadex( NULL, 
    ptThreadParam ? ptThreadParam->dwStackSize : 0, 
    pfTaskEntry, 
    pTaskParam,
    ptThreadParam ? ptThreadParam->nThreadRun : 0,
    &ptAclThread->dwThreadID);

#elif defined (_LINUX_)
    if(pthread_create(&ptAclThread->hThread, NULL, pfTaskEntry, pTaskParam))
    {  
        return ACL_ERROR_FAILED;
    }  
#endif
    return 0;
}

ACL_API int aclCreateThread(PTACL_THREAD ptAclThread, PF_THREAD_ENTRY pfTaskEntry,void * pTaskParam)
{
    PTTHREAD_PARAM ptThreadParam = NULL;
	CHECK_NULL_RET_INVALID(ptAclThread)

    return aclCreateThread_b(ptAclThread, ptThreadParam, pfTaskEntry, pTaskParam);
}

ACL_API int aclDestoryThread(H_ACL_THREAD hThread)
{
#ifdef WIN32
    CHECK_NULL_RET_INVALID(hThread)
    if (CloseHandle(hThread))
    {
        return 0;
    }
#elif defined(_LINUX_)
//linux下线程资源销毁暂时不添加
#endif
    return ACL_ERROR_FAILED;
}