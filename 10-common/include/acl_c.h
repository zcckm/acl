//******************************************************************************
//模块名  ： acl_c
//文件名  ： acl_c.h
//作者    ： zcckm
//版本    ： 1.0
//文件功能说明:
//acl开放接口实现
//------------------------------------------------------------------------------
//修改记录:
//2014-12-30 zcckm 创建
//******************************************************************************
#ifndef _ACL_C_H_
#define _ACL_C_H_

//linux 下编译需要打开此宏定义
//#define _LINUX_

//#include "acl_unittest.h"
#include "acltype.h"

#ifdef WIN32
#pragma warning (disable:4005)
#endif

///////////////////////////////////////////////////////////////////
//	_LINUX_ 操作系统头文件
///////////////////////////////////////////////////////////////////
#ifdef _LINUX_

#ifdef PWLIB_SUPPORT
#include <ptlib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <malloc.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <termios.h>
#include <signal.h>

#ifndef _EQUATOR_
#include <mqueue.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#endif

#endif //__LINUX__

///////////////////////////////////////////////////////////////////
//	Win32 操作系统头文件
///////////////////////////////////////////////////////////////////
#ifdef _MSC_VER

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0400

#ifdef WINVER
#undef WINVER
#endif
#define WINVER 0x0400

#include <malloc.h>

#include <stdio.h>
//#include <windows.h>
#include <winsock2.h>

#include <assert.h>
#pragma comment(lib,"Ws2_32.lib")

#endif	// _MSC_VER

///////////////////////////////////////////////////////////////////
//    操作系统无关宏定义
///////////////////////////////////////////////////////////////////
#ifdef _MSC_VER			//_MSC_VER for Microsoft c++ 

#pragma warning(disable : 4007)
#pragma warning(disable : 4103)
#pragma warning(disable : 4996)  //strncpy unsafe warning

#if(_MSC_VER >= 1300)
#include <winsock2.h>
#include <mswsock.h>
#else
//#  include <winsock.h>
# include <winsock.h>
# endif
#include <windows.h>

#ifdef __cplusplus			// for C++ 
#define ACL_API  extern "C"			__declspec(dllexport)
#else						// for C 
#define ACL_API						__declspec(dllexport)
#endif  // __cplusplus

#elif defined (_LINUX_)				// for gcc 

#ifdef __cplusplus			// for C++ 
#define ACL_API  extern "C"	
#else						// for C 
#define ACL_API	
#endif

#endif //_MSC_VER

// +++++++++++++++++++global++++++++++++++++++++++>
#ifndef DECLARE_HANDLE
#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
#endif

DECLARE_HANDLE(HAclGlbParam);
DECLARE_HANDLE(HSockManage);

typedef void * ACL_HANDLE;

// #ifdef __cplusplus
// extern "C" {
// #endif //extern "C"

typedef enum
{
    ACL_ERROR_NOTSUP   = -12,//not support yet
	ACL_ERROR_MSG      = -11,//inner message not allowed user send.
    ACL_ERROR_EMPTY    = -10,//empty
	ACL_ERROR_FILTER   = -9,
	ACL_ERROR_PARAM    = -8,
	ACL_ERROR_INIT     = -7,
    ACL_ERROR_MALLOC   = -6,
    ACL_ERROR_FAILED   = -5,
    ACL_ERROR_OVERFLOW = -4,//full
    ACL_ERROR_CONFLICT = -3,
    ACL_ERROR_TIMEOUT  = -2,
    ACL_ERROR_INVALID  = -1,
    ACL_ERROR_NOERROR  =  0,
}ACLERROR_CODE;

#define CHECK_NULL_RET_NULL(p)\
    if(NULL == (p) )\
    return NULL;

#define CHECK_NULL_RET(p)\
    if(NULL == (p) )\
    return;

#define CHECK_NULL_RET_FALSE(p)\
    if(NULL == (p) )\
    return FALSE

#define CHECK_NULL_RET_BOOL(p)\
    if(NULL == (p) )\
    return FALSE;return TRUE;

#define CHECK_NULL_RET_INVALID(p)\
    if( NULL == (p) ) \
    return (int)ACL_ERROR_INVALID;

#define CHECK_BOOL_RET_BOOL(p)\
    if( !(p) ) )\
    return FALSE;

