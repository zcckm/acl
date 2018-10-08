//******************************************************************************
//ģ����	�� acl_socket
//�ļ���	�� acl_socket.c
//����	�� zcckm
//�汾	�� 1.0
//�ļ�����˵��:
//ACL socket���ӹ���
//------------------------------------------------------------------------------
//�޸ļ�¼:
//2015-01-22 zcckm ����
//******************************************************************************

#include "acl_socket.h"
#include "acl_time.h"
#include "acl_telnet.h"
#include "acl_lock.h"
#include "acl_manage.h"
#include "acl_memory.h"
#include "acl_msgqueue.h"
#include "acl_task.h"

//socket manage struct
// with one socket Manage include many node
//             
//             ---- node    
//SockManage------- node    --- SOCKET
//             ---- node ------ WaitEvent
//                          --- content 
//                          --- RegCallBack
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
typedef struct
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

//3A
#define SELECT_3A_INTERVAL 100
//3A�̻߳��ⳬʱ����������ӵ�Socketû���ڴ�ʱ���������֤�����ӻᱻ����
#define CHECK_3A_TIMEOUT 3000

//DataProc
#define SELECT_DP_INTERVAL 100


#ifdef WIN32
int  __stdcall aclDataProcThread(void * pParam);
#elif defined _LINUX_
int  aclDataProcThread(void * pParam);
#endif

#ifdef WIN32
int __stdcall aclConnect3AThread(void * pParam);
#elif defined _LINUX_
int  aclConnect3AThread(void * pParam);
#endif

#ifdef WIN32
unsigned int  __stdcall aclHBDetectThread(void * pParam);
#elif defined _LINUX_
void * aclHBDetectThread(void * pParam);
#endif

extern s32 aclNewMsgProcess(H_ACL_SOCKET nFd, ESELECT eEvent, void* pContext);

//���ú��� ������һ��ָ���󶨶˿ڵı��ؼ����㣬��ʱ�������߳�Ϊacl����ˣ�����ͻ��˵��������󣬸�����䱾�����ȫ�ֻỰID
ACL_API H_ACL_SOCKET aclCreateSocket()
{
	H_ACL_SOCKET hSocket = INVALID_SOCKET;
	hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == hSocket)
	{
        ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR,"[aclCreateSocket] create socket failed\n");
		return INVALID_SOCKET;
	}

	/* set socket option */
	{
		struct linger Linger;
		s32 yes = TRUE;
		u32 on = TRUE;

		Linger.l_onoff = TRUE; 
		Linger.l_linger = 0;

		//setsockopt(hSocket, SOL_SOCKET, SO_LINGER, (s8*)&Linger, sizeof(Linger));
		setsockopt(hSocket, SOL_SOCKET, SO_KEEPALIVE, (s8*)&yes, sizeof(yes));
		setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (s8*)&on, sizeof(on));
	}
	return hSocket;
}

ACL_API int aclHandle3AData(char * pData, int nDataLen)
{
	char * pExtData = NULL;
	int nRet = 0;
	int i = 0;
	nRet = checkPack(pData, nDataLen);
	if (ACL_ERROR_NOERROR != nRet)
	{
		return nRet;
	}

	pExtData = (char *)(pData + sizeof(TAclMessage));
    if (IsBigEndian() == pExtData[0])//both Endian is matched
    {
        ACL_DEBUG(E_MOD_MSG,E_TYPE_NOTIF, "[aclHandle3AData] server and Current Env are both running at %s mode\n", 
            IsBigEndian()?"BigEndian":"LittleEndian");
    }
    else
    {
        ACL_DEBUG(E_MOD_MSG,E_TYPE_NOTIF, "[aclHandle3AData] server is running at %s mode but Current Env is running at %s mode\n", 
            pExtData[0]?"BigEndian":"LittleEndian", IsBigEndian()?"BigEndian":"LittleEndian");
    }
	for (i = 0; i < nDataLen; i++)
	{
		pExtData[i] ^= TEMP_3A_CHECK_NUM;
	}
    if (IsBigEndian())
    {        
        pExtData[0] = 1;
    }
    else
    {
        pExtData[0] = 0;
    }
    pExtData[0] ^= TEMP_3A_CHECK_NUM;

    
	return ACL_ERROR_NOERROR;
}

ACL_API int checkPack(char * pData, int nDataLen)
{
	TAclMessage * ptAclMsg = NULL;
	if (NULL == pData)
	{
		return ACL_ERROR_PARAM;
	}
	ptAclMsg = (TAclMessage *)pData;
	if (ptAclMsg->m_dwPackLen != nDataLen || //pack size mismatch
		nDataLen - sizeof(TAclMessage) != ptAclMsg->m_dwContentLen)//ext data size mismatch
	{
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[checkPack] check packet failed, Recv DataLen:%d, ptAclMsg->m_dwPackLen = %d contentLen %d sizeofTaclMsg:%d\n", nDataLen, ptAclMsg->m_dwPackLen, ptAclMsg->m_dwContentLen, sizeof(TAclMessage));
		return ACL_ERROR_INVALID;
	}
	return ACL_ERROR_NOERROR;
}

//=============================================================================
//�� �� ����aclTCPConnect
//��	    �ܣ���Ϊ�ͻ�������ACL������
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����pNodeIP: �����IP
//            wPort: ����˶˿�
//ע    ��: 
//=============================================================================
ACL_API int aclTCPConnect__(s8 * pNodeIP, u16 wPort)
{
	H_ACL_SOCKET hSocket = INVALID_SOCKET;
	char szRcvSnd3AData[RS_SKHNDBUF_LEN] = {0};
	int nRet = 0;
	int nRecvData = 0;
	u32 dwInnerID = 0;
	struct sockaddr_in nAddr;
//	struct in_addr tAddr;

	if (NULL == pNodeIP)
	{
		return ACL_ERROR_PARAM;
	}

	hSocket = aclCreateSocket();
	if (INVALID_SOCKET == hSocket)
	{
		return ACL_ERROR_INVALID;
	}
//	inet_addr(pNodeIP);
//	inet_aton(pNodeIP,&tAddr);

	nAddr.sin_family = AF_INET; 
	nAddr.sin_addr.s_addr = inet_addr(pNodeIP);
	nAddr.sin_port = htons(wPort);
	ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "[aclTCPConnect] start Connect IP:%s PORT:%d...",pNodeIP, ntohs(nAddr.sin_port));
	nRet = connect(hSocket, (struct sockaddr*)&nAddr, sizeof(nAddr));
	if (nRet < 0)
	{
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF,"failed\n");
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR,"[acl_sock] connect failed EC:%d\n",nRet);
		aclCloseSocket(hSocket);
		return ACL_ERROR_FAILED;
	}
	ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF,"ok\n");
	memset(szRcvSnd3AData,0,RS_SKHNDBUF_LEN);

    ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "trying get recv 3A data...");
	nRecvData = aclTcpRecv(hSocket, szRcvSnd3AData, RS_SKHNDBUF_LEN);
	if (nRecvData < 0)
	{ 
        ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "failed\n");
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR,"[aclTCPConnect] rcv 3A data failed EC:%d\n",nRet);
		return ACL_ERROR_FAILED;
	}
    ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "ok\n");
	nRet = aclHandle3AData(szRcvSnd3AData, nRecvData);
	if (ACL_ERROR_NOERROR != nRet)
	{
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR,"[aclTCPConnect] aclHandle3AData EC:%d\n",nRet);
		return ACL_ERROR_FAILED;
	}
    ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "[aclTCPConnect] trying send 3A ACK data...");
	nRet = aclTcpSend(hSocket, szRcvSnd3AData, nRecvData);
	if (nRet < 0)
	{
        ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "failed\n");
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR,"[aclTCPConnect] send 3A data error EC:%d\n",nRet);
		return ACL_ERROR_FAILED;
	}
    ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "ok\n");
	nRecvData = 0;
	memset(szRcvSnd3AData,0,RS_SKHNDBUF_LEN);
	ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "[aclTCPConnect]start rcv 3A ack packet\n");
	nRecvData = aclTcpRecv(hSocket, szRcvSnd3AData, RS_SKHNDBUF_LEN);

	nRet = checkPack(szRcvSnd3AData, nRecvData);
	if (nRet < 0)
	{
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTCPConnect] recv node info failed EC:%d\n",nRet);
		return ACL_ERROR_FAILED;
	}
	dwInnerID = ((TAclMessage *)szRcvSnd3AData)->m_dwSessionID;
	//current connect is work now,insert to selectLoop now
	ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "[aclTCPConnect] SOCK[%d] connect success, insert select loop inner ID %d\n", hSocket, dwInnerID);
	nRet = aclInsertSelectLoop(getSockDataManger(), hSocket, aclNewMsgProcess, ESELECT_READ, dwInnerID, getSockDataManger());
	return nRet;//((TAclMessage *)szRcvSnd3AData)->m_dwSessionID;
}

