//******************************************************************************
//模块名	： acl_manage
//文件名	： acl_manage.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//管理ACL相关资源,消息资源，连接资源
//------------------------------------------------------------------------------
//修改记录:
//2015-01-16 zcckm 创建
//******************************************************************************

#include "acl_manage.h"
#include "acl_task.h"
#include "acl_msgqueue.h"
#include "acl_memory.h"
#include "acl_lock.h"
#include "acl_telnet.h"
#include "acl_time.h"
#include "acl_socket.h"

//全局资源定义区
//->begin
#define ACL_INVALID_APPID (u16)-1 //invalid ID as init value
#define ACL_INVALID_APPPOS -1 //invalid appPos as init value
#define ACL_APPID 0     //g_nAclManageNum[i][ACL_APPID]//appID
#define ACL_APPID_POS 1 //g_nAclManageNum[i][ACL_APPID_POS]//appPos

//->end
//全局资源定义区

//函数声明
//->begin

//inner
ACL_API void cbNtWkMsgHd(TAclMessage *ptMsg, HACLINST hInst);
ACL_API void cbHBDetect(TAclMessage *ptMsg, HACLINST hInst);
ACL_API void aclShowApp();

//timer
ACL_API void initTimer();// inner use only
ACL_API int unInitTimer();
ACL_API u32 aclGetTickCount();

//socket
ACL_API void aclShowNode();

//telnet
ACL_API int aclTelnetInit(BOOL bTelnet, u16 wPort);
ACL_API int aclTelnetExit();

//manage

//标准Instance结构体
typedef struct tagAclInstance
{
    int m_nInstState;
    int m_nInstID;
    int m_nRootAppID;
    H_ACL_LOCK hInstLock;
    int m_nSyncMsgFlag;
    PTMSG_QUEUE m_ptInstMsgQueue;
    TACL_THREAD m_tInstThread;
}TAclInstance;

//用户创建app参数结构
typedef struct tagAclApp
{
    s8 m_achAppName[MAX_APP_NAME_LEN];      //app名称
    u16 m_wAppId;                           //app 号    
    u8 m_byAppPrity;                        //app优先级。默认为 80
    u16 m_wAppMailBoxSize;                  //app 邮箱最大消息数.默认值为 80
    u32 m_dwInstStackSize;                  //app 堆栈大小.默认为64K.
    u32 m_dwInstNum;                        //承载的实例数目
    u32 m_dwInstDataLen;                    //每个实例自定义数据长度
    CBMsgEntry m_pMsgCB;                    //消息入口回调函数指针
    void * m_pExtAppData;                   //app 相关扩展数据指针
    TAclInstance ** m_pptInst;              //ACL inst指针数组 
    u32 m_dwInstCount;                      //Instance引用计数，创建++删除--
    H_ACL_LOCK m_hAppLock;                    //APP锁
}TAclApp;
typedef struct tagGlbParam TAclGlbParam;
struct tagGlbParam
{
    H_ACL_LOCK m_hAclGlbLock;
	H_ACL_LOCK m_hAclIDLock;
    int m_nAclInit;
    u32 m_dwIDGenerate;
	//socket manage data proc
	HSockManage m_hSockMngData;

	//socket manage 3A
	HSockManage m_hSockMng3A;
	//app
	u16 m_nAclAppIDMap[MAX_APP_NUM][2];
	TAclApp m_tAclApp[MAX_APP_NUM];
	char * m_sendBuf;//as send buffer
};

static TAclGlbParam g_tGlbParam;

ACL_API int aclDestroyAppList();

ACL_API TAclInstance * aclCreateInstance(TAclApp * ptAclApp,int nInstID);

ACL_API int regNewAclAPP(u16 nNewAppID);
ACL_API int unRegAclAPP(u16 nAppID);

ACL_API int aclMsgPushAppInit(int nInstNum ,CBMsgEntry pfMsgCB);
ACL_API int aclMsgPushAppUnInit();
ACL_API int aclHBDetectInit(int nInstNum ,CBMsgEntry pfMsgCB);
ACL_API int aclHBDetectUnInit();
#ifdef WIN32
ACL_API unsigned int __stdcall aclInstanceManageThread(void * param);
#elif defined (_LINUX_)
ACL_API void * aclInstanceManageThread(void * param);
#endif


//->end
//函数声明



ACL_API HSockManage getSockDataManger()
{
	return g_tGlbParam.m_hSockMngData;
}

TAclApp * getAclApp(u16 nNewAppID)
{
    int i = 0;
    for (i = 0; i < MAX_APP_NUM; i++)
    {
        if (nNewAppID == g_tGlbParam.m_nAclAppIDMap[i][ACL_APPID])
        {
            return &g_tGlbParam.m_tAclApp[i];
        }
    }
    return NULL;
}

ACL_API int isAclInited()
{
    return g_tGlbParam.m_nAclInit?TRUE:FALSE;
}

//=============================================================================
//函 数 名：aclInit
//功	    能：acl模块初始化
//算法实现：
//全局变量：
//参	    数：bTelnet： 是否启动telnet(仅Windows上有效)
//          wTelPort: telnet监听端口
//注    意: 使用ACL前必须调用此函数用于初始化各个模块
//          
//=============================================================================
ACL_API int aclInit( BOOL bTelnet, u16 wTelPort )
{
	int nRet = 0;
	HSockManage hSockMng_data = NULL;
	HSockManage hSockMng_3A = NULL;
	#ifdef __STACK_PRINT__
	aclCreateLock(&hLock, NULL);
	#endif

#ifdef WIN32
	int ret;
	WORD ver;
	WSADATA data;
	ver = MAKEWORD( 2, 2 );
	ret = WSAStartup( ver, &data );
#endif

    if (g_tGlbParam.m_nAclInit)
    {
        return ACL_ERROR_OVERFLOW;
    }
	memset(g_tGlbParam.m_nAclAppIDMap, -1, sizeof(g_tGlbParam.m_nAclAppIDMap));
	//this lock is for glb operate
    if(ACL_ERROR_NOERROR != aclCreateLock(&g_tGlbParam.m_hAclGlbLock, NULL))
    {
        return ACL_ERROR_INVALID;
    } 

	//this lock is for get glb ID
	if(ACL_ERROR_NOERROR != aclCreateLock(&g_tGlbParam.m_hAclIDLock, NULL))
	{
		return ACL_ERROR_INVALID;
	} 
	

    lockLock(g_tGlbParam.m_hAclGlbLock);
    
	//init data proc sockManage
	hSockMng_data = aclSocketManageInit(E_DATA_PROC);

	if (NULL == hSockMng_data)
	{
		unlockLock(g_tGlbParam.m_hAclGlbLock);
		return ACL_ERROR_INIT;
	}
	g_tGlbParam.m_hSockMngData = hSockMng_data;

	//init connect 3A sockManage thread
	hSockMng_3A = aclSocketManageInit(E_3A_CONNECT);
	if (NULL == hSockMng_3A)
	{
		unlockLock(g_tGlbParam.m_hAclGlbLock);
		return ACL_ERROR_INIT;
	}
	g_tGlbParam.m_hSockMng3A = hSockMng_3A;

	//alloc socket send buffer
    if (NULL == g_tGlbParam.m_sendBuf)
    {
        g_tGlbParam.m_sendBuf = (char *)aclMallocClr(MAX_SEND_PACKET_SIZE);
    }
	
	if (NULL == g_tGlbParam.m_sendBuf)
	{
		unlockLock(g_tGlbParam.m_hAclGlbLock);
		return ACL_ERROR_INIT;
	}

	aclMsgPushAppInit(NETWORK_MSG_PUSH_INST_NUM, cbNtWkMsgHd);

	aclHBDetectInit(HB_DETECT_INST_NUM, cbHBDetect);
	
	//acl Telnet Init
	nRet = aclTelnetInit(bTelnet, wTelPort);
    
	if (ACL_ERROR_NOERROR != nRet)
	{
        ACL_DEBUG(E_MOD_MANAGE,E_TYPE_ERROR,"[aclInit] aclTelnetInit failed %d\n", nRet);
		unlockLock(g_tGlbParam.m_hAclGlbLock);
		return nRet;
	}

    //init acl timer manager
    initTimer();
   
    g_tGlbParam.m_nAclInit = TRUE;
    unlockLock(g_tGlbParam.m_hAclGlbLock);
    aclRegCommand("showapp", (void *)aclShowApp, "show all App");
    aclRegCommand("shownode", (void *)aclShowNode, "show all Node Info");
	ACL_DEBUG(E_MOD_MANAGE,E_TYPE_NOTIF,"[aclInit] acl project init\n");
    return ACL_ERROR_NOERROR;
}