#define CHECK_NULL_RET_ERR_PARAM(p)\
	if(NULL == (p) )\
	return (int)ACL_ERROR_PARAM

// a must be a valid pointer
#define mzero( a ) \
{ \
    memset( &(a), 0x00, sizeof(a)); \
}


#ifdef WIN32

#define  H_ACL_SOCKET			 SOCKET

#define H_ACL_THREAD             HANDLE
#define ACL_THREAD_ATTR          SECURITY_ATTRIBUTES

#define H_ACL_LOCK               HANDLE
#define ACL_LOCK_ATTR            SECURITY_ATTRIBUTES

#define H_ACL_SEM HANDLE
#define ACL_SEM_ATTR SECURITY_ATTRIBUTES

#elif defined _LINUX_

#define  H_ACL_SOCKET			 int

#define H_ACL_THREAD             pthread_t
#define ACL_THREAD_ATTR          int

#define H_ACL_LOCK               pthread_mutex_t
#define ACL_LOCK_ATTR            pthread_mutexattr_t

#define H_ACL_SEM                sem_t
#define ACL_SEM_ATTR             int

#endif


//task start ....
typedef enum
{
	E_TASK_IDLE = 0,
	E_TASK_RUNNING,
	E_TASK_ASK_FOR_EXIT,
	E_TASK_ALREADY_EXIT
}ETASK_STATUS;

typedef struct tagAclThread {
	unsigned  dwThreadID;
	H_ACL_THREAD hThread;
	void *routine_arg;
	char  detached;
}TACL_THREAD, *PTACL_THREAD;

typedef struct tagThreadParam
{
	u32 dwStackSize;
	ACL_THREAD_ATTR aclThreadAttr;
	int nThreadRun;
}TTHREAD_PARAM, *PTTHREAD_PARAM;
//task end

#define PEER_CLIENT_IP_LEN      32

//记录客户端连接上来的ip和端口信息
typedef struct tagPeerClientInfo
{
    u32     m_nPeerClientAddr;
    u16     m_nPeerClientPort;
    char    m_szPeerClientAddrIp[PEER_CLIENT_IP_LEN];
    tagPeerClientInfo()
    {
        m_nPeerClientAddr = 0;
        m_nPeerClientPort = 0;
        memset(m_szPeerClientAddrIp, 0, PEER_CLIENT_IP_LEN);
    }
}TPeerClientInfo;


//thread fun define
#ifdef WIN32
typedef unsigned int(__stdcall * PF_THREAD_ENTRY) (void *);
#elif defined (_LINUX_)
typedef void * (*PF_THREAD_ENTRY) (void *);
#endif

typedef enum
{
    INST_STATE_INVALID = -1,
    INST_STATE_IDLE     = 0,
    INST_STATE_BUSY     = 1,
}INST_STATUS;
//<--------------------global----------------------- 

//  +++++++++++++++++++timer++++++++++++++++++++++>
#ifdef WIN32
typedef int (__stdcall * PF_TIMER_NTF) (void *);
#elif defined(_LINUX_)
typedef int ( * PF_TIMER_NTF) (void *);
#endif
//<--------------------timer----------------------- 

// +++++++++++++++++++message++++++++++++++++++++++>
ACL_API u32 MAKEID(u16 wApp,u16 wInstance);

ACL_API u16 getAppID(u32 dwID);

ACL_API u16 getInsID(u32 dwID);

//seek a idle instance 
#define INST_SEEK_IDLE 0

//broadcast to all instance 
#define INST_BROADCAST (u16)-1

//seek a random instance with any status
#define INST_RANDOM (u16)-2

#define   ACL_USER_MSG_BASE		(u16)0x0400

DECLARE_HANDLE(HACLAPP);
DECLARE_HANDLE(HACLINST);

#ifdef _MSC_VER			//_MSC_VER for Microsoft c++ 
#ifndef _EQUATOR_
#pragma pack(push)
#pragma pack(1)
#endif	// _EQUATOR__ 
#endif                 //_MSC_VER for Microsoft c++ 