//=============================================================================
//�� �� ����aclConnClose__
//��	    �ܣ���Ϊ�ͻ�������ACL������
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����nNode:���ӽڵ�
//ע    ��: 
//=============================================================================
ACL_API int aclConnClose__(int nNode)
{
	TSockManage * ptSockManage = (TSockManage *)getSockDataManger();
	TSockNode * ptNewNode = NULL;
	u32 dwFindNode = (u32)nNode;
	int i = 0;
	CHECK_NULL_RET_INVALID(ptSockManage);
	lockLock(ptSockManage->m_hLock);
	BOOL bFind = FALSE;
	for (i = 0; i < MAX_NODE_SUPPORT; i++)
	{
		//����Ѱ����Ҫ�رյĽڵ�����λ��
		if (ptSockManage->m_dwNodeMap[i] == dwFindNode)
		{
			bFind = TRUE;
			break;
		}
	}
	if (!bFind)
	{
		unlockLock(ptSockManage->m_hLock);
		return ACL_ERROR_FAILED;
	}

	//�ҵ��ڵ��� 
	if (!ptSockManage->m_ptSockNodeArr[i].m_bIsUsed)
	{
		unlockLock(ptSockManage->m_hLock);
		return ACL_ERROR_FAILED;
	}
    aclRemoveSelectLoopUnsafe(getSockDataManger(), ptSockManage->m_ptSockNodeArr[i].m_hSock);
	unlockLock(ptSockManage->m_hLock);
	return ACL_ERROR_NOERROR;

}


ACL_API H_ACL_SOCKET aclCreateSockNode(const char * pIPAddr,u16 wPort)
{
    H_ACL_SOCKET hSocket = INVALID_SOCKET;
    struct in_addr nAddr;
    
    if (NULL == pIPAddr)
    {
		return INVALID_SOCKET;
    }
    inet_aton(pIPAddr, &nAddr);
    hSocket = aclCreateSocket();
	if (INVALID_SOCKET ==hSocket)
	{
		return INVALID_SOCKET;
	}
    //if (wPort != 0)
    {
        struct sockaddr_in tSvrINAddr;
        memset(&tSvrINAddr, 0, sizeof(tSvrINAddr));
        tSvrINAddr.sin_family = AF_INET; 
        tSvrINAddr.sin_port = htons(wPort);
        tSvrINAddr.sin_addr.s_addr = nAddr.s_addr;

        if (bind(hSocket, (struct sockaddr *)&tSvrINAddr, sizeof(tSvrINAddr)) < 0)
        {
            ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR,"[aclCreateSockNode] bind failed. socket:%d\n",hSocket);
            return INVALID_SOCKET;
        }
    }
	if (aclTcpListen(hSocket, 10) < 0)
	{
		return INVALID_SOCKET;
	}
    return hSocket;
}


ACL_API int aclCloseSocket(H_ACL_SOCKET hAclSocket)
{
    s32 nRet;
    if (INVALID_SOCKET == hAclSocket)
    {
        return ACL_ERROR_INVALID;
    }
#ifdef WIN32
    nRet = closesocket(hAclSocket);
#elif defined (_LINUX_)
    nRet = close(hAclSocket);
#endif
    
    return nRet;
}


// convert ip str to network u32 type
int  inet_aton(const char *cp, struct in_addr *ap)
{
    int dots = 0;
    register u_long acc = 0, addr = 0;

    do {
        register char cc = *cp;

        switch (cc) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            acc = acc * 10 + (cc - '0');
            break;

        case '.':
            if (++dots > 3) {
                return 0;
            }
            /* Fall through */

        case '\0':
            if (acc > 255) {
                return 0;
            }
            addr = addr << 8 | acc;
            acc = 0;
            break;

        default:
            return 0;
        }
    } while (*cp++) ;

    /* Normalize the address */
    if (dots < 3) {
        addr <<= 8 * (3 - dots) ;
    }

    /* Store it if requested */
    if (ap) {
        ap->s_addr = htonl(addr);
    }

    return 1;    
}

ACL_API int aclTcpListen(H_ACL_SOCKET hSocket, int nQueue)
{
    if ((listen(hSocket, nQueue)) < 0)
    {
       ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTcpListen] listen failed. socket:%d\n", hSocket);
        return ACL_ERROR_INVALID;
    }
    return ACL_ERROR_NOERROR;
}


ACL_API H_ACL_SOCKET aclTcpAccept(H_ACL_SOCKET hListenSocket, u32* pdwPeerIP, u16* pwPeerPort)
{
    struct sockaddr_in tAddr;
#ifdef WIN32
    int nLen;
#elif defined (_LINUX_)
    socklen_t nLen;
#endif
    
    H_ACL_SOCKET hNewSocket;

    if (INVALID_SOCKET == hListenSocket)
    {
        return -1;
    }

    memset(&tAddr, 0, sizeof(tAddr));
    nLen = (int)sizeof(tAddr);

    hNewSocket = accept(hListenSocket, (struct sockaddr *)&tAddr, &nLen);
    if (INVALID_SOCKET == hNewSocket)
    {
        return INVALID_SOCKET;
    }

    if (NULL != pdwPeerIP)
    {
        *pdwPeerIP = tAddr.sin_addr.s_addr;
    }
    if (NULL != pwPeerPort)
    {
        *pwPeerPort = ntohs(tAddr.sin_port);
    }

    return hNewSocket;
}

ACL_API int aclTcpSend(H_ACL_SOCKET hSocket, s8 * pBuf ,s32 nLen)
{	
	int nRet = 0;
	CHECK_NULL_RET_INVALID(pBuf)
	if (INVALID_SOCKET == hSocket)
	{
		return ACL_ERROR_INVALID;
	}
	nRet = send(hSocket,pBuf,nLen,0);
    if (nRet < 0)
    {
        //�������Ӵ�ӡ��Ϣ����������telnet����Ϣʱ��Ͽ����������ѭ��
        return ACL_ERROR_FAILED;
    }
	return ACL_ERROR_NOERROR;
}