//=============================================================================
//函 数 名：aclQuit
//功	    能：acl模块退出
//算法实现：
//全局变量：
//参	    数：
//注    意: 在退出ACL时必须调用此函数，释放所有资源
//          
//=============================================================================
ACL_API void aclQuit()
{
    if (FALSE == isAclInited())
    {
        return;
    }

    aclSocketManageUninit(g_tGlbParam.m_hSockMngData, E_DATA_PROC);

    aclSocketManageUninit(g_tGlbParam.m_hSockMng3A, E_3A_CONNECT);

    aclMsgPushAppUnInit();
    aclHBDetectUnInit();

    if (g_tGlbParam.m_sendBuf)
    {
        aclFree(g_tGlbParam.m_sendBuf);
        g_tGlbParam.m_sendBuf = NULL;
    }
    
    aclTelnetExit();

    unInitTimer();

    aclDestroyAppList();

    aclDestoryLock(g_tGlbParam.m_hAclGlbLock);

    aclDestoryLock(g_tGlbParam.m_hAclIDLock);

    g_tGlbParam.m_nAclInit = FALSE;
}

void aclShowApp()
{
    int i = 0;
    aclPrintf(TRUE, FALSE, "***********************ACL APP***********************\n");
    for (i = 0; i < MAX_APP_NUM; i++)
    {
        if (ACL_INVALID_APPID != g_tGlbParam.m_nAclAppIDMap[i][ACL_APPID])
        {
            aclPrintf(TRUE, FALSE, "APP[%d], NAME= \"%s\" InstNum= %d\n", 
                g_tGlbParam.m_nAclAppIDMap[i][ACL_APPID],
                g_tGlbParam.m_tAclApp[i].m_achAppName,
                g_tGlbParam.m_tAclApp[i].m_dwInstNum);
        }
        
    }
    aclPrintf(TRUE, FALSE, "***********************ACL APP***********************\n");
    
}

//=============================================================================
//函 数 名：cbNtWkMsgHd
//功	  能： 网络数据发送回调
//算法实现：依托APP管理
//注    意:
//=============================================================================
void cbNtWkMsgHd(TAclMessage *ptMsg, HACLINST hInst)
{
	//处理即将发送的消息
	char szSndData[MAX_SEND_PACKET_SIZE] = {0};
	TAclMessage * ptAclMsg = NULL;
	ptAclMsg = (TAclMessage *)szSndData;
	//regroup send Packet
	if (ptAclMsg->m_dwPackLen > MAX_SEND_PACKET_SIZE)
	{
		//ATTENTION 超过发送buffer
		//暂时不支持分包发送，后续可以考虑支持
		//return ACL_ERROR_NOTSUP;
        ACL_DEBUG(E_MOD_NETWORK,E_TYPE_WARNNING,"[cbNtWkMsgHd] not support long packet send now\n");
		return;
	}
	memcpy(ptAclMsg, ptMsg, sizeof(TAclMessage));
	memcpy(szSndData + sizeof(TAclMessage), ptAclMsg->m_pContent, ptAclMsg->m_dwContentLen);
	ptAclMsg->m_pContent = NULL;
    ACL_DEBUG(E_MOD_NETWORK,E_TYPE_DEBUG,"[cbNtWkMsgHd] handle network message now NETID:%d MSGID:%d PACKLen:%d\n",
        ptMsg->m_dwSessionID,ptMsg->m_wMsgType,ptAclMsg->m_dwPackLen);

	aclTcpSendbyNode(g_tGlbParam.m_hSockMngData, ptMsg->m_dwSessionID, szSndData, ptAclMsg->m_dwPackLen);
}

//=============================================================================
//函 数 名：cbHBDetect
//功	  能： 网络会话心跳检测回调
//算法实现：依托APP管理
//注    意:
//=============================================================================
void cbHBDetect(TAclMessage *ptMsg, HACLINST hInst)
{
	//收到心跳包并回复(Client)
	//或者收到发送心跳包命令(Server)
	char szSndData[MAX_SEND_PACKET_SIZE] = {0};
	TAclMessage * ptAclMsg = NULL;
	ptAclMsg = (TAclMessage *)szSndData;
    ACL_DEBUG(E_MOD_HB,E_TYPE_DEBUG, "[cbHBDetect] DetectRecv  SID:%d MSG:%d\n",ptMsg->m_dwSessionID,ptMsg->m_wMsgType);
	switch(ptMsg->m_wMsgType)
	{
	case MSG_HBMSG_REQ://req Send HB Msg
		{
			//send a HB Packet to pointed session
            ACL_DEBUG(E_MOD_HB,E_TYPE_DEBUG, "[cbHBDetect] has recv local msg ACL_MSG_HBMSG_REQ SID:%d\n",ptMsg->m_dwSessionID);
			aclMsgPush(0, MAKEID(HBDET_APP_NUM, INST_SEEK_IDLE), ptMsg->m_dwSessionID, MSG_HB_MSG, NULL, 0, E_PMT_L_N);
		}
		break;
	case MSG_HB_MSG://Recv a HB Msg
		{
			ACL_DEBUG(E_MOD_HB,E_TYPE_DEBUG,"[cbHBDetect] has recv msg ACL_MSG_HB_MSG SID:%d\n",ptMsg->m_dwSessionID);
			//接收会话心跳，及时反馈
			aclHBConfirm(g_tGlbParam.m_hSockMngData, ptMsg->m_dwSessionID);
			aclMsgPush(0, MAKEID(HBDET_APP_NUM,INST_SEEK_IDLE), ptMsg->m_dwSessionID, MSG_HB_MSG_ACK, NULL, 0, E_PMT_L_N);
		}
		break;
	case MSG_HB_MSG_ACK://Recv a HB Msg
		{
			//节点会话心跳返回，累减超时信号
			aclHBConfirm(g_tGlbParam.m_hSockMngData, ptMsg->m_dwSessionID);
			ACL_DEBUG(E_MOD_HB,E_TYPE_DEBUG, "[cbHBDetect] has recv HB_MSG_ACK SID:%d\n",ptMsg->m_dwSessionID);

		}
		break;
	default:
		{
            ACL_DEBUG(E_MOD_HB,E_TYPE_WARNNING, "[cbHBDetect] unknown detect msg:%d\n", ptMsg->m_wMsgType);
		}
		break;
	}
	ptAclMsg->m_pContent = NULL;
}

//=============================================================================
//函 数 名：aclMsgPushAppInit
//功	    能：消息推送APP的创建
//算法实现：
//全局变量：
//参	    数：nInstNum： 消息推送APP的Instance数量
//           pfMsgCB: 消息推送APP的注册函数
//注    意: 此函数用于管理推送所有发往网络上的消息
//=============================================================================
ACL_API int aclMsgPushAppInit(int nInstNum ,CBMsgEntry pfMsgCB)
{
	static int bInit = FALSE;
	int nRet = 0;
	if (bInit)
	{
		return ACL_ERROR_OVERFLOW;
	}
	if (0 == nInstNum)
	{
		//not create network model
        ACL_DEBUG(E_MOD_MANAGE,E_TYPE_ERROR,"[aclMsgPushAppInit] should have at least one instance\n");
		return ACL_ERROR_NOERROR;
	}
	nRet = aclCreateApp__(NWPUSH_APP_NUM, "NET WORK APP", nInstNum , 0, pfMsgCB);
	if (ACL_ERROR_NOERROR == nRet)
	{
		bInit = TRUE;
	}
	return nRet;
}

//=============================================================================
//函 数 名：aclMsgPushAppUnInit
//功	    能：销毁消息推送管理APP
//算法实现：
//注    意：仅内部使用且调用一次
//=============================================================================
ACL_API int aclMsgPushAppUnInit()
{
    int nRet = 0;
    nRet = aclDestroyApp(NWPUSH_APP_NUM);    
    return nRet;
}

//=============================================================================
//函 数 名：aclHBDetectInit
//功	    能：创建心跳检测APP
//算法实现：
//全局变量：
//参	    数：nInstNum： 心跳检测APP的Instance数量
//           pfMsgCB: 心跳检测APP的注册函数
//注    意: 此函数用于检测两个APP间连接状态，多个Instance是为了增加并发处理能力
//=============================================================================
ACL_API int aclHBDetectInit(int nInstNum ,CBMsgEntry pfMsgCB)
{
	static int bInit = FALSE;
	int nRet = 0;
	if (bInit)
	{
		return ACL_ERROR_OVERFLOW;
	}
	if (0 == nInstNum)
	{
		//not create network model
		ACL_DEBUG(E_MOD_MANAGE,E_TYPE_ERROR,"[aclHBDetectInit] should have at least one instance\n");
		return ACL_ERROR_NOERROR;
	}
	nRet = aclCreateApp__(HBDET_APP_NUM, "HB Detect APP", nInstNum , 0, pfMsgCB);
	if (ACL_ERROR_NOERROR == nRet)
	{
		bInit = TRUE;
	}
	return nRet;
}

//=============================================================================
//函 数 名：aclHBDetectUnInit
//功	    能：销毁心跳管理APP
//算法实现：
//注    意：
//=============================================================================
ACL_API int aclHBDetectUnInit()
{
    int nRet = 0;
    nRet = aclDestroyApp(HBDET_APP_NUM);
    return nRet;
}

