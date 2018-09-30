//******************************************************************************
//ģ����  �� acl_c
//�ļ���  �� acl_c.h
//����    �� zcckm
//�汾    �� 1.0
//�ļ�����˵��:
//acl���Žӿ�ʵ��
//------------------------------------------------------------------------------
//�޸ļ�¼:
//2014-12-30 zcckm ����
//******************************************************************************
#ifndef _ACL_C_H_
#define _ACL_C_H_

//linux �±�����Ҫ�򿪴˺궨��
//#define _LINUX_

//#include "acl_unittest.h"
#include "acltype.h"

#ifdef WIN32
#pragma warning (disable:4005)
#endif

///////////////////////////////////////////////////////////////////
//	_LINUX_ ����ϵͳͷ�ļ�
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
//	Win32 ����ϵͳͷ�ļ�
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
//    ����ϵͳ�޹غ궨��
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

//��¼�ͻ�������������ip�Ͷ˿���Ϣ
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

//acl��Ϣͷ���ṹ����
typedef struct tagOspMessage
{    
    u32 m_dwSessionID;   //�洢�ڵ�ID���ɷ���˷���
    u32 m_dwDstIID;      //Ŀ��Ӧ��ʵ��    
    u32 m_dwSrcIID;      //ԴĿ��ʵ��    
    u16 m_wMsgStatus;    //��Ϣ����   //0: default 1:throw away 
    u16 m_wMsgType;      //��Ϣ�� 
    u32 m_dwPackLen;      //���ܳ��� ���ܺ����и�������
    u32 m_dwContentLen;   //��Ϣ�峤��    
    u8 *m_pContent;      //��Ϣ��    
}
#if defined(_LINUX_)
__attribute__ ((packed)) 
#endif
    TAclMessage;

//�ṹ����붨������������
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
//�� �� ����aclInit
//��	    �ܣ�aclģ���ʼ��
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����bTelnet�� �Ƿ�����telnet(��Windows����Ч)
//          wTelPort: telnet�����˿�
//ע    ��: ʹ��ACLǰ������ô˺������ڳ�ʼ������ģ��
//          
//=============================================================================
ACL_API int aclInit(BOOL bTelnet, u16 wTelPort);


//=============================================================================
//�� �� ����aclQuit
//��	    �ܣ�aclģ���˳�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//ע    ��: ���˳�ACLʱ������ô˺������ͷ�������Դ
//          
//=============================================================================
ACL_API void aclQuit();


//=============================================================================
//�� �� ����aclCreateApp
//��	    �ܣ�����APP
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����nNewAppID�� ������APPID
//           pAppName: APP���� ��Ϊ��
//           nInstNum: ����Instance����
//     nInstStackSize: Instance �̵߳Ķ�ջ��С
//            pfMsgCB: APP��Entry������APP���յ���Ϣ����֪ͨ�˺���
//ע    ��:APP�а�����ÿ��Instance���ж����Ĵ����̣߳�����Instance�������������Ϣ
//         ÿ��Instance�����Լ���״̬���������ڻص��������޸ĸ���Instance״̬��
//=============================================================================
ACL_API int aclCreateApp(u16 nNewAppID,const s8 * pAppName,int nInstNum,int nInstStackSize, CBMsgEntry pfMsgCB);


//=============================================================================
//�� �� ����aclCreateApp
//��	    �ܣ�����APP
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����nNewAppID�� ������APPID
//           pAppName: APP���� ��Ϊ��
//           nInstNum: ����Instance����
//     nInstStackSize: Instance �̵߳Ķ�ջ��С
//            pfMsgCB: APP��Entry������APP���յ���Ϣ����֪ͨ�˺���
//ע    ��:APP�а�����ÿ��Instance���ж����Ĵ����̣߳�����Instance�������������Ϣ
//         ÿ��Instance�����Լ���״̬���������ڻص��������޸ĸ���Instance״̬��
//=============================================================================
ACL_API int aclCreateApp1(u16 nNewAppID, const s8 * pAppName, int nInstNum, int nMsgQueueNum, CBMsgEntry pfMsgCB);

