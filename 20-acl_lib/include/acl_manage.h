//******************************************************************************
//ģ����	�� acl_manage
//�ļ���	�� acl_manage.h
//����	�� zcckm
//�汾	�� 1.0
//�ļ�����˵��:
//����ACL�����Դ
//------------------------------------------------------------------------------
//�޸ļ�¼:
//2015-01-16 zcckm ����
//******************************************************************************
#ifndef _ACL_MANAGE_H_
#define _ACL_MANAGE_H_

#include "acl_common.h"
#include "acl_task.h"
#include "acl_msgqueue.h"

// #ifdef __cplusplus
// extern "C" {
// #endif //extern "C"

typedef struct tagPeerClientInfo TPeerClientInfo;
//++++++++begin manage Interface
ACL_API int aclInit(BOOL bTelnet, u16 wTelPort);
ACL_API void aclQuit();

ACL_API int aclCreateApp__(u16 nNewAppID,const s8 * pAppName,int nInstNum,int nInstStackSize, CBMsgEntry pfMsgCB);

ACL_API int aclCreateApp__b(TAclAppParam * pTAclAppParam);
ACL_API int aclDestroyApp(u16 nAppID);


ACL_API void aclSetInstStatus(HACLINST hAclInst,int nStatus);
ACL_API INST_STATUS aclGetInstStatus(HACLINST hAclInst);
ACL_API int aclGetInstID(HACLINST hAclInst);

ACL_API int aclCreateTcpNode(u16 wPort);

ACL_API int aclInstPost__(HACLINST hInst,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen);
ACL_API int aclPost__(u32 dwSrcAppInstAddr,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,u8 * pContent,u32 dwContentLen);
//--------end manage Interface

ACL_API int aclGetClientInfoBySessionId(u32 dwNodeID, TPeerClientInfo& tPeerCliInfo);

//begin manage

typedef enum
{
    E_PMT_L_L = 0,// local to local//
    E_PMT_L_N ,	  // local to net  // node need G->N
    E_PMT_N_L ,   // net to local  // node need N->G
    E_PMT_L_H ,   // local to HB Detect
    E_PMT_N_H ,   // net to HB Detect
}PUSH_MSG_TYPE;

ACL_API int aclMsgPush(u32 dwSrcAppInstAddr,u32 dwDstAppInstAddr,u32 dwNodeID,u16 wMsgType,void * pContent,u32 dwContentLen,PUSH_MSG_TYPE eMsgMask);
ACL_API HSockManage getSockDataManger();
ACL_API u32 aclSessionIDGenerate();
//end manage
// #ifdef __cplusplus
// }
// #endif //extern "C"

#endif

