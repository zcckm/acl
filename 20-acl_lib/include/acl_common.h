//******************************************************************************
//模块名	： acl_common
//文件名	： acl_common.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL 各个模块可修改宏，内部共用函数，结构体定义等，对外隐藏
//------------------------------------------------------------------------------
//修改记录:
//2015-02-05 zcckm 创建
//2015-08-31 zcckm 修改功能
//******************************************************************************
#ifndef _ACL_COMMON_H_
#define _ACL_COMMON_H_
#include "acl_c.h"
#include <map>
#include <string>
typedef enum 
{
    MSG_3A            = 1,  //3A mess
    MSG_QUTE          = 2,//quit message
    MSG_HBMSG_REQ     = 3,//HB req
    MSG_HB_MSG,
    MSG_HB_MSG_ACK,
	MSG_DISCONNECT_NTF,
	MSG_3A_CHECK_REQ,//S->C 3A检查请求
	MSG_3A_CHECK_ACK,//C->S 3A检查请求响应
	MSG_TRY_NEGOT,
	MSG_NEGOT_CONFIM
}E_ACLSysMsg;

#define FLEX_LEN 0
typedef struct tagIDNegot
{
	E_ACLSysMsg m_msgIDNegot;
	u32 m_dwSessionID;
	u32 m_dwPayLoadLen;
	unsigned char m_arrPayLoad[FLEX_LEN];
}TIDNegot;

//MSG_3A_CHECK_REQ,MSG_3A_CHECK_ACK
typedef struct tag3ACheck
{
	u32 m_dwIsBigEden;
	u32 m_MagicNum;
}T3ACheckReq,T3ACheckAck;

//协商 MSG_TRY_NEGOT,MSG_NEGOT_CONFIM CS端发送相同结构体
typedef struct tagTryNegot
{
	u32 m_dwSessionID;
}TTryNegot;

typedef enum
{
    MSGMASK_DEFAULT = 0,
    MSGMASK_THROW_AWAY =1,
}ACL_MSG_MASK;

//heart beat time out 
#define HB_TIMEOUT 6000
//heart beat check time interval
#define HB_CHECK_ITRVL 2000


//ACL inner APP definition
#define MAX_APP_NAME_LEN 256 //max APP name Len
#define MAX_APP_NUM      20  //max APP number

#define NWPUSH_APP_NUM 0//用于网络消息推送的APP
#define NETWORK_MSG_PUSH_INST_NUM 1

#define HBDET_APP_NUM 21//用于心跳检测的APP
#define HB_DETECT_INST_NUM 1

#define TIMER_CB_APP_NUM 22//用于定时器回调函数的分离处理(避免拖累核心计数线程)
#define TIMER_CB_INST_NUM 10

//3A
#define TEMP_3A_CHECK_NUM 0x32

#define DEFAULT_INST_MSG_NUM 1000


typedef enum
{
    E_INST_STATE_INVALID = -1,
    E_INST_STATE_IDLE     = 0,
    E_INST_STATE_BUSY     = 1,
}INST_STATUS_;

//用户创建app参数结构
typedef struct tagAclAppParam
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
}TAclAppParam;

//socket
typedef enum
{
    E_LET_IT_GO   = 0,
    ESELECT_READ  = 0x01,//listen also using this for handle new connect
    ESELECT_WRITE = 0x02,
    ESELECT_ERROR = 0x04,
    ESELECT_CONN  = 0x08
} ESELECT;
static std::map<u16, std::string> mapSelectTypePrint =
{
	{ E_LET_IT_GO, "E_LET_IT_GO" },
	{ ESELECT_READ, "ESELECT_READ" },
	{ ESELECT_WRITE, "ESELECT_WRITE" },
	{ ESELECT_ERROR, "ESELECT_ERROR" },
	{ ESELECT_CONN, "ESELECT_CONN" }
};


typedef enum
{
    E_DATA_PROC = 0,
    E_3A_CONNECT = 1,
}ESOCKET_MANAGE;

#define MAX_NODE_SUPPORT 40

#ifndef WIN32
#define INVALID_SOCKET  (int)(~0)
#endif

typedef s32 (*FEvSelect)(H_ACL_SOCKET nFd, ESELECT eEvent, void* pContext);

//begin lock
#ifdef WIN32

#else

#ifndef INFINITE
#define INFINITE ((u32)(-1)>>1)
#endif

#endif
//end lock

#ifdef __cplusplus
extern "C" {
#endif //extern "C"


#ifdef __cplusplus
}
#endif //extern "C"

#endif

