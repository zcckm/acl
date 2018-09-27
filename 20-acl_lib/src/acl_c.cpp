//******************************************************************************
//模块名	： acl_c
//文件名	： acl_c.c
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//acl开放接口实现
//------------------------------------------------------------------------------
//修改记录:
//2014-12-30 zcckm 创建
//2015-09-04 zcckm 修改 封装各个模块提供的功能，作为acl对外接口
//******************************************************************************
#include <stdio.h>
#include "acl_c.h"
#include "acl_manage.h"
#include "acl_telnet.h"
#include "acl_socket.h"
#include "acl_time.h"
//#define __STACK_PRINT__
#ifdef __STACK_PRINT__
#include <execinfo.h>

H_ACL_LOCK hLock;
#endif
u32 MAKEID(u16 wApp,u16 wInstance)
{
	return ((wApp << 16) + wInstance);
}

u16 getAppID(u32 dwID)
{
	return (u16)(dwID >> 16);
}

u16 getInsID(u32 dwID)
{
	return (u16)dwID;
}

//manage

ACL_API int aclCreateApp(u16 nNewAppID,const s8 * pAppName,int nInstNum,int nInstStackSize, CBMsgEntry pfMsgCB)
{
    if ( !(nNewAppID > 0 && nNewAppID <= MAX_APP_NUM) )
    {
        return ACL_ERROR_PARAM;
    }
    return aclCreateApp__(nNewAppID,pAppName,nInstNum,nInstStackSize, pfMsgCB);
}
//manage

ACL_API int aclCreateApp1(u16 nNewAppID, const s8 * pAppName, int nInstNum, int nMsgQueueNum, CBMsgEntry pfMsgCB)
{
	if (!(nNewAppID > 0 && nNewAppID <= MAX_APP_NUM))
	{
		return ACL_ERROR_PARAM;
	}
	TAclAppParam tAclAppParam;
	memset(&tAclAppParam, 0, sizeof(TAclAppParam));
	strcpy(tAclAppParam.m_achAppName, pAppName);
	tAclAppParam.m_wAppId = nNewAppID;
	tAclAppParam.m_byAppPrity = 80;
	tAclAppParam.m_wAppMailBoxSize = nMsgQueueNum;
	tAclAppParam.m_dwInstStackSize = 8 * 1024 * 1024;
	tAclAppParam.m_dwInstNum = nInstNum;
	tAclAppParam.m_dwInstDataLen = 256;
	tAclAppParam.m_pMsgCB = pfMsgCB;
	tAclAppParam.m_pExtAppData = NULL;

	
	return aclCreateApp__b(&tAclAppParam);
}

// socket
ACL_API int aclTCPConnect(s8 * pNodeIP, u16 wPort)
{
    if (0 == wPort || NULL == pNodeIP)
    {
        return ACL_ERROR_PARAM;
    }
    return aclTCPConnect__(pNodeIP,  wPort);
}

//close connect Socket
ACL_API int aclConnClose(int nNode)
{
	if (nNode <= 0)
	{
		return ACL_ERROR_PARAM;
	}
	
	return aclConnClose__(nNode);
}

ACL_API ACL_HANDLE setTimer_b(int nTime, 
    u32 dwAppInsAddr,
    u16 wMsgType, 
    BOOL32 bRepeat, 
    PF_TIMER_NTF pfTimerNtf, 
    void * pContent, 
    int nContentLen)
{
    if (NULL != pfTimerNtf && wMsgType < ACL_USER_MSG_BASE)
    {
        return NULL;
    }
    return setTimer_b__(nTime, dwAppInsAddr, wMsgType, bRepeat, pfTimerNtf,  pContent, nContentLen);
}

ACL_API ACL_HANDLE setTimer(int nTime, u32 wAppInsAddr, u16 wMsgType)
{
    if (wMsgType <= ACL_USER_MSG_BASE)
    {
        return NULL;
    }
    return setTimer__(nTime, wAppInsAddr, wMsgType);
}
//message

ACL_API int aclInstPost(HACLINST hInst,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen)
{
    if (wMsgType < ACL_USER_MSG_BASE)//user message number must large than ACL_USER_MSG_BASE
    {
        return ACL_ERROR_FILTER;
    }
    return aclInstPost__(hInst, dwDstAppInstAddr, dwNodeID, wMsgType, pContent, dwContentLen);
}

ACL_API int aclPost(u32 dwSrcAppInstAddr,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen)
{
    if (wMsgType < ACL_USER_MSG_BASE)//user message number must large than ACL_USER_MSG_BASE
    {
        return ACL_ERROR_FILTER;
    }
    return aclPost__(dwSrcAppInstAddr, dwDstAppInstAddr, dwNodeID, wMsgType, pContent, dwContentLen);
}
#ifdef __STACK_PRINT__
void print_stacktrace(int* a, int flag)
{
	int size = 16;
	void * array[16];
	int stack_num = backtrace(array, size);
	
	char ** stacktrace = backtrace_symbols(array, stack_num);
	//pthread_mutex_lock(&hLock);
// 	if (flag != 2)
// 	{
// 		printf("%s value %X \n", flag ? "unlock" : "lock", a);
// 	}
// 	else
// 	{
// 		printf("%s value %X \n", "deadlock", a);
// 	}


	for (int i = 0; i < stack_num; ++i)
	{
		printf("%s\n", stacktrace[i]);
	}
	//pthread_mutex_unlock(&hLock);
	free(stacktrace);
}
#endif