//=============================================================================
//函 数 名：aclCreateApp
//功	    能：创建APP
//算法实现：
//全局变量：
//参	    数：nNewAppID： 创建的APPID
//           pAppName: APP名称 可为空
//           nInstNum: 包含Instance数量
//     nInstStackSize: Instance 线程的堆栈大小
//            pfMsgCB: APP的Entry函数，APP接收的消息都会通知此函数
//注    意:APP中包含的每个Instance都有独立的处理线程，各个Instance独立处理各自消息
//         每个Instance都有自己的状态机，可以在注册函数中修改
//=============================================================================
ACL_API int aclCreateApp__(u16 nNewAppID,const s8 * pAppName,int nInstNum,int nInstStackSize, CBMsgEntry pfMsgCB)
{
    TAclApp * ptAclApp = NULL;
	int nErrCode = 0;
    nErrCode = regNewAclAPP(nNewAppID);
	if (nErrCode <0)
	{
        ACL_DEBUG(E_MOD_MANAGE,E_TYPE_ERROR,"[aclCreateApp]create App failed EC:%d\n",nErrCode);
		return nErrCode;
	}
    ptAclApp = getAclApp(nNewAppID);
    if (ptAclApp == NULL)
    {
        return ACL_ERROR_CONFLICT;
    }
    if (ACL_ERROR_INVALID == aclCreateLock(&ptAclApp->m_hAppLock,NULL))
    {
        return ACL_ERROR_FAILED;
    }
    memset(ptAclApp->m_achAppName, 0, MAX_APP_NAME_LEN);
    
    strncpy(ptAclApp->m_achAppName, pAppName, strlen(pAppName) > MAX_APP_NAME_LEN -1 ? MAX_APP_NAME_LEN -1 : strlen(pAppName));
    ptAclApp->m_wAppId = nNewAppID;
    ptAclApp->m_byAppPrity = 80;
    ptAclApp->m_wAppMailBoxSize = 80;
    ptAclApp->m_dwInstStackSize = nInstStackSize > 0 ? nInstStackSize : 64*1024;
    ptAclApp->m_dwInstNum = nInstNum;
    ptAclApp->m_dwInstDataLen = 256;
    ptAclApp->m_pMsgCB = pfMsgCB;
    ptAclApp->m_pExtAppData = NULL;

    //create aclInstance
    ptAclApp->m_pptInst = (TAclInstance **)aclMallocClr(nInstNum * sizeof(TAclInstance *));
    memset(ptAclApp->m_pptInst,0,nInstNum * sizeof(TAclInstance *));
    {
        int i = 0;
        for (i = 0; i < nInstNum; i++)
        {
            ptAclApp->m_pptInst[i] = aclCreateInstance(ptAclApp, i + 1);
			//insert appID which instance
			ptAclApp->m_pptInst[i]->m_nRootAppID = nNewAppID;

            ptAclApp->m_dwInstCount++;
        }
    }
    return ACL_ERROR_NOERROR;
}

//=============================================================================
//函 数 名：aclCreateApp__b
//功	    能：创建APP
//算法实现：
//全局变量：
//参	    数：nNewAppID： 创建的APPID
//           pAppName: APP名称 可为空
//           nInstNum: 包含Instance数量
//     nInstStackSize: Instance 线程的堆栈大小
//            pfMsgCB: APP的Entry函数，APP接收的消息都会通知此函数
//注    意:APP中包含的每个Instance都有独立的处理线程，各个Instance独立处理各自消息
//         每个Instance都有自己的状态机，可以在注册函数中修改
//=============================================================================
ACL_API int aclCreateApp__b(TAclAppParam * pTAclAppParam)
{
	TAclAppParam * ptAppParam = pTAclAppParam;
	CHECK_NULL_RET_ERR_PARAM(pTAclAppParam);
	TAclApp * ptAclApp = NULL;
	int nErrCode = 0;
	nErrCode = regNewAclAPP(ptAppParam->m_wAppId);
	if (nErrCode < 0)
	{
		ACL_DEBUG(E_MOD_MANAGE, E_TYPE_ERROR, "[aclCreateApp]create App failed EC:%d\n", nErrCode);
		return nErrCode;
	}
	ptAclApp = getAclApp(ptAppParam->m_wAppId);
	if (ptAclApp == NULL)
	{
		return ACL_ERROR_CONFLICT;
	}
	if (ACL_ERROR_INVALID == aclCreateLock(&ptAclApp->m_hAppLock, NULL))
	{
		return ACL_ERROR_FAILED;
	}
	memset(ptAclApp->m_achAppName, 0, MAX_APP_NAME_LEN);

	strncpy(ptAclApp->m_achAppName, ptAppParam->m_achAppName,
		strlen(ptAppParam->m_achAppName) > MAX_APP_NAME_LEN - 1 ? MAX_APP_NAME_LEN - 1 : strlen(ptAppParam->m_achAppName));
	ptAclApp->m_wAppId = ptAppParam->m_wAppId;
	ptAclApp->m_byAppPrity = ptAppParam->m_byAppPrity;
	ptAclApp->m_wAppMailBoxSize = ptAppParam->m_wAppMailBoxSize;
	ptAclApp->m_dwInstStackSize = ptAppParam->m_dwInstStackSize > 0 ? ptAppParam->m_dwInstStackSize : 64 * 1024;
	ptAclApp->m_dwInstNum = ptAppParam->m_dwInstNum;
	ptAclApp->m_dwInstDataLen = ptAppParam->m_dwInstDataLen;
	ptAclApp->m_pMsgCB = ptAppParam->m_pMsgCB;
	ptAclApp->m_pExtAppData = ptAppParam->m_pExtAppData;

	//create aclInstance
	ptAclApp->m_pptInst = (TAclInstance **)aclMallocClr(ptAppParam->m_dwInstNum * sizeof(TAclInstance *));
	memset(ptAclApp->m_pptInst, 0, ptAppParam->m_dwInstNum * sizeof(TAclInstance *));
	{
		u32 i = 0;
		for (i = 0; i < ptAclApp->m_dwInstNum; i++)
		{
			ptAclApp->m_pptInst[i] = aclCreateInstance(ptAclApp, i + 1);
			//insert appID which instance
			ptAclApp->m_pptInst[i]->m_nRootAppID = ptAclApp->m_wAppId;

			ptAclApp->m_dwInstCount++;
		}
	}
	return ACL_ERROR_NOERROR;
}

//=============================================================================
//函 数 名：aclDestroyApp
//功	    能：销毁指定的APP
//算法实现：
//全局变量：
//参	    数：nNewAppID： 要销毁的APPID号
//注    意:
//=============================================================================
ACL_API int aclDestroyApp(u16 nAppID)
{
    TAclApp * ptAclApp = NULL;
    int nErrCode = ACL_ERROR_NOERROR;
    ptAclApp = getAclApp(nAppID);
    if (NULL == ptAclApp)
    {
        return ACL_ERROR_PARAM;
    }

    aclMsgPush(0, MAKEID(nAppID, INST_BROADCAST), 0, MSG_QUTE, NULL, 0, E_PMT_L_L);

    nErrCode = unRegAclAPP(nAppID);
    if (nErrCode <0)
    {
        ACL_DEBUG(E_MOD_MANAGE,E_TYPE_ERROR,"[aclCreateApp]unReg App failed EC:%d\n",nErrCode);
    }
    while(0 != ptAclApp->m_dwInstCount)
    {
        //wait to instance all released
        aclDelay(1);
    }
    //then release Inst root memory
    aclFree(ptAclApp->m_pptInst);

    //destory app lock
    aclDestoryLock(ptAclApp->m_hAppLock);
    return nErrCode;
}

//=============================================================================
//函 数 名：aclDestroyAppList
//功	    能：销毁所有的APP
//算法实现：
//全局变量：
//参	    数：
//注    意:此函数是使用者的最后防线，如果用户创建APP而又不销毁，在ACL退出时
//         会进行检查，销毁所有APP
//=============================================================================
ACL_API int aclDestroyAppList()
{
    int i = 0;
    u16 nCheckAppID = ACL_INVALID_APPID;
    for (i = 0; i < MAX_APP_NUM; i++)
    {
        nCheckAppID = g_tGlbParam.m_nAclAppIDMap[i][ACL_APPID];
        if (ACL_INVALID_APPID != nCheckAppID)
        {
            aclDestroyApp(nCheckAppID);
        }
    }
    return ACL_ERROR_NOERROR;
}