//=============================================================================
//�� �� ����aclTcpSendbyNode
//��	  �ܣ�ͨ��ָ��node����socket��Ϣ(�����Nodeָ����Node��������ʱ����˷���)
//�㷨ʵ�֣�
//ע    ��:
//=============================================================================
ACL_API int aclTcpSendbyNode(HSockManage hSockMng, int nNode, s8 * pBuf ,s32 nLen)
{
	TSockManage * tSockMng = NULL;
	TSockNode * ptSockNode = NULL;
	TAclMessage * ptAclMsg = NULL;
	int nRet = 0;
	u32 dwNodePos = 0;
	tSockMng = (TSockManage *)hSockMng;
	if (0 == nNode || 
		NULL == hSockMng || 
		0 == nLen)
	{
		return ACL_ERROR_PARAM;
	}

	ptAclMsg = (TAclMessage *)pBuf;
	dwNodePos = aclNetNode2Pos(hSockMng, nNode);
    if (-1 == dwNodePos)
    {
        ACL_DEBUG(E_MOD_NETWORK,E_TYPE_ERROR,"[aclTcpSendbyNode] there is no matched Net node to send message\n");
        return ACL_ERROR_INVALID;
    }

	ptSockNode = &tSockMng->m_ptSockNodeArr[dwNodePos];
	ptAclMsg->m_dwSessionID = ptSockNode->m_dwNodeNum;
	if (INVALID_SOCKET == ptSockNode->m_hSock)
	{
		return ACL_ERROR_INVALID;
	}
	nRet = aclTcpSend(ptSockNode->m_hSock, pBuf, nLen);
	if (nRet < 0)
	{
        ACL_DEBUG(E_MOD_NETWORK,E_TYPE_WARNNING,"[aclTcpSendbyNode] aclTcpSendbyNode send failed\n");
	}
	return nRet;
}

ACL_API int aclTcpRecv(H_ACL_SOCKET hSocket, s8 * pBuf ,s32 nLen)
{	
	int nRet = 0;
	CHECK_NULL_RET_INVALID(pBuf)
	if (INVALID_SOCKET == hSocket)
	{
		return ACL_ERROR_INVALID;
	}
	//ע�⣬��ǰloop����Ϊ�����������ݡ����������ݹ��󣬿��ܳ���һ����Ҫ�����ζ������
	nRet = recv(hSocket,pBuf,nLen,0);
	if (nRet < 0)
	{
        ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTcpRecv] acl tcp recv is failed RET %d\n",nRet);
	}
	return nRet;
}


ACL_API HSockManage aclSocketManageInit(ESOCKET_MANAGE eManageType)
{
	TSockManage * ptSockManage = NULL;
	int i = 0;
	PF_THREAD_ENTRY pfCb = NULL;
	ptSockManage = (TSockManage *)aclMallocClr(sizeof(TSockManage));
	CHECK_NULL_RET_NULL(ptSockManage)
	if (ACL_ERROR_NOERROR != aclCreateLock(&ptSockManage->m_hLock,NULL))
	{
		aclFree(ptSockManage);
		return NULL;
	}

	ptSockManage->m_nTotalNode = MAX_NODE_SUPPORT;
	ptSockManage->m_ptSockNodeArr = (TSockNode *)aclMallocClr(sizeof(TSockNode) * ptSockManage->m_nTotalNode);
	CHECK_NULL_RET_NULL(ptSockManage->m_ptSockNodeArr)

	ptSockManage->pActSockPos = (int *)aclMallocClr(sizeof(int) * ptSockManage->m_nTotalNode);
	CHECK_NULL_RET_NULL(ptSockManage->pActSockPos)

	for (i = 0; i < ptSockManage->m_nTotalNode; i++)
	{
		ptSockManage->m_ptSockNodeArr[i].m_hSock = INVALID_SOCKET;
	}
	switch(eManageType)
	{
	case E_DATA_PROC:
		{
            ptSockManage->m_eMainTaskStatus = E_TASK_RUNNING;
			pfCb = (PF_THREAD_ENTRY)aclDataProcThread;
			//also need a heart beat thread for data porc

            ptSockManage->m_eHBTaskStatus = E_TASK_RUNNING;
			aclCreateThread(&ptSockManage->m_tHBThread, aclHBDetectThread, ptSockManage);
		}
		break;
	case E_3A_CONNECT:
		{
            //3A�����߳��ò���������⣬��˲�������������߳�
            ptSockManage->m_eMainTaskStatus = E_TASK_RUNNING;
			pfCb = (PF_THREAD_ENTRY)aclConnect3AThread;
		}
		break;
	default:
		ACL_DEBUG(E_MOD_MANAGE, E_TYPE_ERROR,"[aclSocketManageInit] unknown sockMngtype\n",eManageType);
		break;

	}
	if (!pfCb)
	{
		return NULL;
	}
	aclCreateThread(&ptSockManage->m_tSelectThread, pfCb, ptSockManage);

	return (HSockManage)ptSockManage;
}

//un init
ACL_API void aclSocketManageUninit(HSockManage hSockManage, ESOCKET_MANAGE eManageType)
{
	TSockManage * ptSockManage = (TSockManage *)hSockManage;
    CHECK_NULL_RET(ptSockManage);

    switch(eManageType)
    {
    case E_DATA_PROC:
        {
            WAIT_FOR_EXIT(ptSockManage->m_eMainTaskStatus);
            aclDestoryThread(ptSockManage->m_tSelectThread.hThread);

            WAIT_FOR_EXIT(ptSockManage->m_eHBTaskStatus);
            aclDestoryThread(ptSockManage->m_tHBThread.hThread);
        }
        break;
    case E_3A_CONNECT:
        {
            WAIT_FOR_EXIT(ptSockManage->m_eMainTaskStatus);
            aclDestoryThread(ptSockManage->m_tSelectThread.hThread);
        }
        break;
    default:
        break;
    }

    if (ptSockManage->pActSockPos)
    {
        aclFree(ptSockManage->pActSockPos);
    }
    if (ptSockManage->m_ptSockNodeArr)
    {
        int i = 0;
        for (i = 0; i < ptSockManage->m_nTotalNode; i++)
        {
            if (INVALID_SOCKET != ptSockManage->m_ptSockNodeArr[i].m_hSock)
            {
                aclCloseSocket(ptSockManage->m_ptSockNodeArr[i].m_hSock);
            }
        }
        aclFree(ptSockManage->m_ptSockNodeArr);
    }

    aclDestoryLock(ptSockManage->m_hLock);
    aclFree(ptSockManage);
	return;
}


ACL_API int aclCombPack(s8 * pCombBuf,u32 dwBufSize, TAclMessage * ptHead, char * pExtData, int nExtDataLen)
{
	CHECK_NULL_RET_INVALID(pCombBuf)
	CHECK_NULL_RET_INVALID(ptHead)
	ptHead->m_dwPackLen = sizeof(TAclMessage) + nExtDataLen;
	ptHead->m_dwContentLen = nExtDataLen;

	if (ptHead->m_dwPackLen > dwBufSize)
	{
        ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[aclCombPack]combine packet failed,%dKB > MAX:%dKB\n",ptHead->m_dwPackLen/1024,MAX_SEND_PACKET_SIZE/1024);
		return ACL_ERROR_OVERFLOW;
	}
	memcpy(pCombBuf, ptHead, sizeof(TAclMessage));
	memcpy(pCombBuf + sizeof(TAclMessage), pExtData, nExtDataLen);
	return ptHead->m_dwPackLen;
}