//acl消息头部结构定义
typedef struct tagOspMessage
{    
    u32 m_dwSessionID;   //存储节点ID，由服务端分配
    u32 m_dwDstIID;      //目的应用实例    
    u32 m_dwSrcIID;      //源目的实例    
    u16 m_wMsgStatus;    //消息类型   //0: default 1:throw away 
    u16 m_wMsgType;      //消息号 
    u32 m_dwPackLen;      //包总长度 可能后面有附带数据
    u32 m_dwContentLen;   //消息体长度    
    u8 *m_pContent;      //消息体    
}
#if defined(_LINUX_)
__attribute__ ((packed)) 
#endif
    TAclMessage;

//结构体必须定义在以上区间
#ifdef _MSC_VER			//_MSC_VER for Microsoft c++ 
#ifndef _EQUATOR_
#pragma pack(pop)
#endif	// _EQUATOR__
#endif                  //_MSC_VER for Microsoft c++ 

typedef void (* CBMsgEntry)(TAclMessage *ptMsg, HACLINST hInst); 
//<----------------------message--------------------  

// ++++++++++++++++++++++telnet++++++++++++++++++++>
DECLARE_HANDLE(HAclTel);
DECLARE_HANDLE(HCmdEntry);
//<-----------------------telnet--------------------


// ++++++++++++++++++++++Interface++++++++++++++++++++>
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
ACL_API int aclInit(BOOL bTelnet, u16 wTelPort);


//=============================================================================
//函 数 名：aclQuit
//功	    能：acl模块退出
//算法实现：
//全局变量：
//参	    数：
//注    意: 在退出ACL时必须调用此函数，释放所有资源
//          
//=============================================================================
ACL_API void aclQuit();


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
//         每个Instance都有自己的状态机，可以在回调函数中修改各个Instance状态。
//=============================================================================
ACL_API int aclCreateApp(u16 nNewAppID,const s8 * pAppName,int nInstNum,int nInstStackSize, CBMsgEntry pfMsgCB);


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
//         每个Instance都有自己的状态机，可以在回调函数中修改各个Instance状态。
//=============================================================================
ACL_API int aclCreateApp1(u16 nNewAppID, const s8 * pAppName, int nInstNum, int nMsgQueueNum, CBMsgEntry pfMsgCB);

//=============================================================================
//函 数 名：aclDestroyApp
//功	    能：销毁指定的APP
//算法实现：
//全局变量：
//参	    数：nNewAppID： 要销毁的APPID号
//注    意:
//=============================================================================
ACL_API int aclDestroyApp(u16 nAppID);


//=============================================================================
//函 数 名：aclSetInstStatus
//功	    能：设置当前Instance的状态
//算法实现：
//全局变量：
//参	    数：hAclInst: 要修改的Instance的句柄
//           nStatus: 修改的状态
//注    意:设置默认状态机状态 为0 空闲，1为忙，可以从1开始定义符合
//         应用需求的InstStatus 定义
//         
//=============================================================================
ACL_API void aclSetInstStatus(HACLINST hAclInst,int nStatus);

//=============================================================================
//函 数 名：aclGetInstNum
//功	    能：获得Instance下标
//算法实现：
//全局变量：
//参	    数：hAclInst: Instance句柄
//           
//返    回:Instance下标
//注    意:Instance下标从0开始
//         
//         
//=============================================================================
ACL_API int aclGetInstNum(HACLINST hAclInst);


//=============================================================================
//函 数 名：aclGetInstStatus
//功	    能：获得当前Instance的状态
//算法实现：
//全局变量：
//参	    数：hAclInst: Instance目标的句柄
//注    意:返回值将是aclSetInstStatus的设置值，可以根据项目需要修改
//         
//=============================================================================
ACL_API INST_STATUS aclGetInstStatus(HACLINST hAclInst);



//=============================================================================
//函 数 名：aclGetInstStatus
//功	    能：获得当前Instance的ID
//算法实现：
//全局变量：
//参	    数：hAclInst: Instance目标的句柄
//注    意:返回值将是aclSetInstStatus的设置值，可以根据项目需要修改
//         
//=============================================================================
ACL_API int aclGetInstID(HACLINST hAclInst);


//=============================================================================
//函 数 名：aclGetInstMsgPoolLeft
//功	    能：获取当前Instance的缓冲池残存消息数量
//算法实现：
//全局变量：
//参	    数：hAclInst: Instance的句柄
//返        回：缓冲池消息数量
//         
//=============================================================================
ACL_API int aclGetInstMsgPoolLeft(HACLINST hAclInst);

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
ACL_API int aclCreateTcpNode(u16 wPort);