//=============================================================================
//函 数 名：regNewAclAPP
//功	    能：APP注册
//算法实现：
//全局变量：
//参	    数：nNewAppID: APPID
//注    意:ACL的APP对象为全局静态数组维护，创建新的APP都需要通过此函数注册
//         可以利用此函数返回值判断APPID是否已经被创建
//=============================================================================
ACL_API int regNewAclAPP(u16 nNewAppID)
{
    int i = 0;
    int nFindPos = ACL_INVALID_APPPOS;

    for (i = 0; i < MAX_APP_NUM; i++)
    {
        if (nNewAppID == g_tGlbParam.m_nAclAppIDMap[i][ACL_APPID])
        {
            return ACL_ERROR_CONFLICT;
        }
		if (ACL_INVALID_APPPOS == nFindPos && (u16)ACL_INVALID_APPID == g_tGlbParam.m_nAclAppIDMap[i][ACL_APPID_POS])
		{
			nFindPos = i;
		}
    }
    if (ACL_INVALID_APPPOS == nFindPos)
    {
        return ACL_ERROR_OVERFLOW;
    }
    g_tGlbParam.m_nAclAppIDMap[nFindPos][ACL_APPID_POS] = nFindPos;
    g_tGlbParam.m_nAclAppIDMap[nFindPos][ACL_APPID] = nNewAppID;
    return nFindPos;
}

//=============================================================================
//函 数 名：unRegAclAPP
//功	    能：APP反注册
//算法实现：
//全局变量：
//参	    数：nAppID: 注销的APPID
//注    意:销毁的APP都需要调用此函数进行反注册
//=============================================================================
ACL_API int unRegAclAPP(u16 nAppID)
{
    int i = 0;
    int nFindPos = ACL_INVALID_APPPOS;

    for (i = 0; i < MAX_APP_NUM; i++)
    {
        if (nAppID == g_tGlbParam.m_nAclAppIDMap[i][ACL_APPID])
        {
            g_tGlbParam.m_nAclAppIDMap[i][ACL_APPID_POS] = ACL_INVALID_APPPOS;
            g_tGlbParam.m_nAclAppIDMap[i][ACL_APPID] = ACL_INVALID_APPID;
            return ACL_ERROR_NOERROR;
        }
    }
    return ACL_ERROR_FAILED;
}

//=============================================================================
//函 数 名：aclSetInstStatus
//功	    能：设置当前Instance的状态
//算法实现：
//全局变量：
//参	    数：hAclInst: 要修改的Instance的句柄
//           nStatus: 修改的状态
//注    意:设置默认状态机状态 为0 空闲，1为忙，可以从1开始定义符合应用需求的
//         InstStatus 定义，寻找Instance
//         
//=============================================================================
ACL_API void aclSetInstStatus(HACLINST hAclInst,int nStatus)
{
    TAclInstance * ptAclInst = (TAclInstance *)hAclInst;
    CHECK_NULL_RET(ptAclInst)
    lockLock(ptAclInst->hInstLock);
    ptAclInst->m_nInstState = nStatus;
    unlockLock(ptAclInst->hInstLock);
}

//=============================================================================
//函 数 名：aclGetInstStatus
//功	    能：获得当前Instance的状态
//算法实现：
//全局变量：
//参	    数：hAclInst: Instance目标的句柄
//注    意:返回值将是aclSetInstStatus的设置值，可以根据项目需要修改
//         
//=============================================================================
ACL_API INST_STATUS aclGetInstStatus(HACLINST hAclInst)
{
    TAclInstance * ptAclInst = (TAclInstance *)hAclInst;
    if (NULL == hAclInst)
    {
        return INST_STATE_INVALID;
    }
    return (INST_STATUS)ptAclInst->m_nInstState;
}

//=============================================================================
//函 数 名：aclGetInstStatus
//功	    能：获得当前Instance的ID
//算法实现：
//全局变量：
//参	    数：hAclInst: Instance目标的句柄
//注    意:返回值将是aclSetInstStatus的设置值，可以根据项目需要修改
//         
//=============================================================================
ACL_API int aclGetInstID(HACLINST hAclInst)
{
	TAclInstance * ptAclInst = (TAclInstance *)hAclInst;
	if (NULL == hAclInst)
	{
		return INST_STATE_INVALID;
	}
	return (INST_STATUS)ptAclInst->m_nInstID;
}

//=============================================================================
//函 数 名：aclGetInstNum
//功	    能：获得Instance下标
//算法实现：
//全局变量：
//参	    数：hAclInst: Instance句柄
//           
//返    回:Instance下标
//注    意:Instance下标从0开始
//=============================================================================
ACL_API int aclGetInstNum(HACLINST hAclInst)
{
	TAclInstance * ptAclInst = (TAclInstance *)hAclInst;
	if (NULL == hAclInst)
	{
		return INST_STATE_INVALID;
	}
	return ptAclInst->m_nInstID;
}

ACL_API int aclGetInstMsgPoolLeft(HACLINST hAclInst)
{
	TAclInstance * ptAclInst = (TAclInstance *)hAclInst;
	int nInstBufPoolLen = 0;
	CHECK_NULL_RET_INVALID(ptAclInst)
	lockLock(ptAclInst->hInstLock);
	nInstBufPoolLen = ptAclInst->m_ptInstMsgQueue->m_nCurQueMembNum;
	unlockLock(ptAclInst->hInstLock);
	return nInstBufPoolLen;
}

//=============================================================================
//函 数 名：aclCreateInstance
//功	    能：创建Instance线程
//算法实现：
//全局变量：
//参	    数：ptAclApp: Instance所属APP对象的指针
//          nInstID : Instance编号ID，一般顺序排列
//        param: 线程数附带参数
//注    意:instance 销毁问题由Instance线程通过消息管理，因此没有Instance销毁函数
//=============================================================================
ACL_API TAclInstance * aclCreateInstance(TAclApp * ptAclApp,int nInstID)
{ 
    TTHREAD_PARAM tThreadParam;
    ACL_THREAD_ATTR tthreadAttr;
    u32 nInstPos = nInstID - 1;
    void * * paramList = (void * * )aclMallocClr(sizeof(void *) * 2);
    CHECK_NULL_RET_NULL(ptAclApp)

    if (ptAclApp->m_dwInstNum < nInstPos)
    {
        return NULL;
    }
    
    if (NULL != ptAclApp->m_pptInst[nInstPos])
    {
        aclFree(ptAclApp->m_pptInst[nInstPos]);
    }

    
    ptAclApp->m_pptInst[nInstPos]  = (TAclInstance *)aclMallocClr(sizeof(TAclInstance));
    
    if (NULL == ptAclApp->m_pptInst[nInstPos])
    {
        return NULL;
    }
    ptAclApp->m_pptInst[nInstPos]->m_nInstID = nInstID;

    memset(&tthreadAttr,0,sizeof(ACL_THREAD_ATTR));

    tThreadParam.aclThreadAttr = tthreadAttr;
    tThreadParam.dwStackSize = ptAclApp->m_dwInstStackSize;
    tThreadParam.nThreadRun = 1;

    ptAclApp->m_pExtAppData = NULL;

    //将APP指针作为参数的话，由于创建线程需要时间，可能线程函数来不及处理，app对应的扩展数据就变掉了
    //这里申请2字节内存用于线程参数， pos0 = appAddress, pos1 = nInstPos
    //内存在线程函数中释放
    paramList[0] = (void *)ptAclApp;
    paramList[1] = (void *)nInstPos;
    aclCreateThread_b(&ptAclApp->m_pptInst[nInstPos]->m_tInstThread,
        &tThreadParam,
        aclInstanceManageThread,
        (void *)paramList);

    return ptAclApp->m_pptInst[nInstPos];
}

//=============================================================================
//函 数 名：aclInstanceManageThread
//功	    能：Instance管理线程，用于处理消息
//算法实现：
//全局变量：
//参	    数：
//        param: 线程数附带参数
//注    意:线程附带参数是一个内存分配的指针数组
//        [0] 存放Instance所属 APP的指针
//        [1] 存放Instance在APP的Inst数组中的位置信息
// 参数动态分配是由于创建多个Instance线程运行时间的无序性，如果使用局部变量可能
// 造成多个Instance 线程创建，而使用一份参数造成问题

//Instance消息的管理使用了 消息浅拷贝模式 即消息体由用户维护,内容需要使用者分配
//并交出管理权
//=============================================================================
#ifdef WIN32
ACL_API unsigned int __stdcall aclInstanceManageThread(void * param)
#elif defined (_LINUX_)
ACL_API void * aclInstanceManageThread(void * param)
#endif

