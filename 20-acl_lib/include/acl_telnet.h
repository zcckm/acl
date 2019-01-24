//******************************************************************************
//模块名	： acl_telnet
//文件名	： acl_telnet.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL telnet 调试
//------------------------------------------------------------------------------
//修改记录:
//2015-01-20 zcckm 创建
//******************************************************************************
#ifndef _ACL_TELNET_H_
#define _ACL_TELNET_H_

#include "acl_c.h"

// #ifdef __cplusplus
// extern "C" {
// #endif //extern "C"

//++++++++Telnet Interface
ACL_API int aclPrintf(int bOpenPrint,int bSaveLog,const char * param,...);
ACL_API int aclRegCommand(const char * pCmd, void * pFunc, const char * pPrompt);
ACL_API int setTelnetPrompt(int nPort, const char * pPrompt);
//--------Telnet Interface



ACL_API int ACL_DEBUG(int MODULE, int TYPE,const char * param,...);

//Debug info filter

/*
打印级别设置：
1.根据模块打印(管理，Socket，内存，锁等)
2.根据信息类型打印(调试信息：无，调试,通知，警告，错误，)
通过telnet调试设置
*/
typedef enum
{
    E_TYPE_NONE = 0,
	E_TYPE_ERROR,
	E_TYPE_WARNNING,
	E_TYPE_KEY,
    E_TYPE_NOTIF,    
	E_TYPE_DEBUG,
    E_TYPE_RESERVED_0,
    E_TYPE_RESERVED_1,
    E_TYPE_ALL
}EDEBUG_TYPE;

typedef enum
{
    E_MOD_NONE = 0,
    E_MOD_LOCK,//锁
    E_MOD_NETWORK,//网络收发相关，偏向于Socket
    E_MOD_HB,//心跳
    E_MOD_MSG,//message,消息收发逻辑相关
    E_MOD_MANAGE,//管理
    E_MOD_TELNET,//telnet
    E_MOD_TIMER,//定时器
    E_MOD_ALL //所有模块
}EDEBUG_MODULE;


// #ifdef __cplusplus
// }
// #endif //extern "C"

#endif