//=============================================================================
//�� �� ����aclDestroyApp
//��	    �ܣ�����ָ����APP
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����nNewAppID�� Ҫ���ٵ�APPID��
//ע    ��:
//=============================================================================
ACL_API int aclDestroyApp(u16 nAppID);


//=============================================================================
//�� �� ����aclSetInstStatus
//��	    �ܣ����õ�ǰInstance��״̬
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����hAclInst: Ҫ�޸ĵ�Instance�ľ��
//           nStatus: �޸ĵ�״̬
//ע    ��:����Ĭ��״̬��״̬ Ϊ0 ���У�1Ϊæ�����Դ�1��ʼ�������
//         Ӧ�������InstStatus ����
//         
//=============================================================================
ACL_API void aclSetInstStatus(HACLINST hAclInst,int nStatus);

//=============================================================================
//�� �� ����aclGetInstNum
//��	    �ܣ����Instance�±�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����hAclInst: Instance���
//           
//��    ��:Instance�±�
//ע    ��:Instance�±��0��ʼ
//         
//         
//=============================================================================
ACL_API int aclGetInstNum(HACLINST hAclInst);


//=============================================================================
//�� �� ����aclGetInstStatus
//��	    �ܣ���õ�ǰInstance��״̬
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����hAclInst: InstanceĿ��ľ��
//ע    ��:����ֵ����aclSetInstStatus������ֵ�����Ը�����Ŀ��Ҫ�޸�
//         
//=============================================================================
ACL_API INST_STATUS aclGetInstStatus(HACLINST hAclInst);



//=============================================================================
//�� �� ����aclGetInstStatus
//��	    �ܣ���õ�ǰInstance��ID
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����hAclInst: InstanceĿ��ľ��
//ע    ��:����ֵ����aclSetInstStatus������ֵ�����Ը�����Ŀ��Ҫ�޸�
//         
//=============================================================================
ACL_API int aclGetInstID(HACLINST hAclInst);


//=============================================================================
//�� �� ����aclGetInstMsgPoolLeft
//��	    �ܣ���ȡ��ǰInstance�Ļ���زд���Ϣ����
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����hAclInst: Instance�ľ��
//��        �أ��������Ϣ����
//         
//=============================================================================
ACL_API int aclGetInstMsgPoolLeft(HACLINST hAclInst);

//=============================================================================
//�� �� ����aclCreateTcpNode
//��	    �ܣ����������ڵ�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//      wPort: �����˿�
//ע    ��: �˺������ڴ��������ڵ㣬���ڵ��� newConnectProc����������
//          �������е���������
//=============================================================================
ACL_API int aclCreateTcpNode(u16 wPort);


//=============================================================================
//�� �� ����aclInstPost
//��	    �ܣ���ָ����InstanceͶ����Ϣ
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//            hInst: ����ָ����Instance���
// dwDstAppInstAddr: ��Ϣ��ַ(zheli )
//         dwNodeID: �ỰID�������ӽ�����˫��������һ��ID�����ڱ�ǵ�ǰ�Ự
//         wMsgType: ��Ϣ��
//         pContent: ��Ϣ����  
//      wContentLen: ��Ϣ���ݳ���
//ע    ��: ��Ϣ�ڵ���ɻ�ȡ��
//=============================================================================
ACL_API int aclInstPost(HACLINST hInst,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen);


//=============================================================================
//�� �� ����aclPost
//��	    �ܣ�����Ϣ���͵�Ҫ���inst��(���ػ���������)
//ע	    �⣺����һ��ȫ�ֺ���������������ط�����
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//        dwSrcAppInstAddr:Դ�ڵ�ID�����ͨ���ڲ�֪ͨ������ȫ����0
//        dwDstAppInstAddr:Ŀ�Ľڵ�ID����ͨ������checkMsgType���ַ�����Ϣ����
//        dwNodeID: �ڵ�ID��ÿ��������˫���������һ���µĽڵ�ID��ΪͨѶID
//					0��ʾĿ��Ϊ����
//        dwMsgType: ��Ϣ����
//        pContent: ������Ϣ��
//        dwContentLen: ��Ϣ��ߴ�
//ע    ��:pContent�����NULL����Ϊ�ڲ��п������̣���˱��뱣ָ֤��ֵָ����ȷλ��
//         ��д��ȷ��dwContentLenֵ, �������ɱ���
//=============================================================================
ACL_API int aclPost(u32 dwSrcAppInstAddr,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen);