{
    TAclApp * ptAclAppInfo = NULL;
    PTMSG_QUEUE ptMsgQueue = NULL;
    TAclMessage  tMsg = {0};
    int nMsgLen = 0;
    int nInstPos = 0;
    int nRet = 0;
    int bThreadRun = 1;

    if (NULL == param)
    {
#ifdef WIN32
        return ACL_ERROR_INVALID;
#elif defined (_LINUX_)
        return (void *)ACL_ERROR_INVALID;
#endif
    }
    
    ptAclAppInfo = (TAclApp *) ((void **)param)[0];
    nInstPos = *(int*)(&((void **)param)[1]); //ptAclAppInfo->m_pExtAppData;

    //参数为分配的内存，获得参数指针值后就可以释放了
    aclFree(param);

    ptAclAppInfo->m_pExtAppData = NULL;

    //浅拷贝模式，附带TAclMessage的消息队列
    ptMsgQueue = createAclMsgQueue(ptAclAppInfo->m_wAppMailBoxSize ? ptAclAppInfo->m_wAppMailBoxSize : DEFAULT_INST_MSG_NUM,
        ACL_QUE_SHALW_COPYMODE, sizeof(TAclMessage));
    
    ptAclAppInfo->m_pptInst[nInstPos]->m_nInstState = (int)E_INST_STATE_IDLE;
    
    //Instance锁，生命周期就在此关系线程中
    if (ACL_ERROR_INVALID == aclCreateLock(&ptAclAppInfo->m_pptInst[nInstPos]->hInstLock,NULL))
    {
#ifdef WIN32
        return ACL_ERROR_FAILED;
#elif defined (_LINUX_)
        return (void *)ACL_ERROR_FAILED;
#endif
        
    }
    ptAclAppInfo->m_pptInst[nInstPos]->m_ptInstMsgQueue = ptMsgQueue;

    while(bThreadRun)
    {
        nRet = getAclMsg(ptMsgQueue,&tMsg, &nMsgLen, INFINITE);
        if (ACL_ERROR_NOERROR != nRet)
        {
            ACL_DEBUG(E_MOD_MANAGE,E_TYPE_ERROR,"[aclInstanceManageThread] getAclMsg failed %d\n",nRet);
            continue;
        }
        if (MSG_QUTE == tMsg.m_wMsgType)// check inner msg;
        {
            aclDestoryThread(ptAclAppInfo->m_pptInst[nInstPos]->m_tInstThread.hThread);
            bThreadRun = FALSE;
            break;
        }
        if (MSGMASK_THROW_AWAY == tMsg.m_wMsgStatus)
        {
            ACL_DEBUG(E_MOD_MANAGE,E_TYPE_DEBUG,"[aclInstanceManageThread]throw msg %d away\n",tMsg.m_wMsgType);
            if (ACL_QUE_SHALW_COPYMODE == ptMsgQueue->eHandleMode ||
                ACL_QUE_DEEP_COPYMODE == ptMsgQueue->eHandleMode)
            {
				if (tMsg.m_pContent)
				{
					aclFree(tMsg.m_pContent);
					tMsg.m_pContent = NULL;
				}
                
            }
            continue;
        }
		

        ptAclAppInfo->m_pMsgCB(&tMsg,(HACLINST)ptAclAppInfo->m_pptInst[nInstPos]);
        //after handle cure message  destroy it

        //消息处理完毕后还是要选择性清除消息头指向的消息体的
        if (ACL_QUE_SHALW_COPYMODE == ptMsgQueue->eHandleMode ||
            ACL_QUE_DEEP_COPYMODE == ptMsgQueue->eHandleMode)
        {
			if (tMsg.m_pContent)
			{
				aclFree(tMsg.m_pContent);
				tMsg.m_pContent = NULL;
			}
        }
    }

    {
        //这里需要销毁所有消息，以及环形队列的消息节点
        //同时由于在销毁消息队列的时候，可能Instance 还在尝试插入，因此仍旧需要Instance锁
        lockLock(ptAclAppInfo->m_pptInst[nInstPos]->hInstLock);
        destoryAclMsgQueue(ptMsgQueue);
        unlockLock(ptAclAppInfo->m_pptInst[nInstPos]->hInstLock);
    }
    nRet = aclDestoryLock(ptAclAppInfo->m_pptInst[nInstPos]->hInstLock);

    aclFree(ptAclAppInfo->m_pptInst[nInstPos]);

    lockLock(ptAclAppInfo->m_hAppLock);
    ptAclAppInfo->m_dwInstCount--;
    unlockLock(ptAclAppInfo->m_hAppLock);

    
#ifdef WIN32
    return ACL_ERROR_NOERROR;
#elif defined (_LINUX_)
    return (void *)ACL_ERROR_NOERROR;
#endif
    
}

#ifdef _LINUX_
unsigned long GetTickCount()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

unsigned int getRandNum()
{
	unsigned long res = 0;
    static unsigned long seed = 0xDEADB00B;
    res = GetTickCount();

	seed = ((seed & 0x007F00FF) << 7) ^
		((seed & 0x0F80FF00) >> 8) ^ // be sure to stir those low bits
		(res << 13) ^ (res >> 9);    // using the clock too!

	return seed;
};

void getRandomByte(void *buf, size_t len)
{
	unsigned int ranbuf;
	unsigned int *lp;
	int i, count;
	count = len / sizeof(unsigned int);
	lp = (unsigned int *) buf;

	for(i = 0; i < count; i ++) {
		lp[i] = getRandNum();  
		len -= sizeof(unsigned int);
	}

	if(len > 0) {
		ranbuf = getRandNum();
		memcpy(&lp[i], &ranbuf, len);
	}
}
int aclCheckPack(char * pPackData, u16 wPackLen)
{
	TAclMessage * ptAclMsg = NULL;
	if (NULL == pPackData || 0 == wPackLen)
	{
		return ACL_ERROR_PARAM;
	}
	if (wPackLen < sizeof(TAclMessage))
	{
		//size is not enough
		return ACL_ERROR_INVALID;
	}

	ptAclMsg = (TAclMessage *)pPackData;
	if (ptAclMsg->m_dwPackLen != wPackLen || wPackLen - sizeof(TAclMessage) != ptAclMsg->m_dwContentLen)
	{
        ACL_DEBUG(E_MOD_NETWORK,E_TYPE_WARNNING, "[aclCheckPack] LEN_INFO:%d %d %d %d\n",
            wPackLen, ptAclMsg->m_dwPackLen, sizeof(TAclMessage),ptAclMsg->m_dwContentLen);
		return ACL_ERROR_INVALID;
	}
	return ACL_ERROR_NOERROR;
}


//=============================================================================
//函 数 名：aclNewMsgProcess
//功	    能：处理网络上来的新消息
//算法实现：
//全局变量：
//参	    数：
//        nFd: 连接的Socket句柄
//     eEvent: Socket类型
//   pContext: 附带参数
//注    意: 此函数用于注册给Socket处理的Select循环
//          此函数只在新的连接通过3A验证后才会注册到新Socket对应的 Select循环中去
//
//         类似的函数有newConn3ACheck 区别在于此函数之后通过newConn3ACheck
//         后才有机会注册给连接的Socket
//=============================================================================
s32 aclNewMsgProcess(H_ACL_SOCKET nFd, ESELECT eEvent, void* pContext)
{
	HSockManage hSockmng = (HSockManage)getSockDataManger();
	char szRcvData[MAX_RECV_PACKET_SIZE] = {0};
	TAclMessage * ptAclMsg = NULL;
	int nRet = 0,nRcvSize = 0;

	if (ESELECT_READ != eEvent)
	{
		ACL_DEBUG(E_MOD_MSG,E_TYPE_WARNNING,"[aclNewMsgProcess] new message but not read,wearied\n");
		return ACL_ERROR_INVALID;
	}

	nRcvSize = aclTcpRecv(nFd, szRcvData, MAX_RECV_PACKET_SIZE);

	if (0 >= nRcvSize)//attempt to disconnect current connect
	{
		ACL_DEBUG(E_MOD_MSG,E_TYPE_ERROR,"[aclNewMsgProcess] TCP Recv error, aclNewMsgProcess disconnected RecvSize %d\n", nRcvSize);
#ifdef WIN32
		ACL_DEBUG(E_MOD_MSG,E_TYPE_WARNNING,"[aclNewMsgProcess] connection was forcibly closed by the remote host err id %d\n", WSAGetLastError());
#endif
        aclRemoveSelectLoopUnsafe(hSockmng, nFd);
		return ACL_ERROR_NOERROR;
	}
	ACL_DEBUG(E_MOD_MSG,E_TYPE_DEBUG,"[aclNewMsgProcess] nRcvSize:%d\n",nRcvSize);
	nRet = aclCheckPack(szRcvData,nRcvSize);
	//更正逻辑，如果返回无错误，表示包完整，直接进行，否则表示出现黏包，包不全
	if (ACL_ERROR_NOERROR == nRet)
	{
		//regroup struct
		ptAclMsg = (TAclMessage *)szRcvData;
		ptAclMsg->m_pContent = (u8 *)((char *)ptAclMsg + sizeof(TAclMessage));

		//as a local message handle
		aclMsgPush(ptAclMsg->m_dwSrcIID, 
			ptAclMsg->m_dwDstIID, 
			ptAclMsg->m_dwSessionID, 
			ptAclMsg->m_wMsgType, 
			ptAclMsg->m_pContent, 
			ptAclMsg->m_dwContentLen, E_PMT_N_L);
		return ACL_ERROR_NOERROR;
	}
	
	if (ACL_ERROR_NOERROR != nRet)
	{
#ifdef WIN32
		int id = WSAGetLastError();
		switch (id)
		{
		case WSAECONNRESET:  
			{
				ACL_DEBUG(E_MOD_MSG,E_TYPE_WARNNING,"[aclNewMsgProcess] connection was forcibly closed by the remote host err id %d\n", id);
                aclRemoveSelectLoopUnsafe(hSockmng, nFd);
				return ACL_ERROR_NOERROR;
			}
			break;
		case 0:
			{
				//无错误，不应该执行到这里
				ACL_DEBUG(E_MOD_MSG,E_TYPE_ERROR,"[aclNewMsgProcess] no error here, odd\n");
			}
			break;
		default:ACL_DEBUG(E_MOD_MSG,E_TYPE_ERROR,"[aclNewMsgProcess] unknown error id = %d\n",id); break;
		};
//		return ACL_ERROR_FILTER;
#elif defined (_LINUX_)
		//linux目前不能确认是否socket出了问题，因默认假定出了黏包，半包，下面统一处理
        
#endif
	}

	//没有错误出现(linux假定也没有错误)，只有一种可能，出现黏包，当前接收的包不全，需要插入包缓存管理
	//下面的函数支持管理包数据，同时自动发送可发送的包
	nRet = aclInsertPBMAndSend(hSockmng, nFd, szRcvData, nRcvSize);
	return ACL_ERROR_NOERROR;
}