ACL_API int aclResetSockNode(HSockManage hSockManage, int nPos)
{
	TSockManage * ptSockMng = NULL;
	TSockNode * ptSockNode = NULL;
	ptSockMng = (TSockManage *)hSockManage;
	if (NULL == ptSockMng)
	{
		return ACL_ERROR_PARAM;
	}

	if (nPos < 0 || nPos >= ptSockMng->m_nTotalNode)
	{
		return ACL_ERROR_PARAM;
	}
	ptSockNode = &ptSockMng->m_ptSockNodeArr[nPos];

	if (INVALID_SOCKET != ptSockNode->m_hSock)
	{
#ifdef WIN32
        closesocket(ptSockNode->m_hSock);
#elif defined (_LINUX_)
        close(ptSockNode->m_hSock);

#endif
		
	}
	//����(Telnet����)�ڵ�Context�洢������Ϊ��malloc���͵�����
    //���������ڵ���aclInsertSelectLoopʱ��content��ֵΪ����ʵ����ָ�룬��������ͷŻ������������
    //��ǰֻ��3A��֤��ʱ�������malloc��ֵ���ڴ洢��֤��Ϣ����ʱ��Context����Ҫ�ͷŵ�
    //3A��֤����aclInsertSelectLoopʹ�õ�WaitEvent��ESELECT_CONN����ʾ�ȴ���֤������ͨ������
    //������ǰ��ʹ�õ�Read�����������ڵ㣬ֻ�ǽ������������Socket���ӣ����ջ�Ҫ��3A��֤
	if (NULL != ptSockNode->m_pContext && ESELECT_CONN == ptSockNode->m_eWaitEvent)
	{
		aclFree(ptSockNode->m_pContext);
	}

	ptSockMng->m_dwNodeMap[nPos] = 0;
	CHECK_NULL_RET_INVALID(ptSockNode);
	memset(ptSockNode, 0, sizeof(TSockNode));
	ptSockNode->m_hSock = INVALID_SOCKET;
	return ACL_ERROR_NOERROR;
}


int aclSelectLoop(TSockManage * ptSockManage,u32 dwWaitMilliSec)
{
	int i = 0;
	fd_set * pRead = NULL;
	fd_set * pWrite = NULL;
	fd_set * pError = NULL;

	int nReadyNum = 0;
	H_ACL_SOCKET sMaxSock = 0;
	int nWaitSockCount = 0;
	int * pActiSockPos = NULL;
	s32 nRet = 0;

	struct timeval tv, *ptv;

	TSockNode * ptSockNode = NULL;
	CHECK_NULL_RET_INVALID(ptSockManage)

	pActiSockPos = ptSockManage->pActSockPos;


	pRead  = &ptSockManage->m_fdRead;
	pWrite = &ptSockManage->m_fdWrite;
	pError = &ptSockManage->m_fdError;
	ptSockNode = ptSockManage->m_ptSockNodeArr;

	FD_ZERO(pRead);
	FD_ZERO(pWrite);
	FD_ZERO(pError);
	lockLock(ptSockManage->m_hLock);
	for (i = 0; i < ptSockManage->m_nTotalNode; i++)
	{
		if (INVALID_SOCKET != ptSockNode[i].m_hSock)
		{
			//current socket is not wait anymore
			if (E_LET_IT_GO == ptSockNode[i].m_eWaitEvent)
			{
				aclCloseSocket(ptSockNode[i].m_hSock);
			    ptSockNode[i].m_hSock = INVALID_SOCKET;
				ptSockNode[i].m_pContext = NULL;
				ptSockNode[i].m_pfCB = NULL;
				ptSockNode[i].m_dwNodeNum = -1;
				continue;
			}
			if (ptSockNode[i].m_eWaitEvent & ESELECT_READ)
			{
				FD_SET(ptSockNode[i].m_hSock, pRead);
			}
			if (ptSockNode[i].m_eWaitEvent & ESELECT_WRITE)
			{
				FD_SET(ptSockNode[i].m_hSock, pWrite);
			}
			if (ptSockNode[i].m_eWaitEvent & ESELECT_ERROR)
			{
				FD_SET(ptSockNode[i].m_hSock, pError);
			}
			if (sMaxSock < ptSockNode[i].m_hSock)
			{
				sMaxSock = ptSockNode[i].m_hSock;
			}

			pActiSockPos[nWaitSockCount] = i;

			nWaitSockCount++;
			ptSockNode->m_nSelectCount++;
		}
	}

	if (dwWaitMilliSec != (u32)-1)
	{
		tv.tv_sec = dwWaitMilliSec/1000;
		tv.tv_usec = (dwWaitMilliSec%1000)*1000;
		ptv = &tv;
	}
	else
	{
		ptv = NULL;
	}
	unlockLock(ptSockManage->m_hLock);
	nReadyNum = select(sMaxSock + 1, pRead, pWrite, pError, ptv);
	if (nReadyNum >0)
	{
        //lockLock(ptSockManage->m_hLock);
		for (i = 0; i < nWaitSockCount; i++)
		{
			ptSockNode = &ptSockManage->m_ptSockNodeArr[pActiSockPos[i]];
            if (INVALID_SOCKET == ptSockNode->m_hSock)
            {
                //unlockLock(ptSockManage->m_hLock);
                continue;
            }
			if((ESELECT_READ & ptSockNode->m_eWaitEvent) && 
				FD_ISSET(ptSockNode->m_hSock, pRead))
			{
				if (ptSockNode->m_pfCB)
				{
					nRet = (*ptSockNode->m_pfCB)(ptSockNode->m_hSock, ptSockNode->m_eWaitEvent, ptSockNode->m_pContext);
					if (ESELECT_CONN & ptSockNode->m_eWaitEvent)//this is a 3A process
					{
						if (ACL_ERROR_NOERROR == nRet)//3A is Pass, delete it
						{
							//when time is up 3A thread will delete it automatically
							//set INVALID_SOCKET can prevent thread from closing socket
							ptSockNode->m_hSock = INVALID_SOCKET;
						}
					}
				}
			}
			if((ESELECT_WRITE & ptSockNode->m_eWaitEvent) && 
				FD_ISSET(ptSockNode->m_hSock, pWrite))
			{
				if (ptSockNode->m_pfCB)
				{
					(*ptSockNode->m_pfCB)(ptSockNode->m_hSock, ESELECT_WRITE, ptSockNode->m_pContext);
				}
			}
			if((ESELECT_ERROR & ptSockNode->m_eWaitEvent) && 
				FD_ISSET(ptSockNode->m_hSock, pError))
			{
				if (ptSockNode->m_pfCB)
				{
					(*ptSockNode->m_pfCB)(ptSockNode->m_hSock, ESELECT_ERROR, ptSockNode->m_pContext);
				}
			}
			ptSockNode->m_nSelectCount = 0;
		}
        //unlockLock(ptSockManage->m_hLock);
	}
	else
	{
		aclDelay(dwWaitMilliSec);
	}
	return ACL_ERROR_NOERROR;
}




