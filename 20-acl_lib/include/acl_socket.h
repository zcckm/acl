//******************************************************************************
//ģ����	�� acl_socket
//�ļ���	�� acl_socket.h
//����	�� zcckm
//�汾	�� 1.0
//�ļ�����˵��:
//ACL socket���ӹ���
//------------------------------------------------------------------------------
//�޸ļ�¼:
//2015-01-22 zcckm ����
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
	void * pPktBufMng; //��������socket��Ҫһ����ʱ���壬���������������⣬��ʼsizeΪ MAX_RECV_PACKET_SIZE*2
	u32 dwCurePBMSize; // ��ǵ�ǰpPktBufMngռ�ÿռ�Ĵ�С
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

//NODE_MAP��Ϊ��ֹnode��ͻ
//���ۿͻ��˻��߷���ˣ�����Ҫ����nodeӳ��

//�ڵ�ӳ��˵��
//m_nNodeMap[X]ȫ��ID��
//TSockNode->m_dwNodeNum ����ID��

//��Ϊ�����ʱ:�ͻ����������ӷ���ˣ�����˷���ȫ��ID��
//��ʱ����ID��Ҳ��ͬ���������ͻ�����Ϊ�ỰID

//��Ϊ�ͻ���ʱ:�ͻ����յ�����˷��������ID��Ϊ�ỰID
//Ȼ�󱾵ط��䱾��ȫ��ID��ʹ��

//������Ϣʱ����Ҫʹ��ȫ��ID��
//�����ڲ����Node����L->N����
//������Ϣʱ�������ڲ����Node����N->L����,Ȼ��֪ͨ���û�
//��˰����շ���ʱ�򣬶���һ��
typedef struct tagSockManage
{
	//Node 0 is for listen
	TSockNode * m_ptSockNodeArr;
	//nodemap: to avode node conflict
	u32 m_dwNodeMap[MAX_NODE_SUPPORT];//����ȫ�ֽڵ��->��SockNode��Ӧ
	int m_nTotalNode;

	fd_set m_fdWrite;
	fd_set m_fdRead;
	fd_set m_fdError;
    //���л��socketλ����Ϣ�����ڼ��ϴ���
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

//����
//->begin

//->end
//����
// #ifdef __cplusplus
// }
// #endif //extern "C"

#endif