//=============================================================================
//函 数 名：newConn3ACheck
//功	    能：处理验证新的连接的3A
//算法实现：
//全局变量：
//参	    数：
//        nFd: 连接的Socket句柄
//     eEvent: Socket类型
//   pContext: 附带参数
//注    意: 此函数用于注册给Socket处理的Select循环
//          此函数仅用于节点收到的连接请求，并进行3A验证，如果通过则会将Socket绑定给
//          aclNewMsgProcess 函数，否则直接断开
//=============================================================================
s32 newConn3ACheck(H_ACL_SOCKET nFd, ESELECT eEvent, void* pContext)
{
	char szRcvSnd3AData[RS_SKHNDBUF_LEN] = {0};
	int nRet = 0;
	int nRcvSize = 0;
	int i = 0;
	TAclMessage tAclMsg;
    char * p3AContent = NULL;
	ACL_DEBUG(E_MOD_MSG,E_TYPE_DEBUG,"[newConn3ACheck] check 3A pass or not\n");
	if (!(ESELECT_CONN & eEvent))
	{
		ACL_DEBUG(E_MOD_MSG,E_TYPE_ERROR,"[newConn3ACheck] confused conn3A got not conn socket status\n");
		return ACL_ERROR_INVALID;
	}
	//get Client Check Data
	nRet = aclTcpRecv(nFd, szRcvSnd3AData, RS_SKHNDBUF_LEN);
	if(nRet < 0)
	{
		ACL_DEBUG(E_MOD_MSG,E_TYPE_ERROR,"[newConn3ACheck] acl recv 3A check value failed\n");
		return ACL_ERROR_FAILED;
	}

	for(i = 0; i < nRet; i++)
	{
		szRcvSnd3AData[i] ^= TEMP_3A_CHECK_NUM;
		ACL_DEBUG(E_MOD_MSG,E_TYPE_DEBUG, "[newConn3ACheck]%02X ", *(unsigned char *)&szRcvSnd3AData[i]);
	}

    //服务端处理3A返回数据
    p3AContent = (szRcvSnd3AData+ sizeof(TAclMessage));


    if (IsBigEndian() == p3AContent[0])//both Endian is matched
    {
        ACL_DEBUG(E_MOD_MSG,E_TYPE_NOTIF, "[newConn3ACheck] client and Current Env are both running at %s mode\n", 
            IsBigEndian()?"BigEndian":"LittleEndian");
    }
    else
    {
        ACL_DEBUG(E_MOD_MSG,E_TYPE_NOTIF, "[newConn3ACheck] client is running at %s mode but Current Env is running at %s mode\n", 
            p3AContent[0]?"BigEndian":"LittleEndian", IsBigEndian()?"BigEndian":"LittleEndian");
    }

	//insert new connect socket to aclDataProcThread
	//then current node need not 3A anymore
	// but do nothing the acl3AcheckThread will delete after
	//3A thread will free it automatically
//	aclFree(pContext);//this is alloc by newConnectProc;

	//nRet is Node Number as communication session ID
	ACL_DEBUG(E_MOD_MSG,E_TYPE_DEBUG, "[newConn3ACheck] current socket 3A is pass\n");
	nRet = aclInsertSelectLoop(g_tGlbParam.m_hSockMngData, nFd, aclNewMsgProcess, ESELECT_READ, 0, pContext);

	if (nRet < ACL_ERROR_NOERROR)//insert failed, maybe node space is all busy
	{
		ACL_DEBUG(E_MOD_MSG,E_TYPE_ERROR, "[newConn3ACheck] insert 3A passed session EC:%d failed\n", nRet);
		return nRet;
	}
	ACL_DEBUG(E_MOD_MSG,E_TYPE_DEBUG,"[newConn3ACheck]node %d:insert..OK\n",nRet);
	memset(&tAclMsg, 0, sizeof(TAclMessage));
	tAclMsg.m_dwSessionID = nRet;
	//send confirm pack with node info
	nRcvSize = aclCombPack(g_tGlbParam.m_sendBuf, MAX_SEND_PACKET_SIZE, &tAclMsg, NULL, 0);
	aclTcpSend(nFd, g_tGlbParam.m_sendBuf, nRcvSize);
	return ACL_ERROR_NOERROR;
}


//=============================================================================
//函 数 名：newConn3ACheck
//功	    能：处理新的连接请求
//算法实现：
//全局变量：
//参	    数：
//        nFd: 连接的Socket句柄
//     eEvent: Socket类型
//   pContext: 附带参数
//注    意: 此函数和监听Node绑定，处理新的连接请求，并将建立连接的Socket投递到
//          newConn3ACheck 函数中进行 3A验证。
//=============================================================================
s32 newConnectProc(H_ACL_SOCKET nFd, ESELECT eEvent, void* pContext)
{
	H_ACL_SOCKET hConnSock = INVALID_SOCKET;
	char * pChkID = NULL;
	TAclMessage tNewMessage;
	char sz3ABuf[RS_SKHNDBUF_LEN] = {0};
	u32 dwConnIP = 0;
	u16 wConnPort = 0;
	int nSendDataLen = 0;
	ACL_DEBUG(E_MOD_MSG,E_TYPE_DEBUG,"[newConnectProc] coming into new Connect Proc\n");
	if (ESELECT_READ != eEvent)
	{
		return ACL_ERROR_INVALID;
	}
	//new connect 
    TPeerClientInfo *tpPeerCliInfo = (TPeerClientInfo*)aclMallocClr(sizeof(TPeerClientInfo));
	hConnSock = aclTcpAccept(nFd, &dwConnIP, &wConnPort);
	if (INVALID_SOCKET == hConnSock)//connect is failed
	{
		ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[newConnectProc] tcp accept is failed\n");
		return ACL_ERROR_FAILED;
	}
    tpPeerCliInfo->m_nPeerClientAddr = dwConnIP;
    tpPeerCliInfo->m_nPeerClientPort = wConnPort;
	if (NULL ==g_tGlbParam.m_hSockMng3A)
	{
		ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[newConnectProc] handle new conn failed, 3A proc is not ready\n");
	}
	pChkID = (char *)aclMallocClr(CHECK_DATA_LEN);
	if (NULL == pChkID)
	{
		return ACL_ERROR_MALLOC;
	}
	getRandomByte(pChkID,CHECK_DATA_LEN);
	//这里需要发送标准包结构（附带随机数），然后3A线程等待验证通过.
    //第一个字节作为服务端的大小端模式标记位
    pChkID[0] = IsBigEndian();
	memset(&tNewMessage,0,sizeof(tNewMessage));
	tNewMessage.m_wMsgType = MSG_3A;

	nSendDataLen = aclCombPack(sz3ABuf, RS_SKHNDBUF_LEN, &tNewMessage, pChkID, CHECK_DATA_LEN);
	if (nSendDataLen < 0)
	{
		ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[newConnectProc] combine packet failed EC:%d\n" ,nSendDataLen);
	}
	aclInsertSelectLoop(g_tGlbParam.m_hSockMng3A, hConnSock, newConn3ACheck, ESELECT_CONN, -1, tpPeerCliInfo);
	aclTcpSend(hConnSock, sz3ABuf, nSendDataLen);
	return ACL_ERROR_NOERROR;
}