//��������߳�
//ѭ�������socket�ĻỰ������������ͬʱ��ⷴ��״̬
//���Ự�������õĳ�ʱʱ�䣬���϶���ʱ��ֱ�ӶϿ�
int aclSockHBCheckLoop(TSockManage * ptSockManage,u32 dwWaitMilliSec)
{
	int i = 0;
	TSockNode * ptSockNode = NULL;
	CHECK_NULL_RET_INVALID(ptSockManage);
	ptSockNode = ptSockManage->m_ptSockNodeArr;

	lockLock(ptSockManage->m_hLock);
	for (i = 0; i < ptSockManage->m_nTotalNode; i++)
	{
		//check if HB is time out
        //�����ڵ㲻�ᱻ������⣬��˲��õ��ļ����ڵ㱻����
		if( ptSockNode[i].m_nHBCount > HB_TIMEOUT/(int)dwWaitMilliSec)
		{
			//time is up, remove current 3A check
			aclResetSockNode((HSockManage)ptSockManage, i);
		}

		//connected node, and must be a client Node
		if( 0 != ptSockManage->m_dwNodeMap[i])
		{
			++ptSockNode[i].m_nHBCount;
			ACL_DEBUG(E_MOD_HB, E_TYPE_DEBUG, "[aclSockHBCheckLoop] POS:%d CT:%d\n",i,ptSockNode[i].m_nHBCount);

			//only client socket send heartbeat message
			if ( E_NT_SERVER == ptSockNode[i].m_eNodeType)
			{
				aclMsgPush(0, MAKEID(HBDET_APP_NUM, INST_SEEK_IDLE), ptSockManage->m_dwNodeMap[i], MSG_HBMSG_REQ,NULL,0,E_PMT_L_L);
			}
		}
	}
	unlockLock(ptSockManage->m_hLock);

	aclDelay(dwWaitMilliSec);
	return ACL_ERROR_NOERROR;
}

//=============================================================================
//�� �� ����aclCreateNode
//��	    �ܣ�����socket�ڵ㣬��ʼ���ڵ�������ӵ�Ԫ,ָ���ü����˿ڣ�������������
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ���� hSockMng �ڵ������
//          u16 wPort ���ؼ����˿�
//     FEvSelect pfCB ���������ʱ�Ļص�����
//ע    ��: 
//�� �� ֵ�����ص�ǰ�ڲ�Node�ţ�ֻ�ܵ���ʱ�����
//=============================================================================
ACL_API int aclCreateNode(HSockManage hSockMng, const char * pIPAddr, u16 wPort,FEvSelect pfCB, void * pContext)
{
	TSockManage * ptSockManage = NULL;
	H_ACL_SOCKET hSockNode = INVALID_SOCKET;
	u32 nListNodeID = 0;
	ptSockManage = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockManage)

	hSockNode = aclCreateSockNode(pIPAddr,wPort);
	if (INVALID_SOCKET == hSockNode)
	{
        ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF, "[aclCreateNode] create socket node at PORT:%d\n", wPort);
		return ACL_ERROR_FAILED;
	}
	nListNodeID = aclInsertSelectLoop(hSockMng, hSockNode, pfCB, ESELECT_READ, -1, pContext);
	return ACL_ERROR_NOERROR;

}

//=============================================================================
//�� �� ����aclInsertSelectLoop
//��	    �ܣ���socket����ָ����selectѭ��
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����hSockMng: Ҫ�����socket�������ָ��
//             hSock: Ҫ�����Socket
//              pfCB: ��Ϣ��Ӧ����
//          eSelType: socket��Ϣ�������
//      nInnerNodeID: �ڲ��ڵ��
//          pContext: �ڵ㸽������
//ע    ��: �˺������ڲ���һ���µĽڵ㣬�õ�������ESELECT_READ �������ڽ�����Ϣ
//          ��֪ͨע��ĺ������˺�������������ݴ���3A��֤����������߳�ʹ��
//�ڲ��ڵ�� nInnerNodeID 
//-1 ��ʾ�Ǽ���ID��ֵ����ν
//=0  ��ʾ�Ƿ����������µ�socket��IDҪ�뱾��ȫ��IDһ��
//>0 ��ʾ�ǿͻ��˲����µ�socket��ID��Ҫ���뵽�ڵ㣬������ȫ��ID��֮��Ӧ
//=============================================================================
ACL_API int aclInsertSelectLoop(HSockManage hSockMng, H_ACL_SOCKET hSock, FEvSelect pfCB, ESELECT eSelType, int nInnerNodeID, void * pContext)
{
	int i;
	TSockManage * ptSockManage = NULL;
	TSockNode * ptNewNode = NULL;
	int nFindNode = -1;
	u32 dwGlbID = 0;
	ptSockManage = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockManage)
	lockLock(ptSockManage->m_hLock);
	
	ptNewNode = ptSockManage->m_ptSockNodeArr;
	for (i = 0; i < ptSockManage->m_nTotalNode; i++)
	{
		//����ÿ���Լ�ò��û����ʵ����ۼ�
//		ptNewNode = ptNewNode + i;//NB BUG 
		ptNewNode = &ptSockManage->m_ptSockNodeArr[i];
		if (!ptNewNode->m_bIsUsed)//find a node whitch have not used yet
		{
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_DEBUG, "[aclInsertSelectLoop] [%d] has not used SEL TYPE:%d\n",i,eSelType);
			ptNewNode->m_eWaitEvent = eSelType;
			if (ESELECT_READ == eSelType)
			{
				//ֻ�д�recv���͵�socekt����Ҫ����������������ڹ����������
				if (ptNewNode->tPktBufMng.pPktBufMng)
				{
					aclFree(ptNewNode->tPktBufMng.pPktBufMng);
				}
				ptNewNode->tPktBufMng.pPktBufMng = aclMallocClr(INIT_PACKET_BUFFER_MANAGER);
			}

			if (ESELECT_CONN & eSelType)
			{
				ptNewNode->m_eWaitEvent = (ESELECT)(ESELECT_READ | ESELECT_CONN);
			}
			ptNewNode->m_dwNodeNum = nInnerNodeID;// Node start at 1
			ptNewNode->m_pfCB = pfCB;//cb to handle new connect
			ptNewNode->m_hSock = hSock;
			ptNewNode->m_pContext = pContext;
			ptNewNode->m_bIsUsed = TRUE;
			nFindNode = i;
			break;
		}
	}
	if (-1 == nFindNode)// have not find new node
	{
		unlockLock(ptSockManage->m_hLock);
        ACL_DEBUG(E_MOD_NETWORK, E_TYPE_WARNNING, "[aclInsertSelectLoop] have not find new node to insert\n");
		return ACL_ERROR_OVERFLOW;
	}

	if (0 == nInnerNodeID)//means it is a server insert
	{
		dwGlbID = aclSessionIDGenerate();
		ptNewNode->m_dwNodeNum = dwGlbID;
		ptNewNode->m_eNodeType = E_NT_SERVER;
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_DEBUG, "[aclInsertSelectLoop] aclInsertSelectLoop insert server\n");
	}
	else if (-1 == nInnerNodeID)//means it is a listen Node
	{
		//local listen ID, not important no glbID alloc
		ptNewNode->m_dwNodeNum = dwGlbID;
		ptNewNode->m_eNodeType = E_NT_LISTEN;
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_DEBUG, "[aclInsertSelectLoop] aclInsertSelectLoop local listen\n");
	}
	else
	{
		dwGlbID = aclSessionIDGenerate();
		ptNewNode->m_eNodeType = E_NT_CLIENT;
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_DEBUG, "[aclInsertSelectLoop] aclInsertSelectLoop insert client\n");
	}
	ptSockManage->m_dwNodeMap[nFindNode] = dwGlbID;
    
    ACL_DEBUG(E_MOD_NETWORK, E_TYPE_DEBUG, "[aclInsertSelectLoop] insert a NETWORK Node ID %d and GlbID %d\n",nInnerNodeID,dwGlbID);

	unlockLock(ptSockManage->m_hLock);

	return dwGlbID;// return GLBID
}

