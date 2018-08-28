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
#ifndef _ACL_TASK_H_
#define _ACL_TASK_H_

#include "acl_common.h"


#define WAIT_FOR_EXIT(v) v = E_TASK_ASK_FOR_EXIT; while(E_TASK_ALREADY_EXIT != v){aclDelay(1);} 

// #ifdef __cplusplus
// extern "C" {
// #endif //extern "C"

ACL_API int aclCreateThread_b(PTACL_THREAD ptAclThread, PTTHREAD_PARAM ptThreadParam, PF_THREAD_ENTRY pfTaskEntry,void * pTaskParam);
ACL_API int aclDestoryThread(H_ACL_THREAD hThread);
ACL_API int aclCreateThread(PTACL_THREAD ptAclThread, PF_THREAD_ENTRY pfTaskEntry,void * pTaskParam);

// #ifdef __cplusplus
// }
// #endif //extern "C"

#endif

