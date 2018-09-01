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
//测试
//->begin

//->end
//测试
// #ifdef __cplusplus
// }
// #endif //extern "C"

#endif