//ɾ��ָ����socket
//HSockManage hSockMng     �ڵ������
//H_ACL_SOCKET hSock       ��Ҫɾ����Socket
ACL_API int aclRemoveSelectLoop(HSockManage hSockMng, H_ACL_SOCKET hSock)
{
	int i;
	TSockManage * ptSockManage = NULL;
	TSockNode * ptNewNode = NULL;
	int nFindNode = -1;
	ptSockManage = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockManage);
	if (INVALID_SOCKET == hSock)
	{
		return ACL_ERROR_INVALID;
	}
	lockLock(ptSockManage->m_hLock);
	ptNewNode = ptSockManage->m_ptSockNodeArr;
	for (i = 0; i < ptSockManage->m_nTotalNode; i++)
	{
		ptNewNode = &ptSockManage->m_ptSockNodeArr[i];
		if (hSock == ptNewNode->m_hSock)//find out this socket
		{
			if (ptNewNode->tPktBufMng.pPktBufMng)
			{
				aclFree(ptNewNode->tPktBufMng.pPktBufMng);
				ptNewNode->tPktBufMng.pPktBufMng = NULL;
			}
			aclResetSockNode(hSockMng, i);
			nFindNode = i;
            ptNewNode->m_bIsUsed = FALSE;
			break;
		}
	}
	unlockLock(ptSockManage->m_hLock);
	return nFindNode;
}

ACL_API int aclRemoveSelectLoopUnsafe(HSockManage hSockMng, H_ACL_SOCKET hSock)
{
    int i;
    TSockManage * ptSockManage = NULL;
    TSockNode * ptNewNode = NULL;
    int nFindNode = -1;
    ptSockManage = (TSockManage *)hSockMng;
    CHECK_NULL_RET_INVALID(ptSockManage);
    if (INVALID_SOCKET == hSock)
    {
        return ACL_ERROR_INVALID;
    }
    ptNewNode = ptSockManage->m_ptSockNodeArr;
    for (i = 0; i < ptSockManage->m_nTotalNode; i++)
    {
        ptNewNode = &ptSockManage->m_ptSockNodeArr[i];
        if (hSock == ptNewNode->m_hSock)//find out this socket
        {
            if (ptNewNode->tPktBufMng.pPktBufMng)
            {
                aclFree(ptNewNode->tPktBufMng.pPktBufMng);
                ptNewNode->tPktBufMng.pPktBufMng = NULL;
            }
            aclResetSockNode(hSockMng, i);
            nFindNode = i;
            ptNewNode->m_bIsUsed = FALSE;
            break;
        }
    }
    return nFindNode;
}

//heart beat confirmed
//=============================================================================
//�� �� ����aclHBConfirm
//��	    �ܣ�������Ӧ
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����hSockMng: ����Socket���������
//             nNode: ȷ�����ӵĽڵ��
//ע    ��: 
//=============================================================================
ACL_API int aclHBConfirm(HSockManage hSockMng, int nNode)
{
	TSockManage * tSockMng = NULL;
	int i = 0;
	int nPos = -1;
	tSockMng = (TSockManage *)hSockMng;

	if (0 == nNode || NULL == tSockMng)
	{
		return ACL_ERROR_PARAM;
	}

	lockLock(tSockMng->m_hLock);
	for(i = 0; i < tSockMng->m_nTotalNode; i++)
	{
		if (nNode == tSockMng->m_dwNodeMap[i])
		{
			tSockMng->m_ptSockNodeArr[i].m_nHBCount = 0;//reset when recv one heart beat ack packet
			ACL_DEBUG(E_MOD_HB, E_TYPE_DEBUG,"[aclHBConfirm] POS:%d CT:%d\n",i, tSockMng->m_ptSockNodeArr[i].m_nHBCount);
			nPos = i;
			break;
		}
	}
	unlockLock(tSockMng->m_hLock);
	return nPos;
}


//�ڵ�ӳ�����ú��������ڲ��ڵ���ȫ�ֽڵ�ӳ�䣬����ȫ�ֽڵ�
ACL_API u32 aclSetNodeMap(HSockManage hSockMng, u32 dwNewNode)
{
	TSockManage * ptSockMng = NULL;
	u32 dwGlbID = 0;
	ptSockMng = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockMng);

	lockLock(ptSockMng->m_hLock);
	dwGlbID = aclSessionIDGenerate();
	ptSockMng->m_dwNodeMap[dwNewNode] = dwGlbID;
	unlockLock(ptSockMng->m_hLock);

	return dwGlbID;
}

ACL_API u32 aclGlbNode2Net(HSockManage hSockMng, u32 dwCheckNode)
{
	TSockManage * ptSockMng = NULL;
	int i = 0;
	ptSockMng = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockMng);

	for (i = 0; i < ptSockMng->m_nTotalNode; i++)
	{
		if (dwCheckNode == ptSockMng->m_dwNodeMap[i])
		{
			return ptSockMng->m_ptSockNodeArr[i].m_dwNodeNum;
		}	
	}
	return 0;
}


ACL_API int aclGetCliInfoBySpecSessionId(HSockManage hSockMng, u32 dwCheckNode, TPeerClientInfo &tPeerCliInfo)
{
    TSockManage * ptSockMng = (TSockManage *)hSockMng;
    CHECK_NULL_RET_INVALID(ptSockMng);

    for (int i = 0; i < ptSockMng->m_nTotalNode; i++)
    {
        if (dwCheckNode == ptSockMng->m_dwNodeMap[i])
        {
            tPeerCliInfo = *(TPeerClientInfo*)ptSockMng->m_ptSockNodeArr[i].m_pContext;
            return 1;
        }
    }
    return 0;
}

ACL_API int aclNetNode2Glb(HSockManage hSockMng, u32 dwNetNode)
{
	TSockManage * ptSockMng = NULL;
	int i = 0;
	ptSockMng = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockMng);

	for (i = 0; i < ptSockMng->m_nTotalNode; i++)
	{
		if (dwNetNode == ptSockMng->m_ptSockNodeArr[i].m_dwNodeNum)
		{
			return ptSockMng->m_dwNodeMap[i];
		}	
	}
	return 0;
}

ACL_API int aclNetNode2Pos(HSockManage hSockMng, u32 dwNetNode)
{
	TSockManage * ptSockMng = NULL;
	int i = 0;
	ptSockMng = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockMng);

	for (i = 0; i < ptSockMng->m_nTotalNode; i++)
	{
		if (dwNetNode == ptSockMng->m_ptSockNodeArr[i].m_dwNodeNum)
		{
			return i;
		}	
	}
	return -1;
}