//=============================================================================
//�� �� ����aclTCPConnect
//��	    �ܣ���Ϊ�ͻ�������ACL������
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����pNodeIP: �����IP
//            wPort: ����˶˿�
//ע    ��: 
//=============================================================================
ACL_API int aclTCPConnect(s8 * pNodeIP, u16 wPort);

//=============================================================================
//�� �� ����aclConnClose
//��	    �ܣ��رտͻ�������ACL�������Ự
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����pNodeIP: �����IP
//            wPort: ����˶˿�
//ע    ��: 
//=============================================================================
ACL_API int aclConnClose(int nNode);


//=============================================================================
//�� �� ����aclDelay
//��	    �ܣ��ӳٺ���
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����dwMsecs: �ӳ�ʱ�䣬��λΪ����
//ע    ��: 
//=============================================================================
ACL_API void aclDelay(u32 dwMsecs);


//=============================================================================
//�� �� ����setTimer_b
//��	    �ܣ����ö�ʱ��
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//        nTime: ���ó�ʱʱ��(��λΪ����)
// dwAppInsAddr: ��ʱ�󣬶�ʱ����Ϣ������λ��
//     wMsgType: ���嶨ʱ����Ϣ��
//      bRepeat: �Ƿ��ظ�
//   pfTimerNtf:ע��Ļص���������ʱ���֪ͨ
//     pContent: ��������
//  nContentLen: �������ݳ���
//ע    ��: 
//         ����Ķ�ʱ����Ϣ�ű������  ACL_USER_MSG_BASE��������Ϣ�ᱻ���˵�
//         �ص����������Ϊ�գ�����Ϣ�����ٷ���ָ����ID����ʱ dwAppInsAddr ��Ч
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
//�� �� ����setTimer
//��	    �ܣ����ö�ʱ��
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//        nTime: ���ó�ʱʱ��(��λΪ����)
// dwAppInsAddr: ��ʱ�󣬶�ʱ����Ϣ������λ��
//     wMsgType: ���嶨ʱ����Ϣ��
//ע    ��: �˺�����Ϊͨ�ö�ʱ���ĺ�����Ĭ�ϲ�ѭ������
//=============================================================================
ACL_API ACL_HANDLE setTimer(int nTime, u32 wAppInsAddr, u16 wMsgType);


//=============================================================================
//�� �� ����killTimer
//��	    �ܣ�ɱ����ʱ��
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//        wMsgType: ���ö�ʱ��ʱע��Ķ�ʱ������Ϣ
//ע    ��: ���ж�����ϢΪ wMsgType�Ķ�ʱ�����󶼻ᱻɱ��
//=============================================================================
ACL_API int killTimer(u16 wMsgType);


//=============================================================================
//�� �� ����aclPrintf
//��	    �ܣ��������
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//        wMsgType: ���ö�ʱ��ʱע��Ķ�ʱ������Ϣ
//ע    ��: ���ж�����ϢΪ wMsgType�Ķ�ʱ�����󶼻ᱻɱ��
//=============================================================================
ACL_API int aclPrintf(int bOpenPrint,int bSaveLog,const char * param,...);


//=============================================================================
//�� �� ����aclRegCommand
//��	    �ܣ����Ժ���ע��
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����pCmd: ע����ú���������
//         pFunc: ע�ắ��ָ��
//       pPrompt: �������ʾ
//ע    ��:
//=============================================================================
ACL_API int aclRegCommand(const char * pCmd, void * pFunc, const char * pPrompt);


