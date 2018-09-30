//******************************************************************************
//模块名	： acl_socket
//文件名	： acl_socket.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL socket连接功能
//------------------------------------------------------------------------------
//修改记录:
//2015-01-22 zcckm 创建
//******************************************************************************
#ifndef _ACL_SOCKET_H_
#define _ACL_SOCKET_H_

#include "acltype.h"
#include "acl_common.h"
#define MAX_SEND_PACKET_SIZE 800*1024
#define MAX_RECV_PACKET_SIZE 800*1024
#define INIT_PACKET_BUFFER_MANAGER (MAX_RECV_PACKET_SIZE * 2)
#define RS_SKHNDBUF_LEN 1024//rcv dend shake hand buffer len
#define CHECK_DATA_LEN 64

// #ifdef __cplusplus
// extern "C" {
// #endif //extern "C"

typedef enum
{
    E_NT_INITED = 0,
    E_NT_CLIENT,
    E_NT_SERVER,
    E_NT_LISTEN,
}ENODE_TYPE;

typedef struct
{
	void * pPktBufMng; //接收类型socket需要一个临时缓冲，解决黏包，半包的问题，初始size为 MAX_RECV_PACKET_SIZE*2
	u32 dwCurePBMSize; // 标记当前pPktBufMng占用空间的大小
}TPktBufMng;

typedef struct
{
	H_ACL_SOCKET m_hSock;
	void * m_pContext;
	ESELECT m_eWaitEvent;
	FEvSelect m_pfCB;
	u32 m_dwNodeNum;//new socket have assign a node number may be need not
	BOOL m_bIsUsed;
	int m_nSelectCount;
	int m_nHBCount;
	ENODE_TYPE m_eNodeType; //Server Node Client Node Or
	TPktBufMng tPktBufMng; // packet buffer manager
}TSockNode;

//for store all socket node
//select&send operation is get socket here

//NODE_MAP：为防止node冲突
//无论客户端或者服务端，都需要进行node映射

//节点映射说明
//m_nNodeMap[X]全局ID号
//TSockNode->m_dwNodeNum 网络ID号

//当为服务端时:客户端主动连接服务端，服务端分配全局ID号
//此时网络ID号也相同，并发给客户端作为会话ID

//当为客户端时:客户端收到服务端分配的网络ID作为会话ID
//然后本地分配本地全局ID号使用

//发送消息时，需要使用全局ID号
//程序内部会对Node进行L->N操作
//接收消息时，程序内部会对Node进行N->L操作,然后通知给用户
//因此包在收发的时候，都有一步
typedef struct tagSockManage
{
	//Node 0 is for listen
	TSockNode * m_ptSockNodeArr;
	//nodemap: to avode node conflict
	u32 m_dwNodeMap[MAX_NODE_SUPPORT];//保存全局节点号->与SockNode对应
	int m_nTotalNode;

	fd_set m_fdWrite;
	fd_set m_fdRead;
	fd_set m_fdError;
    //集中活动的socket位置信息，便于集合处理
	int * pActSockPos;//Activated socket position array

	H_ACL_LOCK m_hLock;
	u32 m_dwTaskState;//
    ETASK_STATUS m_eMainTaskStatus;
    ETASK_STATUS m_eHBTaskStatus;
	TACL_THREAD m_tSelectThread;
	TACL_THREAD m_tHBThread;//heart beat thread
	int m_nHBCount; //heart beat count
}TSockManage;


typedef struct tagPeerClientInfo TPeerClientInfo;

//+++ begin interface
ACL_API int aclTCPConnect__(s8 * pNodeIP, u16 wPort);
ACL_API int aclConnClose__(int nNode);
//--- end interface

int  inet_aton(const char *cp, struct in_addr *ap);

ACL_API HSockManage aclSocketManageInit(ESOCKET_MANAGE eManageType);
ACL_API void aclSocketManageUninit(HSockManage hSockManage, ESOCKET_MANAGE eManageType);
ACL_API int aclInsertSelectLoop(HSockManage hSockMng, H_ACL_SOCKET hSock, FEvSelect pfCB, ESELECT eSelType, int nInnerNodeID, void * pContext);
ACL_API int aclRemoveSelectLoop(HSockManage hSockMng, H_ACL_SOCKET hSock);
ACL_API int aclInsertPBMAndSend(HSockManage hSockMng, H_ACL_SOCKET hSock,void * data, int nDataLen);

ACL_API H_ACL_SOCKET aclCreateSocket();
ACL_API H_ACL_SOCKET aclCreateSockNode(const char * pIPAddr,u16 wPort);
ACL_API int aclCreateNode(HSockManage hSockMng, const char * pIPAddr, u16 wPort,FEvSelect pfCB, void * pContext);
ACL_API H_ACL_SOCKET aclTcpAccept(H_ACL_SOCKET hListenSocket, u32* pdwPeerIP, u16* pwPeerPort);
ACL_API int aclTcpSend(H_ACL_SOCKET hSocket, s8 * pBuf ,s32 nLen);
ACL_API int aclTcpSendbyNode(HSockManage hSockMng, int nNode, s8 * pBuf ,s32 nLen);
ACL_API int aclTcpRecv(H_ACL_SOCKET hSocket, s8 * pBuf ,s32 nLen);
ACL_API int aclCloseSocket(H_ACL_SOCKET hAclSocket);
ACL_API int aclTcpListen(H_ACL_SOCKET hSocket, int nQueue);

ACL_API int aclCombPack(s8 * pCombBuf,u32 dwBufSize, TAclMessage * ptHead, char * pExtData, int nExtDataLen);
ACL_API int checkPack(char * pData, int nDataLen);
ACL_API int aclHandle3AData(char * pData, int nDataLen);


ACL_API u32 aclSetNodeMap(HSockManage hSockMng, u32 dwNewNode);
ACL_API u32 aclGlbNode2Net(HSockManage hSockMng, u32 dwCheckNode);
ACL_API int aclNetNode2Glb(HSockManage hSockMng, u32 dwNetNode);
ACL_API int aclNetNode2Pos(HSockManage hSockMng, u32 dwNetNode);
ACL_API int aclHBConfirm(HSockManage hSockMng, int nNode);
ACL_API BOOL IsBigEndian();

ACL_API int aclGetCliInfoBySpecSessionId(HSockManage hSockMng, u32 dwCheckNode, TPeerClientInfo &tPeerCliInfo);

//测试
//->begin

//->end
//测试
// #ifdef __cplusplus
// }
// #endif //extern "C"

#endif