//=============================================================================
//函 数 名：aclInstPost
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
//注    意: 消息节点号由获取的
//=============================================================================
ACL_API int aclInstPost(HACLINST hInst,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen);


//=============================================================================
//函 数 名：aclPost
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
ACL_API int aclPost(u32 dwSrcAppInstAddr,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen);


//=============================================================================
//函 数 名：aclTCPConnect
//功	    能：作为客户端连接ACL服务器
//算法实现：
//全局变量：
//参	    数：pNodeIP: 服务端IP
//            wPort: 服务端端口
//注    意: 
//=============================================================================
ACL_API int aclTCPConnect(s8 * pNodeIP, u16 wPort);

//=============================================================================
//函 数 名：aclConnClose
//功	    能：关闭客户端连接ACL服务器会话
//算法实现：
//全局变量：
//参	    数：pNodeIP: 服务端IP
//            wPort: 服务端端口
//注    意: 
//=============================================================================
ACL_API int aclConnClose(int nNode);


//=============================================================================
//函 数 名：aclDelay
//功	    能：延迟函数
//算法实现：
//全局变量：
//参	    数：dwMsecs: 延迟时间，单位为毫秒
//注    意: 
//=============================================================================
ACL_API void aclDelay(u32 dwMsecs);


//=============================================================================
//函 数 名：setTimer_b
//功	    能：设置定时器
//算法实现：
//全局变量：
//参	    数：
//        nTime: 设置超时时间(单位为毫秒)
// dwAppInsAddr: 超时后，定时器消息发往的位置
//     wMsgType: 定义定时器消息号
//      bRepeat: 是否重复
//   pfTimerNtf:注册的回调函数，超时后会通知
//     pContent: 附带数据
//  nContentLen: 附带数据长度
//注    意: 
//         定义的定时器消息号必须大于  ACL_USER_MSG_BASE，否则消息会被过滤掉
//         回调函数如果不为空，则消息将不再发往指定的ID，此时 dwAppInsAddr 无效
//
//=============================================================================
ACL_API ACL_HANDLE setTimer_b(int nTime, 
    u32 dwAppInsAddr,
    u16 wMsgType, 
    BOOL32 bRepeat, 
    PF_TIMER_NTF pfTimerNtf, 
    void * pContent, 
    int nContentLen);


//=============================================================================
//函 数 名：setTimer
//功	    能：设置定时器
//算法实现：
//全局变量：
//参	    数：
//        nTime: 设置超时时间(单位为毫秒)
// dwAppInsAddr: 超时后，定时器消息发往的位置
//     wMsgType: 定义定时器消息号
//注    意: 此函数作为通用定时器的函数，默认不循环计数
//=============================================================================
ACL_API ACL_HANDLE setTimer(int nTime, u32 wAppInsAddr, u16 wMsgType);


//=============================================================================
//函 数 名：killTimer
//功	    能：杀死定时器
//算法实现：
//全局变量：
//参	    数：
//        wMsgType: 设置定时器时注册的定时器的消息
//注    意: 所有定义消息为 wMsgType的定时器对象都会被杀死
//=============================================================================
ACL_API int killTimer(u16 wMsgType);


//=============================================================================
//函 数 名：aclPrintf
//功	    能：输出函数
//算法实现：
//全局变量：
//参	    数：
//        wMsgType: 设置定时器时注册的定时器的消息
//注    意: 所有定义消息为 wMsgType的定时器对象都会被杀死
//=============================================================================
ACL_API int aclPrintf(int bOpenPrint,int bSaveLog,const char * param,...);


//=============================================================================
//函 数 名：aclRegCommand
//功	    能：调试函数注册
//算法实现：
//全局变量：
//参	    数：pCmd: 注册调用函数的命令
//         pFunc: 注册函数指针
//       pPrompt: 命令功能提示
//注    意:
//=============================================================================
ACL_API int aclRegCommand(const char * pCmd, void * pFunc, const char * pPrompt);


//=============================================================================
//函 数 名：setTelnetPrompt
//功	    能：设置Telnet调试窗口的提示符
//算法实现：
//全局变量：
//参	    数：nPort: 监听节点的监听端口
//        pPrompt: 提示符内容
//注    意: 默认提示符为 "acl_Telnet->"
//=============================================================================
ACL_API int setTelnetPrompt(int nPort, const char * pPrompt);