//=============================================================================
//�� �� ����aclConnect3AThread
//��	    �ܣ�3A�������߳�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����pParam�� socket�������ָ��
//ע    ��: ���߳�����3A��֤�����еĿͻ�����������3A�̴߳���ͨ����֤��Ͷ�͵�
//          ���ݴ����̣߳��˺����ж�ʱ�������ܣ�һ��ʱ�������ͨ����3A����ֱ�Ӷ���
//=============================================================================
#ifdef WIN32
int __stdcall aclConnect3AThread(void * pParam)
#elif defined _LINUX_
int aclConnect3AThread(void * pParam)
#endif
{
	TSockManage * ptSockManage = (TSockManage *)pParam;
	TSockNode * ptSockNode = NULL;
	int i = 0;
	ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF,"[aclConnect3AThread] aclConnect3AThread started...");
	if (NULL == ptSockManage)
	{
		ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF,"failed\n");
		return ACL_ERROR_INVALID;
	}
	ptSockNode = ptSockManage->m_ptSockNodeArr;

	ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF,"ok\n");
	while(E_TASK_RUNNING == ptSockManage->m_eMainTaskStatus)
	{
//		lockLock(ptSockManage->m_hLock);
		for (i = 0; i < ptSockManage->m_nTotalNode; i++)
		{
			if (ptSockNode[i].m_eWaitEvent & ESELECT_READ)
			{
				if (ptSockNode[i].m_nSelectCount * SELECT_3A_INTERVAL > CHECK_3A_TIMEOUT)
				{
					//time is up, remove current 3A check
					aclPrintf(TRUE,FALSE, "----------3A check node: %d count %d\n", ptSockNode[i].m_hSock, ptSockNode[i].m_nSelectCount);
					aclResetSockNode((HSockManage)ptSockManage, i);
				}
			}

			//socket has already finish 3A, reset it now
			if (ptSockNode[i].m_eWaitEvent & ESELECT_CONN && INVALID_SOCKET == ptSockNode[i].m_hSock)
			{
				aclResetSockNode((HSockManage)ptSockManage, i);
			}
		}
		aclSelectLoop(ptSockManage, SELECT_3A_INTERVAL);
//		unlockLock(ptSockManage->m_hLock);
	}

    //3A Thread is exit , reset all SockNode
    for (i = 0; i < ptSockManage->m_nTotalNode; i++)
    {
        if (ptSockNode[i].m_eWaitEvent & ESELECT_READ)
        {
                //reset
                aclResetSockNode((HSockManage)ptSockManage, i);
        }
        //socket has already finish 3A, reset it now
        if (ptSockNode[i].m_eWaitEvent & ESELECT_CONN && INVALID_SOCKET == ptSockNode[i].m_hSock)
        {
            aclResetSockNode((HSockManage)ptSockManage, i);
        }
    }
    ptSockManage->m_eMainTaskStatus = E_TASK_ALREADY_EXIT;
	ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF,"[aclConnect3AThread] aclConnect3AThread terminated\n");
	return ACL_ERROR_NOERROR;
}

//=============================================================================
//�� �� ����aclDataProcThread
//��	    �ܣ������շ��߳�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����pParam�� socket�������ָ��
//ע    ��: ���߳����ڽ�������������յ���Ϣ�������䷢��ָ����APP Instance��
//          Telnet���Եļ���Ҳ�ɴ��߳�ά�����յ�telnet������Ҳ�ᷢ����Ӧ�ĵ���APP��
//=============================================================================
#ifdef WIN32
int  __stdcall aclDataProcThread(void * pParam)
#elif defined _LINUX_
int  aclDataProcThread(void * pParam)
#endif
{
	TSockManage * ptSockManage = (TSockManage *)pParam;
	if (NULL == ptSockManage)
	{
		ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF, "[aclDataProcThread] aclDataProcThread started...failed\n");
		return ACL_ERROR_INVALID;
	}
    ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF, "[aclDataProcThread] aclDataProcThread started...ok\n");
	while(E_TASK_RUNNING == ptSockManage->m_eMainTaskStatus)
	{
//		lockLock(ptSockManage->m_hLock);
		aclSelectLoop(ptSockManage, SELECT_DP_INTERVAL);
//		unlockLock(ptSockManage->m_hLock);
	}

    ptSockManage->m_eMainTaskStatus = E_TASK_ALREADY_EXIT;
	ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF,"[aclDataProcThread] aclDataProcThread terminated\n");
	return ACL_ERROR_NOERROR;
}

//=============================================================================
//�� �� ����aclHBDetectThread
//��	    �ܣ���������߳�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����pParam�� socket�������ָ��
//ע    ��: ���̹߳�������ͨ��3A��֤�Ự��������������̼߳���߳�ͬʱ����
//          �ͻ���(��Ϊ����˸�����)һ������������⣬CS���Լ�ⳬʱ���Զ�����
//
//=============================================================================
#ifdef WIN32
unsigned int  __stdcall aclHBDetectThread(void * pParam)
#elif defined _LINUX_
void * aclHBDetectThread(void * pParam)
#endif
{
	TSockManage * ptSockManage = (TSockManage *)pParam;
	if (NULL == ptSockManage)
	{
        ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF, "[aclHBDetectThread] aclErrorDetectThread started...failed\n");
#ifdef WIN32
        return ACL_ERROR_INVALID;
#elif defined (_LINUX_)
        return (void *)ACL_ERROR_INVALID;
#endif
		
	}
	ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF, "[aclHBDetectThread] aclErrorDetectThread started...ok\n");
	while(E_TASK_RUNNING == ptSockManage->m_eHBTaskStatus)
	{
		aclSockHBCheckLoop(ptSockManage, HB_CHECK_ITRVL);
	}

    ptSockManage->m_eHBTaskStatus = E_TASK_ALREADY_EXIT;
	ACL_DEBUG(E_MOD_MANAGE, E_TYPE_NOTIF,"[aclHBDetectThread] aclErrorDetectThread terminated\n");
#ifdef WIN32
    return ACL_ERROR_NOERROR;
#elif defined (_LINUX_)
    return (void *)ACL_ERROR_NOERROR;
#endif
	
}


/*====================================================================
  ��������IsBigEndian
  ���ܣ��жϵ�ǰϵͳ��˻���С��
  �㷨ʵ�֣����ô�С�˵�ַ����ʱ�ߵ��ֽ�λ�õ�ԭ���ж�
  �������˵����
			u8 * pBuffer ��Ҫ�������������
			u8 byByteOffset ָ���ֽ���ʼ��pBuffer���ƫ��
			u8 dwBitsLen ��Ҫ��ȡ�ֽڵĳ���
  ����ֵ˵��: TRUE ��� FALSE С��
  ====================================================================*/
ACL_API BOOL IsBigEndian()
{
	u16 dwTeEndian = 0x0102;
	u8 * pTeEndian = (u8 *)&dwTeEndian;
	if (pTeEndian[0] == 0x01)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

ACL_API void aclShowNode()
{
    TSockManage *  ptSockManage = (TSockManage *)getSockDataManger();
    int i = 0;
    CHECK_NULL_RET(ptSockManage);
    
    aclPrintf(TRUE, FALSE, "*********************Node List start*********************\n");
    for (i = 0; i < ptSockManage->m_nTotalNode; i++)
    {
        if (E_NT_INITED != ptSockManage->m_ptSockNodeArr[i].m_eNodeType)
        {
            aclPrintf(TRUE, FALSE, "Node: %d\tSocket: %X\t NodeType:%s\n", 
               ptSockManage->m_ptSockNodeArr[i].m_dwNodeNum,
               ptSockManage->m_ptSockNodeArr[i].m_hSock,
               (E_NT_LISTEN == ptSockManage->m_ptSockNodeArr[i].m_eNodeType)? "NODE_LISTEN":\
               (E_NT_CLIENT == ptSockManage->m_ptSockNodeArr[i].m_eNodeType)? "NODE_CLIENT":"NODE_SERVER");
        }
    }
    aclPrintf(TRUE, FALSE, "*********************Node List end  *********************\n");

}

/*====================================================================
  ��������aclFindSocketNode
  ���ܣ����Socket�ڵ��ַ
  �������˵����
       hSockMng Socket������
	      hSock Ŀ��Socket
    pptSockNode <���>��ýڵ��ַ���ձ�ʾû�ҵ�
  ����ֵ˵��: �����ACL����˵��
             
  ��ע���˺��������ڲ�ʹ�ã����̲߳���ȫ�ģ���Ҫ�ھ��峡����������ʹ��
  ====================================================================*/
ACL_API int aclFindSocketNode(HSockManage hSockMng, H_ACL_SOCKET hSock, TSockNode ** pptSockNode)
{
	int i;
	TSockManage * ptSockManage = NULL;
	TSockNode * ptNewNode = NULL;
	int nFindNode = -1;
	ptSockManage = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockManage);
	if (INVALID_SOCKET == hSock)
	{
		return ACL_ERROR_PARAM;
	}
	
	ptNewNode = ptSockManage->m_ptSockNodeArr;
	for (i = 0; i < ptSockManage->m_nTotalNode; i++)
	{
		ptNewNode = &ptSockManage->m_ptSockNodeArr[i];
		if (hSock == ptNewNode->m_hSock)//find out this socket
		{
			* pptSockNode = ptNewNode;
			return ACL_ERROR_NOERROR;
			break;
		}
	}
	return ACL_ERROR_EMPTY;
}