//=============================================================================
//函 数 名：aclCreateTcpNode
//功	    能：创建监听节点
//算法实现：
//全局变量：
//参	    数：
//      wPort: 监听端口
//注    意: 此函数用于创建监听节点，将节点与 newConnectProc函数绑定用于
//          处理所有的连接请求
//=============================================================================
ACL_API int aclCreateTcpNode(u16 wPort)
{
	int nErrCode = 0;
	if (0 == wPort)
	{
		return ACL_ERROR_PARAM;
	}

	if (NULL == g_tGlbParam.m_hSockMngData)
	{
		return ACL_ERROR_INIT;
	}

	//由于3A select线程有定时关闭socket监听的功能，因此会将创建的Node放到数据收发线程中
	//当收到新的连接后再将返回的socket放到3A线程进行限制时间的验证
	nErrCode = aclCreateNode(g_tGlbParam.m_hSockMngData,"0.0.0.0",wPort,newConnectProc, NULL);
	if (ACL_ERROR_NOERROR != nErrCode)
	{
		ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[newConnectProc] create tcp node failed EC:%d\n",nErrCode);
	}
	return nErrCode;
}

    

//用来将消息投递到消息处理线程队列，中间会有内存分配与数据拷贝过程
//app inst  定义
//inst 号为0 表示从inst池中随机找一个在空闲状态的inst线程
//inst 号为1 到 TAclApp.m_dwInstNum 表示对应的inst号
//inst 号为 TAclApp.m_dwInstNum+1表示inst广播，所有相关inst都会被推送此消息到队列
//
typedef enum
{
    MSG_INVALID = -1,
    MSG_SEEK_IDLE = 0,//seek an unused Inst
    MSG_DIRECT_INST = 1,//post msg to the inst directly
    MSG_BROADCAST = 2,//post broadcast in  APP
    MSG_RANDOM = 3,//post random inst  in  APP
}EACLMSGTYPE;

typedef enum
{
    SEEK_INVALID = -1,
    SEEK_SEEK_INST = 0,//seek an unused Inst
    SEEK_DIRECT_INST = 1,//seek msg to the inst directly
    SEEK_BROADCAST = 2,//seek all inst in  APP
}EACL_SEEK_ID_TYPE;


//=============================================================================
//函 数 名：checkMsgType
//功	    能：ACL 消息类型检测
//算法实现：
//全局变量：
//参	    数：
//      dwAppInstID: 发送消息的地址(APP Instance)
//注    意: 此函数用于解析投递的消息类型，从而决定发往的Instance
//=============================================================================
EACLMSGTYPE checkMsgType(u32 dwAppInstID)
{
    u16 wDstAPPID,wDstInstID;
    TAclApp * ptAclApp = NULL;
    wDstAPPID = getAppID(dwAppInstID);
    wDstInstID = getInsID(dwAppInstID);
    ptAclApp = getAclApp(wDstAPPID);

    if (NULL == ptAclApp)
    {
        return MSG_INVALID;
    }


    if (wDstInstID <= ptAclApp->m_dwInstNum && wDstInstID > 0)// post message to direct instance
    {
        return MSG_DIRECT_INST;
    }

    switch(wDstInstID)
    {
    case INST_SEEK_IDLE:
        {
            return MSG_SEEK_IDLE;
        }
        break;
    case INST_BROADCAST:
        {
            return MSG_BROADCAST;
        }
        break;
    case INST_RANDOM:
        {
            return MSG_RANDOM;
        }
        break;
    default:
        return MSG_INVALID;
    }
    
}

//=============================================================================
//函 数 名：aclInstPost__
//功	    能：向指定的Instance投递消息
//算法实现：
//全局变量：
//参	    数：
//            hInst: 发往指定的Instance句柄
// dwDstAppInstAddr: 消息地址(zheli )
//         dwNodeID: 会话ID，由连接建立后双方各分配一个ID，用于标记当前会话
//         wMsgType: 消息号
//         pContent: 消息内容  
//      wContentLen: 消息内容长度
//注    意: 此消息用于内部调用，可以发送ACL内部消息和通用消息
//          外层封装API的时候需要检测消息有效性
//=============================================================================
ACL_API int aclInstPost__(HACLINST hInst,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen)
{
	TAclInstance * tAclInst = (TAclInstance *)hInst;
	PUSH_MSG_TYPE eMsgmask = E_PMT_L_L;

	if (NULL == tAclInst)
	{
		return ACL_ERROR_PARAM;
	}
	//fill src ID automatically
	if (0 != dwNodeID)
	{
		eMsgmask = E_PMT_L_N;
	}
    ACL_DEBUG(E_MOD_NETWORK, E_TYPE_DEBUG, "[aclInstPost] ROOTAPPID:%X INSTID:%X\n",tAclInst->m_nRootAppID,tAclInst->m_nInstID);
	return aclMsgPush(MAKEID(tAclInst->m_nRootAppID, tAclInst->m_nInstID),
		dwDstAppInstAddr, 
		dwNodeID, 
		wMsgType, 
		pContent, 
		dwContentLen,
		eMsgmask);
}

//=============================================================================
//函 数 名：aclPost__
//功	    能：将消息推送到要求的inst中(本地或者网络上)
//注	    意：这是一个全局函数，可以在任意地方调用
//算法实现：
//全局变量：
//参	    数：
//        dwSrcAppInstAddr:源节点ID，如果通过内部通知，可以全部填0
//        dwDstAppInstAddr:目的节点ID，可通过函数checkMsgType区分发送消息类型
//        dwNodeID: 节点ID，每次新链接双方都会分配一个新的节点ID作为通讯ID
//					0表示目标为本地
//        dwMsgType: 消息类型
//        pContent: 发送消息体
//        dwContentLen: 消息体尺寸
//注    意:pContent如果非NULL，因为内部有拷贝过程，因此必须保证指针值指向正确位置
//         填写正确的dwContentLen值, 否则会造成崩溃
//=============================================================================
ACL_API int aclPost__(u32 dwSrcAppInstAddr,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen)
{
	PUSH_MSG_TYPE ePushMask = E_PMT_L_L;
    if (wMsgType <= ACL_USER_MSG_BASE)
    {
        return ACL_ERROR_PARAM;
    }
	if (0 != dwNodeID)//local message
	{
		ePushMask = E_PMT_L_N;
	}
	if (wMsgType <= ACL_USER_MSG_BASE)//user message number must large than ACL_USER_MSG_BASE
	{
		return ACL_ERROR_FILTER;
	}
	return aclMsgPush(dwSrcAppInstAddr,dwDstAppInstAddr,dwNodeID,wMsgType,pContent,dwContentLen,ePushMask);
}