//=============================================================================
//�� �� ����setTelnetPrompt
//��	    �ܣ�����Telnet���Դ��ڵ���ʾ��
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����nPort: �����ڵ�ļ����˿�
//        pPrompt: ��ʾ������
//ע    ��: Ĭ����ʾ��Ϊ "acl_Telnet->"
//=============================================================================
ACL_API int setTelnetPrompt(int nPort, const char * pPrompt);


//=============================================================================
//�� �� ����aclCreateThread
//��	    �ܣ������߳�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����ptAclThread: �߳̽ṹ��
//         pfTaskEntry: �̺߳���
//         pTaskParam: �������
//ע    ��: 
//=============================================================================
ACL_API int aclCreateThread(PTACL_THREAD ptAclThread, PF_THREAD_ENTRY pfTaskEntry, void * pTaskParam);

//=============================================================================
//�� �� ����aclCreateSem
//��	    �ܣ������źŵ�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: �źŵƾ��
//         nMaxSemNum: �źŵ�����
//ע    ��: 
//=============================================================================
int aclCreateSem(H_ACL_SEM * phAclSem, int nMaxSemNum);

//=============================================================================
//�� �� ����aclDestorySem
//��	    �ܣ������źŵ�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: �źŵƾ��
//ע    ��: 
//=============================================================================
int aclDestorySem(H_ACL_SEM * phAclSem);

//=============================================================================
//�� �� ����alcCheckGetSem
//��	    �ܣ���ȡ�źŵ�(����)
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: �źŵƾ��
//ע    ��: 
//=============================================================================
int alcCheckGetSem(H_ACL_SEM * phAclSem);

//=============================================================================
//�� �� ����aclCheckGetSem_b
//��	    �ܣ���ȡ�źŵ�(������)
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: �źŵƾ��,nMaxWaitTime ��ȴ�ʱ��
//ע    ��: ���ȴ�ʱ����û�л���źŵƣ��򷵻�
//=============================================================================
int aclCheckGetSem_b(H_ACL_SEM * phAclSem,u32 nMaxWaitTime);

//=============================================================================
//�� �� ����aclReleaseSem
//��	    �ܣ��ͷ��źŵ�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: �źŵƾ��
//ע    ��: 
//=============================================================================
int aclReleaseSem(H_ACL_SEM * phAclSem);


//=============================================================================
//�� �� ����aclCreateLock
//��	    �ܣ�����������
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: ���������
//ע    ��: 
//=============================================================================
int aclCreateLock(H_ACL_LOCK * phAclLock, ACL_LOCK_ATTR * pAclLockAttr);

//=============================================================================
//�� �� ����aclDestoryLock
//��	    �ܣ����ٻ�����
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: ���������
//ע    ��: 
//=============================================================================
int aclDestoryLock(H_ACL_LOCK  & hAclLock);

//=============================================================================
//�� �� ����lockLock_t
//��	    �ܣ�����(������)
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: �����������dwMaxWaitTime ������ʱʱ��
//ע    ��: ��ʱ����޷��������򷵻�
//=============================================================================
int lockLock_t(H_ACL_LOCK  & hAclLock, u32 dwMaxWaitTime);

//=============================================================================
//�� �� ����lockLock
//��	    �ܣ�����(����)
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: ���������
//ע    ��:
//=============================================================================
int lockLock(H_ACL_LOCK   & hAclLock);

//=============================================================================
//�� �� ����unlockLock
//��	    �ܣ�����(����)
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����phAclSem: ���������
//ע    ��:
//=============================================================================
int unlockLock(H_ACL_LOCK  & hAclLock);


//=============================================================================
//�� �� ����aclGetClientInfoBySessionId
//��	    �ܣ�����ָ���ػ�id��ȡ��Ӧ�ͻ��˵�ip��ַ�����ʮ���ƣ�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����dwNodeID: �ػ�id
//ע    ��:
//=============================================================================
ACL_API int aclGetClientInfoBySessionId(u32 dwNodeID, TPeerClientInfo& tPeerCliInfo);


//<-----------------------Interface--------------------
// #ifdef __cplusplus
// }
// #endif  //extern "C"

#endif