//=============================================================================
//函 数 名：aclCreateThread
//功	    能：创建线程
//算法实现：
//全局变量：
//参	    数：ptAclThread: 线程结构体
//         pfTaskEntry: 线程函数
//         pTaskParam: 额外参数
//注    意: 
//=============================================================================
ACL_API int aclCreateThread(PTACL_THREAD ptAclThread, PF_THREAD_ENTRY pfTaskEntry, void * pTaskParam);

//=============================================================================
//函 数 名：aclCreateSem
//功	    能：创建信号灯
//算法实现：
//全局变量：
//参	    数：phAclSem: 信号灯句柄
//         nMaxSemNum: 信号灯数量
//注    意: 
//=============================================================================
int aclCreateSem(H_ACL_SEM * phAclSem, int nMaxSemNum);

//=============================================================================
//函 数 名：aclDestorySem
//功	    能：销毁信号灯
//算法实现：
//全局变量：
//参	    数：phAclSem: 信号灯句柄
//注    意: 
//=============================================================================
int aclDestorySem(H_ACL_SEM * phAclSem);

//=============================================================================
//函 数 名：alcCheckGetSem
//功	    能：获取信号灯(阻塞)
//算法实现：
//全局变量：
//参	    数：phAclSem: 信号灯句柄
//注    意: 
//=============================================================================
int alcCheckGetSem(H_ACL_SEM * phAclSem);

//=============================================================================
//函 数 名：aclCheckGetSem_b
//功	    能：获取信号灯(非阻塞)
//算法实现：
//全局变量：
//参	    数：phAclSem: 信号灯句柄,nMaxWaitTime 最长等待时间
//注    意: 当等待时间内没有获得信号灯，则返回
//=============================================================================
int aclCheckGetSem_b(H_ACL_SEM * phAclSem,u32 nMaxWaitTime);

//=============================================================================
//函 数 名：aclReleaseSem
//功	    能：释放信号灯
//算法实现：
//全局变量：
//参	    数：phAclSem: 信号灯句柄
//注    意: 
//=============================================================================
int aclReleaseSem(H_ACL_SEM * phAclSem);


//=============================================================================
//函 数 名：aclCreateLock
//功	    能：创建互斥锁
//算法实现：
//全局变量：
//参	    数：phAclSem: 互斥锁句柄
//注    意: 
//=============================================================================
int aclCreateLock(H_ACL_LOCK * phAclLock, ACL_LOCK_ATTR * pAclLockAttr);

//=============================================================================
//函 数 名：aclDestoryLock
//功	    能：销毁互斥锁
//算法实现：
//全局变量：
//参	    数：phAclSem: 互斥锁句柄
//注    意: 
//=============================================================================
int aclDestoryLock(H_ACL_LOCK  hAclLock);

//=============================================================================
//函 数 名：lockLock_t
//功	    能：上锁(非阻塞)
//算法实现：
//全局变量：
//参	    数：phAclSem: 互斥锁句柄，dwMaxWaitTime 上锁超时时间
//注    意: 超时如果无法上锁，则返回
//=============================================================================
int lockLock_t(H_ACL_LOCK  hAclLock, u32 dwMaxWaitTime);

//=============================================================================
//函 数 名：lockLock
//功	    能：上锁(阻塞)
//算法实现：
//全局变量：
//参	    数：phAclSem: 互斥锁句柄
//注    意:
//=============================================================================
int lockLock(H_ACL_LOCK  hAclLock);

//=============================================================================
//函 数 名：unlockLock
//功	    能：上锁(阻塞)
//算法实现：
//全局变量：
//参	    数：phAclSem: 互斥锁句柄
//注    意:
//=============================================================================
int unlockLock(H_ACL_LOCK  hAclLock);


//=============================================================================
//函 数 名：aclGetClientInfoBySessionId
//功	    能：根据指定回话id获取对应客户端的ip地址（点分十进制）
//算法实现：
//全局变量：
//参	    数：dwNodeID: 回话id
//注    意:
//=============================================================================
ACL_API int aclGetClientInfoBySessionId(u32 dwNodeID, TPeerClientInfo& tPeerCliInfo);


//<-----------------------Interface--------------------
// #ifdef __cplusplus
// }
// #endif  //extern "C"

#endif