//=============================================================================
//函 数 名：aclMsgPush
//功	  能：将消息推送到要求的inst中(本地或者网络上)
//注	  意：
//算法实现：
//全局变量：
//参	  数：
//        dwSrcAppInstAddr:源节点ID，如果通过内部通知，可以全部填0
//        dwDstAppInstAddr:目的节点ID，可通过函数checkMsgType区分发送消息类型
//        dwNodeID: 节点ID，每次新链接双方都会分配一个新的节点ID作为通讯ID
//					0表示目标为本地
//        dwMsgType: 消息类型
//        pContent: 发送消息体
//        dwContentLen: 消息体尺寸
//注    意:pContent如果非NULL，因为内部有拷贝过程，因此必须保证指针值指向正确位置
//         填写正确的dwContentLen值, 否则会造成崩溃
//=============================================================================
ACL_API int aclMsgPush(u32 dwSrcAppInstAddr,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,void * pContent,u32 dwContentLen, PUSH_MSG_TYPE eMsgMask)
{
    u16 wDstAPPID,wDstInstID;
    TAclMessage tAclMsg;
    TAclApp * ptAclApp = NULL;
	TAclInstance * ptAclInst = NULL;
	u32 i = 0;
	int nRet = 0;
	unsigned int nRandNum = 0;
	u32 dwTmpID = 0;
    wDstAPPID = getAppID(dwDstAppInstAddr);
    wDstInstID = getInsID(dwDstAppInstAddr);
    memset(&tAclMsg,0,sizeof(tAclMsg));

    ACL_DEBUG(E_MOD_NETWORK, E_TYPE_DEBUG, "[aclMsgPush] SRCID:%X DSTID:%X NodeID:%d MSGType:%d \n",
		dwSrcAppInstAddr,
		dwDstAppInstAddr,
		dwNodeID,
		wMsgType);

	tAclMsg.m_dwSrcIID = dwSrcAppInstAddr;
	tAclMsg.m_dwDstIID = dwDstAppInstAddr;
	tAclMsg.m_dwSessionID = dwNodeID;
	tAclMsg.m_wMsgType = wMsgType;
	if (NULL != pContent && 0 != dwContentLen)
	{
		tAclMsg.m_pContent = (u8 *)aclMallocClr(dwContentLen);
		if (NULL != tAclMsg.m_pContent)
		{
			memcpy(tAclMsg.m_pContent,pContent,dwContentLen);
		}
	}
	tAclMsg.m_dwContentLen = dwContentLen;
	tAclMsg.m_dwPackLen = sizeof(TAclMessage) + tAclMsg.m_dwContentLen;

	switch(eMsgMask)
	{
	case E_PMT_L_N://local -> net
		{
			ptAclApp = getAclApp(NWPUSH_APP_NUM);
			nRandNum = getRandNum();
			//using random instance to send tcp message
			ptAclInst = ptAclApp->m_pptInst[nRandNum % ptAclApp->m_dwInstNum];
			dwTmpID = aclGlbNode2Net(g_tGlbParam.m_hSockMngData,dwNodeID);
			if (0 == dwTmpID)
			{
				ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[aclMsgPush] Node G[%d]->N[%d] Failed\n",dwNodeID,dwTmpID);
				return ACL_ERROR_FILTER;
			}
			//任何发往网络上的消息，会话ID必须是网络ID，在发送的时候，需要依据网络ID查找要发送的socket的偏移地址
            tAclMsg.m_dwSessionID = dwTmpID;
			lockLock(ptAclInst->hInstLock);
			insertAclMsg(ptAclInst->m_ptInstMsgQueue,&tAclMsg);
			unlockLock(ptAclInst->hInstLock);
			return ACL_ERROR_NOERROR;
			break;
		}
	case E_PMT_L_L:
		{
			break;
		}
	case E_PMT_N_L://trans node N->L
		{
			dwTmpID = aclNetNode2Glb(g_tGlbParam.m_hSockMngData,dwNodeID);
			
			if (0 == dwTmpID)
			{
				ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[aclMsgPush] Node N[%d]->G[%d] Failed\n",dwNodeID,dwTmpID);
				return ACL_ERROR_FILTER;
			}
			tAclMsg.m_dwSessionID = dwTmpID;
			break;
		}
	default:
		break;
	}
/*
	//it is a local message
    if (wDstAPPID >= MAX_APP_NUM)//beyond MAXAPPNUM
    {
		//APP NO. is so large
		aclPrintf(TRUE,FALSE,"[acl_mng] DstAPPID is large then MAX_APP_NUM(%d) msg discard\n",MAX_APP_NUM);
		aclFree(tAclMsg.m_pContent);
        return ACL_ERROR_FILTER;
    }
*/
	//查找本地APP
    ptAclApp = getAclApp(wDstAPPID);
	ACL_DEBUG(E_MOD_NETWORK,E_TYPE_DEBUG,"[aclMsgPush]Push Target APP:%d\n",wDstAPPID);
//  CHECK_NULL_RET_INVALID(ptAclApp)
	if (NULL == ptAclApp)
	{
		//查无此APP
		ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[aclMsgPush] app %d is not existed\n",wDstAPPID);
		aclFree(tAclMsg.m_pContent);
		return ACL_ERROR_FILTER;
	}

	//local msg check and post
    switch (checkMsgType(dwDstAppInstAddr))
    {
    case MSG_SEEK_IDLE:
        {
            for (i = 0; i < ptAclApp->m_dwInstNum; i++)
            {
                if ((int)E_INST_STATE_IDLE == ptAclApp->m_pptInst[i]->m_nInstState)
                {
                    ptAclInst = ptAclApp->m_pptInst[i];
                    break;
                }   
            }
            if (NULL == ptAclInst)
            {
                return ACL_ERROR_OVERFLOW;
            }
            lockLock(ptAclInst->hInstLock);
            nRet = insertAclMsg(ptAclInst->m_ptInstMsgQueue,&tAclMsg);
            unlockLock(ptAclInst->hInstLock);
        }
        break;
    case MSG_DIRECT_INST:
        {
            if (wDstInstID > ptAclApp->m_dwInstNum)//beyond max inst number
            {
                return ACL_ERROR_INVALID;
            }
            ptAclInst = ptAclApp->m_pptInst[wDstInstID -1];//inst start 1 but pos start 0

            if (NULL == ptAclInst)
            {
                return ACL_ERROR_OVERFLOW;
            }
            lockLock(ptAclInst->hInstLock);
            nRet = insertAclMsg(ptAclInst->m_ptInstMsgQueue,&tAclMsg);
            unlockLock(ptAclInst->hInstLock);
        }
        break;
    case MSG_BROADCAST:
        {
            for (i = 0; i < ptAclApp->m_dwInstNum; i++)
            {
                ptAclInst = ptAclApp->m_pptInst[i];
                lockLock(ptAclInst->hInstLock);
                nRet = insertAclMsg(ptAclInst->m_ptInstMsgQueue,&tAclMsg);
                unlockLock(ptAclInst->hInstLock);
            }
        }
        break;
    case MSG_RANDOM:
        {
            int nPos = aclGetTickCount()%ptAclApp->m_dwInstNum;
            ptAclInst = ptAclApp->m_pptInst[nPos];
            lockLock(ptAclInst->hInstLock);
            nRet = insertAclMsg(ptAclInst->m_ptInstMsgQueue,&tAclMsg);
            unlockLock(ptAclInst->hInstLock);
            
        }
        break;
    case MSG_INVALID:
        {
            ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[aclMsgPush]current msg addr is invalid APPID:%d INSID:%d\n",wDstAPPID,wDstInstID);
        }
        break;
    default:
        break;
    }
	if (ACL_ERROR_NOERROR != nRet)
	{
		ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[aclMsgPush]APPID:%d INSID:%d Push Msg Failed Ret %d\n",wDstAPPID,wDstInstID, nRet);
	}
	
    return 0;
}

EACL_SEEK_ID_TYPE checkMsgPos(u32 dwAppInstID)
{
    u16 wInstID;//,wAPPID;
    TAclApp * ptAclApp = NULL;
//  wAPPID = getAppID(dwAppInstID);
    wInstID = getInsID(dwAppInstID);
    ptAclApp = getAclApp(getAppID(dwAppInstID));

    if (NULL == ptAclApp)
    {
        return SEEK_INVALID;//cannot find app
    }
    if (0 == wInstID)
    {
        return SEEK_INVALID;//invalid means
    }

    if (1 + ptAclApp->m_dwInstNum == wInstID)//broadcast msg; usually used inner only
    {
        return SEEK_BROADCAST;
    }

    return SEEK_SEEK_INST;


}

//删除指定消息
ACL_API int aclDeleteMsg(u32 dwAppInstID,u16 wMsgType)
{
    u16 wInstID;
    TAclApp * ptAclApp = NULL;
    wInstID = getInsID(dwAppInstID);
    ptAclApp = getAclApp(getAppID(dwAppInstID));

    switch(checkMsgPos(dwAppInstID))
    {
    case SEEK_INVALID:
        {
            ACL_DEBUG(E_MOD_MSG,E_TYPE_ERROR,"[aclDeleteMsg] msg ID invalid\n");
        }
        break;
    case SEEK_BROADCAST://del all msg at all instance
        {
            u32 i;
            TAclInstance * ptAclInst = NULL;
            for (i = 0; i < ptAclApp->m_dwInstNum; i++)
            {
                ptAclInst = ptAclApp->m_pptInst[i];
                setAclMsg(ptAclInst->m_ptInstMsgQueue,wMsgType,MSGMASK_THROW_AWAY);
            }
        }
        break;
    case SEEK_DIRECT_INST: //del msg at confirmed instance 
        {

        }
        break;
    default:
        ACL_DEBUG(E_MOD_MSG,E_TYPE_ERROR,"[aclDeleteMsg] default invalid\n");
        break;
    }
	return 0;
}


//当前会话ID生成为毫秒级别，一天生成会话量为8640万个，
//SessionID不可为0 0表示内部节点，有特殊意义
ACL_API u32 aclSessionIDGenerate()
{
    u32 dwID = 0;
    lockLock(g_tGlbParam.m_hAclIDLock);
    dwID = ++g_tGlbParam.m_dwIDGenerate;
	if (0 == dwID)
	{
		dwID = ++g_tGlbParam.m_dwIDGenerate;
	}
    unlockLock(g_tGlbParam.m_hAclIDLock);
    return dwID;
}


//根据当前会话id获取对应的客户端ip和端口信息
ACL_API int aclGetClientInfoBySessionId(u32 dwNodeID, TPeerClientInfo& tPeerCliInfo)
{
    if (!dwNodeID)
    {
        return ACL_ERROR_PARAM;
    }

    int nRet = aclGetCliInfoBySpecSessionId(g_tGlbParam.m_hSockMngData, dwNodeID, tPeerCliInfo);
    if (!nRet)
    {
        return ACL_ERROR_EMPTY;
    }

    struct sockaddr_in sClientAddr;
#ifdef _MSC_VER
    sClientAddr.sin_addr.S_un.S_addr = tPeerCliInfo.m_nPeerClientAddr;
#elif defined (_LINUX_)	
    sClientAddr.sin_addr.s_addr = tPeerCliInfo.m_nPeerClientAddr;
#endif

    strcpy(tPeerCliInfo.m_szPeerClientAddrIp, inet_ntoa(sClientAddr.sin_addr));

    return ACL_ERROR_NOERROR;
}
