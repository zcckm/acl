//******************************************************************************
//模块名	： acl_time
//文件名	： acl_time.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL 时间相关
//------------------------------------------------------------------------------
//修改记录:
//2015-01-23 zcckm 创建
//******************************************************************************
#ifndef _ACL_TIME_H_
#define _ACL_TIME_H_

#include "acltype.h"
#include "acl_common.h"

// #ifdef __cplusplus
// extern "C" {
// #endif //extern "C"


//++++++++ Timer Interface
ACL_API void aclDelay(u32 dwMsecs);

ACL_API ACL_HANDLE setTimer_b__(int nTime, 
    u32 dwAppInsAddr,
    u16 wMsgType, 
    BOOL32 bRepeat, 
    PF_TIMER_NTF pfTimerNtf, 
    void * pContent, 
    int nContentLen);

ACL_API ACL_HANDLE setTimer__(int nTime, u32 wAppInsAddr, u16 wMsgType);

ACL_API int killTimer(u16 wMsgType);
//-------- Timer Interface

// #ifdef __cplusplus
// }
// #endif //extern "C"

#endif