/*====================================================================
  ��������aclCheckPktBufMngContent
  ���ܣ�����Ƿ��п��õİ����ϳ�
  �������˵����
      ptNewNode Socket�ڵ�ָ��
	     pnSize ���ֵ�����ڻ�ÿ��óߴ磬���û���򷵻�0
       
  ����ֵ˵��: �����ACL����˵��
             
  ��ע���ڲ�ʹ�ã����̲߳���ȫ��
  ====================================================================*/
ACL_API int aclCheckPktBufMngContent(TPktBufMng * ptPktMng, int * pnSize)
{
	TAclMessage * ptAclMsg = NULL;

	CHECK_NULL_RET_ERR_PARAM(ptPktMng);
	CHECK_NULL_RET_ERR_PARAM(pnSize);
	
	if (ptPktMng->dwCurePBMSize < sizeof(TAclMessage))
	{
		//���ݰ�ͷ�Ĵ�С��û�ﵽ���޷��жϰ�״̬
		return ACL_ERROR_FAILED;
	}
	ptAclMsg = (TAclMessage *)ptPktMng->pPktBufMng;

	if (ptAclMsg->m_dwPackLen <= ptPktMng->dwCurePBMSize)
	{
		//����������,�������
		ACL_DEBUG(E_MOD_MSG, E_TYPE_NOTIF,"[aclCheckPktBufMngContent] find a new packet and ready to send BufData<%d> PktLen<%d>,\n", ptPktMng->dwCurePBMSize, ptAclMsg->m_dwPackLen);
		* pnSize = ptAclMsg->m_dwPackLen;
		return ACL_ERROR_NOERROR;
	}
	ACL_DEBUG(E_MOD_MSG, E_TYPE_NOTIF,"[aclCheckPktBufMngContent] no new packet found yet firstPKT Len %d, PMB Size %d\n", ptAclMsg->m_dwPackLen, ptPktMng->dwCurePBMSize);

	//Ŀǰ��û�����һ��
	return ACL_ERROR_FAILED;
}


/*====================================================================
  ��������aclInsertPBMAndSend
  ���ܣ������յ���ACL��Ϣ���ݲ��뻺���,���������
  �������˵����
           data ���������
	   nDataLen �������ݵĳ���

  ����ֵ˵��: 
             >0 �����������İ���
			 =0 �������һ�����ݣ��Һ���û�п��ð��ˡ�
  ��ע��������������ACL��Ϣ��ʱ�����ִ��������������ʹ�ô˺���ʵ�ְ����ݵĹ���
        ��:�����������˳���������ֻ�е����յ������ݲ�������ʱ���ʹ�ð�������
		  :Ŀǰֻ���ð�����λ��������֮����Ҫ����ʹ��checksum������ȫ
  ====================================================================*/
ACL_API int aclInsertPBMAndSend(HSockManage hSockMng, H_ACL_SOCKET hSock,void * data, int nDataLen)
{
	int nRet = ACL_ERROR_NOERROR;
	int nCheckSize = 0;
	TSockManage * ptSockManage = NULL;
	TSockNode * ptNewNode = NULL;
	ptSockManage = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockManage);
	if (INVALID_SOCKET == hSock)
	{
		return ACL_ERROR_INVALID;
	}
	lockLock(ptSockManage->m_hLock);
	nRet = aclFindSocketNode(hSockMng, hSock, &ptNewNode);
	if(ACL_ERROR_NOERROR != nRet || NULL == ptNewNode)
	{
		//Ѱ��SocketNode���ִ���
		ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[aclInsertPBMAndSend] cannot find target socket node SOCK[%d]\n",hSock);
		unlockLock(ptSockManage->m_hLock);
		return ACL_ERROR_FAILED;
	}

	//���µ����ݲ��������

	if ((nDataLen + ptNewNode->tPktBufMng.dwCurePBMSize) > INIT_PACKET_BUFFER_MANAGER)
	{
		//��������PBM���ޣ���ô����?���ݵ�ǰʵ����Ҫ�Ĵ�С��������ߵĿռ�
		void * pTmp = aclMallocClr(nDataLen + ptNewNode->tPktBufMng.dwCurePBMSize);
		if (NULL == pTmp)
		{
			//�����ڴ�ʧ��
			ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[aclInsertPktBufMng] alloc large memory<%d Byte> error\n", nDataLen + ptNewNode->tPktBufMng.dwCurePBMSize);
			unlockLock(ptSockManage->m_hLock);
			return ACL_ERROR_MALLOC;
		}
		//����Ǩ��
		memcpy(pTmp, ptNewNode->tPktBufMng.pPktBufMng, ptNewNode->tPktBufMng.dwCurePBMSize);
		//�ͷž��ڴ�
		aclFree(ptNewNode->tPktBufMng.pPktBufMng);
		//�������ڴ��
		ptNewNode->tPktBufMng.pPktBufMng = pTmp;
		ACL_DEBUG(E_MOD_MSG, E_TYPE_NOTIF, "[aclInsertPktBufMng] realloc large memory <%d byte>\n",nDataLen + ptNewNode->tPktBufMng.dwCurePBMSize);
	}
	//�����µİ�Ƭ��
	memcpy((unsigned char *)ptNewNode->tPktBufMng.pPktBufMng + ptNewNode->tPktBufMng.dwCurePBMSize, data, nDataLen);
	//����PBMSize
	ptNewNode->tPktBufMng.dwCurePBMSize += nDataLen;

	//��ʼ����Ƿ��п��õİ�
	nRet = aclCheckPktBufMngContent(&ptNewNode->tPktBufMng, &nCheckSize);
	while(0 == nRet && 0 != nCheckSize)
	{
		TAclMessage * ptAclMsg = NULL;
		//���ֿ��õİ�,ֱ�ӷ��ͳ�ȥ
		ptAclMsg = (TAclMessage *)ptNewNode->tPktBufMng.pPktBufMng;
		ptAclMsg->m_pContent = (u8 *)((char *)ptAclMsg + sizeof(TAclMessage));

		//as a local message handle
		aclMsgPush(ptAclMsg->m_dwSrcIID, 
			ptAclMsg->m_dwDstIID, 
			ptAclMsg->m_dwSessionID, 
			ptAclMsg->m_wMsgType, 
			ptAclMsg->m_pContent, 
			ptAclMsg->m_dwContentLen, E_PMT_N_L);
		
		//���µ��������������ݵ�λ��
		memmove(ptNewNode->tPktBufMng.pPktBufMng,
			    (unsigned char *)((unsigned char *)ptNewNode->tPktBufMng.pPktBufMng + nCheckSize), 
			    ptNewNode->tPktBufMng.dwCurePBMSize - nCheckSize);

		//���µ���PMBSize
		ptNewNode->tPktBufMng.dwCurePBMSize -= nCheckSize;

		nCheckSize = 0;
		nRet = 0;
		//�ٴμ��
		nRet = aclCheckPktBufMngContent(&ptNewNode->tPktBufMng, &nCheckSize);
	}

	unlockLock(ptSockManage->m_hLock);
	return ACL_ERROR_NOERROR;
}
