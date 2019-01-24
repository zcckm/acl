//******************************************************************************
//模块名	： acl_socket
//文件名	： acl_socket.c
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL socket连接功能
//------------------------------------------------------------------------------
//修改记录:
//2015-01-22 zcckm 创建
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
typedef struct
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

//3A
#define SELECT_3A_INTERVAL 100
//3A线程会检测超时，如果新连接的Socket没有在此时间内完成验证，连接会被重置
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

//调用函数 会生成一个指定绑定端口的本地监听点，此时，调用者成为acl服务端，处理客户端的连接请求，负责分配本程序的全局会话ID
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
//函 数 名：aclTCPConnect
//功	    能：作为客户端连接ACL服务器
//算法实现：
//全局变量：
//参	    数：pNodeIP: 服务端IP
//            wPort: 服务端端口
//注    意: 
//=============================================================================
ACL_API int aclTCPConnect__(s8 * pNodeIP, u16 wPort)
{
	H_ACL_SOCKET hSocket = INVALID_SOCKET;
	char szRcvSnd3AData[RS_SKHNDBUF_LEN] = {0};
	int nRet = 0;
	int nRecvData = 0;
	u32 dwSuggestID = 0;
	struct sockaddr_in nAddr;
	//接收一次服务端建议ID
	TIDNegot * ptIDNegot = (TIDNegot *)szRcvSnd3AData;

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
	ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclTCPConnect]-[Client Side] start Connect IP:%s PORT:%d...",pNodeIP, ntohs(nAddr.sin_port));
	nRet = connect(hSocket, (struct sockaddr*)&nAddr, sizeof(nAddr));
	if (nRet < 0)
	{
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY,"failed\n");
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR,"[acl_sock] connect failed EC:%d\n",nRet);
		aclCloseSocket(hSocket);
		return ACL_ERROR_FAILED;
	}
	ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY,"ok\n");
	memset(szRcvSnd3AData,0,RS_SKHNDBUF_LEN);

	//Step 0.1 recv 3A data from server
    ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclTCPConnect]-[Step 0.1, 3A Check, Client Side] trying get recv 3A Negotiation, Packet 1...\n");
	nRecvData = aclTcpRecv(hSocket, szRcvSnd3AData, RS_SKHNDBUF_LEN);
	if (sizeof(TIDNegot) + ptIDNegot->m_dwPayLoadLen != nRecvData)
	{
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTCPConnect]-[Step 0.1, 3A Check, Client Side] Recv 3A data failed MsgSize: [Should %d, Real: %d]",
			sizeof(TIDNegot) + ptIDNegot->m_dwPayLoadLen,
			nRecvData);
		return ACL_ERROR_FAILED;
	}

	if (MSG_3A_CHECK_REQ == ptIDNegot->m_msgIDNegot)
	{
		T3ACheckReq * pt3ACheckReq = (T3ACheckReq *)ptIDNegot->m_arrPayLoad;

		//判定大小端，如果大小端不匹配
		if ((IsBigEndian() && pt3ACheckReq->m_dwIsBigEden) || 
			(!IsBigEndian() && !pt3ACheckReq->m_dwIsBigEden))
		{
			//双方都是大端口，或者双方都是小端口
			//大小端匹配,原样返回
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "[aclTCPConnect][Step 0.1, 3A Check, Client Side] Server And Client Both [%s] Endian\n", pt3ACheckReq->m_dwIsBigEden?"Big":"Small");

			//返回3A Ack消息
			ptIDNegot->m_msgIDNegot = MSG_3A_CHECK_ACK;

			//解密幻数
			pt3ACheckReq->m_MagicNum = lightEncDec(pt3ACheckReq->m_MagicNum);
			nRet = aclTcpSend(hSocket, szRcvSnd3AData, nRecvData);
		}
		else
		{
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTCPConnect][Step 0.1, 3A Check, Client Side] Endian Mismatch Server: [%s Endian], Client: [%s Endian]:\n", pt3ACheckReq->m_dwIsBigEden ? "Big" : "Small", IsBigEndian()?"Big":"Small");
			aclCloseSocket(hSocket);
			return ACL_ERROR_NOTSUP;
		}
	}
	else
	{
		ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTCPConnect][Step 0.1, 3A Check, Client Side] Msg Seq failed:%d\n", nRet);
		aclCloseSocket(hSocket);
		return ACL_ERROR_FAILED;
	}

	ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclTCPConnect]-[Step 1.x, Client Side] trying get recv 3A Negotiation, Packet 2...\n");
	
	//客户端与服务端开始协商,方案如下:
	//1.服务端发送生成的最大网络SID作为建议ID，发送协商信令
	//2.客户端接收后，开始ID跳跃
	//客户端跳跃逻辑如下:
	//2.1.如果SID >= ++CID，则跳跃生成CID使得 CID == SID，将ID注册作为会话ID，客户端发送确认指令，流程结束
	//2.2.如果SID < ++CID,则发送协商信令，将新的CID作为建议ID

	//2.x.如果客户端接收到确认信令，则将确认信令中的SID作为会话ID注册，流程结束

	//3.服务端接收客户端指令
	//3.1如果为确认信令，则协商结束，服务端将协商的ID作为会话ID
	//3.2如果服务端接收到协商信令，则开始ID跳跃
	//服务端跳跃逻辑如下:
	//3.2.1.如果CID >=++SID，则跳跃生成SID == CID，将ID注册作为会话ID，服务端发送确认信令，流程结束
	//3.2.2.如果CID < ++SID, 服务端跳转到流程1

	//3.x.如果服务端接收到确认信令，则将确认信令中的CID作为会话ID注册，流程结束
	BOOL bSessionIDNeg = true;
	while (bSessionIDNeg)
	{
		//接收服务端信令
		nRecvData = aclTcpRecv(hSocket, szRcvSnd3AData, RS_SKHNDBUF_LEN);
		if (sizeof(TIDNegot) + ptIDNegot->m_dwPayLoadLen != nRecvData)
		{
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTCPConnect]-[Step 1, Try Neg, Client Side] Session ID Neg Message Size Error, MsgSize: [Should %d, Real: %d]\n",
				sizeof(TIDNegot),
				nRecvData);
			aclCloseSocket(hSocket);
			return ACL_ERROR_FAILED;
		}
		switch (ptIDNegot->m_msgIDNegot)
		{
		case MSG_TRY_NEGOT:
		{
			//Step 1
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_NOTIF, "[aclTCPConnect]-[Step 1,Try Neg, Client Side] Get Server SuggestID: [%d]\n", ptIDNegot->m_dwSessionID);

			//Step2
			dwSuggestID = aclSSIDGenByStartPos(ptIDNegot->m_dwSessionID);
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclTCPConnect]-[Step 2, Try Neg, Client Side] Client get SuggestID: [%d]\n", dwSuggestID);

			//Step 2.1
			if (dwSuggestID == ptIDNegot->m_dwSessionID)
			{
				ptIDNegot->m_msgIDNegot = MSG_NEGOT_CONFIM;
				ptIDNegot->m_dwSessionID = dwSuggestID;
				ptIDNegot->m_dwPayLoadLen = 0;
				nRet = aclTcpSend(hSocket, szRcvSnd3AData, sizeof(TIDNegot));
				if (nRet < 0)
				{
					ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTCPConnect]-[Step 2.1, Neg Confirm, Client Side] Client send Negotiation Confirm Message Failed: [%d]\n", nRet);
					aclCloseSocket(hSocket);
					return ACL_ERROR_FAILED;
				}
				//2.1任务结束，Client协商完成，流程结束
				ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclTCPConnect]-[Step 2.1, Neg Confirm, Client Side] Negotiation confirm, SSID: [%d]\n", dwSuggestID);

				//协商完成，退出协商流程
				bSessionIDNeg = false;
				break;
			}
			ptIDNegot->m_msgIDNegot = MSG_TRY_NEGOT;
			ptIDNegot->m_dwSessionID = dwSuggestID;
			ptIDNegot->m_dwPayLoadLen = 0;
			//Step 2.2 表示服务端发来的协商失败，客户端推出建议CID作为 SSID
			nRet = aclTcpSend(hSocket, szRcvSnd3AData, sizeof(TIDNegot));
			if (nRet < 0)
			{
				ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTCPConnect]-[Step 2.2, Try Neg, Client Side] Client send Try Negotiation Message Failed: [%d]\n", nRet);
				aclCloseSocket(hSocket);
				return ACL_ERROR_FAILED;
			}
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclTCPConnect]-[Step 2.2, Try Neg, Client Side] Client Send CID: [%d] as SSID\n", dwSuggestID);
			//因为客户端一定会通过ID生成函数，因此可以保证节点数组中的值必定小于此ID
			break;
		}
		case MSG_NEGOT_CONFIM:
		{
			//2.x 服务端协商完成
			dwSuggestID = ptIDNegot->m_dwSessionID;
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclTCPConnect]-[Step 2.x, Neg Confirm, Client Side] Receive SID: [%d] as SSID\n", dwSuggestID);
			bSessionIDNeg = false;
			break;
		}
		default:
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_ERROR, "[aclTCPConnect]-[Step x.x, Client Side] Unsupported Msg: [%d]\n", ptIDNegot->m_msgIDNegot);
			break;
		}
	}

	//current connect is work now,insert to selectLoop now
	ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclTCPConnect] socket: [%d] 3A handshake success, confirm SSID: [%d]\n", hSocket, dwSuggestID);
	TNodeInfo tNodeInfo;
	tNodeInfo.m_dwNodeSSID = dwSuggestID;
	tNodeInfo.m_eNodeType = E_NT_CLIENT;
	nRet = aclInsertSelectLoop(getSockDataManger(), hSocket, aclNewMsgProcess, ESELECT_READ, tNodeInfo, getSockDataManger());
	return nRet;//((TAclMessage *)szRcvSnd3AData)->m_dwSessionID;
}

//=============================================================================
//函 数 名：aclConnClose__
//功	    能：作为客户端连接ACL服务器
//算法实现：
//全局变量：
//参	    数：nNode:连接节点
//注    意: 
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
		//尝试寻找需要关闭的节点所在位置
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

	//找到节点了 
	if (!ptSockManage->m_ptSockNodeArr[i].m_bIsUsed)
	{
		unlockLock(ptSockManage->m_hLock);
		return ACL_ERROR_FAILED;
	}
	aclRemoveSelectLoop(getSockDataManger(), ptSockManage->m_ptSockNodeArr[i].m_hSock,true,false);
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
        //不能增加打印信息，否则在向telnet发消息时候断开，会造成死循环
        return ACL_ERROR_FAILED;
    }
	return ACL_ERROR_NOERROR;
}


//=============================================================================
//函 数 名：aclTcpSendbyNode
//功	  能：通过指定node发送socket消息(这里的Node指网络Node，由连接时服务端分配)
//算法实现：
//注    意:
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
	//注意，当前loop设置为非阻塞读数据。因此如果数据过大，可能出现一包需要分两次读的情况
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
            //3A处理线程用不到心跳检测，因此不创建心跳检测线程
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


ACL_API int aclResetSockNode(HSockManage hSockManage, int nPos, bool bCloseSocket)
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

	if (INVALID_SOCKET != ptSockNode->m_hSock && bCloseSocket)
	{
		if (ptSockNode->m_dwNodeNum > 0)
		{
			//只有Node大于0的节点才会是Client或者Server
			//对于心跳超时，ACL会向注册APPID=1的APP发送心跳
			TDisconnNtf tDisconnNtf;
			tDisconnNtf.m_nSessionID = aclNetNode2Glb(hSockManage, ptSockNode->m_dwNodeNum);
			aclPrintf(true, false, "reset Session ID %d\n", tDisconnNtf.m_nSessionID);
			aclMsgPush(MAKEID(1, 0), MAKEID(1, 0), 0, MSG_DISCONNECT_NTF, (u8 *)&tDisconnNtf, sizeof(tDisconnNtf), E_PMT_L_L);
		}
#ifdef WIN32
        closesocket(ptSockNode->m_hSock);
#elif defined (_LINUX_)
        close(ptSockNode->m_hSock);

#endif
		
	}
	//监听(Telnet监听)节点Context存储的数据为非malloc类型的内容
    //其他类型在调用aclInsertSelectLoop时，content传值为管理实例的指针，如果调用释放会造成严重问题
    //当前只有3A验证的时候会输入malloc的值用于存储验证信息，此时的Context是需要释放的
    //3A验证调用aclInsertSelectLoop使用的WaitEvent是ESELECT_CONN，表示等待验证并最终通过连接
    //其他当前都使用的Read，包括监听节点，只是接收任意请求的Socket连接，最终还要给3A验证
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
		if (ptSockNode[i].m_bIsUsed && INVALID_SOCKET != ptSockNode[i].m_hSock)
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
        lockLock(ptSockManage->m_hLock);
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
// 					if (ESELECT_CONN & ptSockNode->m_eWaitEvent)//this is a 3A process
// 					{
// 						if (ACL_ERROR_NOERROR == nRet)//3A is Pass, delete it
// 						{
// 							//when time is up 3A thread will delete it automatically
// 							//set INVALID_SOCKET can prevent thread from closing socket
// 							ptSockNode->m_hSock = INVALID_SOCKET;
// 						}
// 					}
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
        unlockLock(ptSockManage->m_hLock);
	}
	else
	{
		aclDelay(dwWaitMilliSec);
	}
	return ACL_ERROR_NOERROR;
}




//心跳检测线程
//循环向存在socket的会话发送心跳包，同时检测反馈状态
//当会话超过设置的超时时间，则认定超时，直接断开
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
        //监听节点不会被心跳检测，因此不用担心监听节点被重置
		if( ptSockNode[i].m_nHBCount > HB_TIMEOUT/(int)dwWaitMilliSec)
		{
			//time is up, remove current 3A check
			ACL_DEBUG(E_MOD_HB, E_TYPE_KEY, "[aclSockHBCheckLoop] TimeOut Reset Socket:[%X], Pos: [%d]\n", ptSockNode[i].m_hSock, i, ptSockNode[i].m_nHBCount);
			aclResetSockNode((HSockManage)ptSockManage, i, true);
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
//函 数 名：aclCreateNode
//功	    能：创建socket节点，初始化节点管理连接单元,指定好监听端口，设置限制条件
//算法实现：
//全局变量：
//参	    数： hSockMng 节点管理句柄
//          u16 wPort 本地监听端口
//     FEvSelect pfCB 监听有情况时的回调函数
//注    意: 
//返 回 值：返回当前内部Node号，只能调用时候分配
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
	TNodeInfo tNodeInfo;
	memset(&tNodeInfo, 0, sizeof(tNodeInfo));
	tNodeInfo.m_dwNodeSSID = 0;
	tNodeInfo.m_eNodeType = E_NT_LISTEN;

	nListNodeID = aclInsertSelectLoop(hSockMng, hSockNode, pfCB, ESELECT_CONN, tNodeInfo, pContext);
	
	return ACL_ERROR_NOERROR;

}

//=============================================================================
//函 数 名：aclInsertSelectLoop
//功	    能：将socket插入指定的select循环
//算法实现：
//全局变量：
//参	    数：hSockMng: 要插入的socket管理对象指针
//             hSock: 要插入的Socket
//              pfCB: 消息响应函数
//          eSelType: socket消息检测类型
//      nInnerNodeID: 内部节点号
//          pContext: 节点附带参数
//注    意: 此函数用于插入一个新的节点，用的最多的是ESELECT_READ 类型用于接收消息
//          并通知注册的函数。此函数用于配合数据处理，3A验证，心跳检测线程使用
//内部节点号 nInnerNodeID 
//-1 表示是监听ID，值无所谓
//=0  表示是服务器插入新的socket，ID要与本地全局ID一致
//>0 表示是客户端插入新的socket，ID需要输入到节点，并分配全局ID与之对应
//=============================================================================
ACL_API int aclInsertSelectLoop(HSockManage hSockMng, H_ACL_SOCKET hSock, FEvSelect pfCB, ESELECT eSelType, TNodeInfo tNodeInfo, void * pContext)
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
		//这里每次自加貌似没错，其实造成累加
//		ptNewNode = ptNewNode + i;//NB BUG 
		ptNewNode = &ptSockManage->m_ptSockNodeArr[i];
		if (!ptNewNode->m_bIsUsed)//find a node which have not used yet
		{
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclInsertSelectLoop] SocketMng Index:[%d] has not used SEL TYPE: [%s]\n",i, mapSelectTypePrint[eSelType].c_str());
			ptNewNode->m_eWaitEvent = eSelType;
			if (ESELECT_READ == eSelType)
			{
				//只有纯recv类型的socekt才需要包缓冲管理器，用于管理黏包，半包
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
			ptNewNode->m_nHBCount = 0;
			ptNewNode->m_dwNodeNum = tNodeInfo.m_dwNodeSSID;// Node start at 1
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
	ptNewNode->m_eNodeType = tNodeInfo.m_eNodeType;
	//存放全局ID
	ptSockManage->m_dwNodeMap[nFindNode] = tNodeInfo.m_dwNodeSSID;

    ACL_DEBUG(E_MOD_NETWORK, E_TYPE_KEY, "[aclInsertSelectLoop] insert a Node SSID: [%d] type : [%s], OBJ: [%X]\n",
		tNodeInfo.m_dwNodeSSID, mapNodeTypePrint[tNodeInfo.m_eNodeType].c_str(), hSockMng);

	unlockLock(ptSockManage->m_hLock);
	return tNodeInfo.m_dwNodeSSID;// return GLBID
}

ACL_API int aclInsertSelectLoopUnsafe(HSockManage hSockMng, H_ACL_SOCKET hSock, FEvSelect pfCB, ESELECT eSelType, TNodeInfo tNodeInfo, void * pContext)
{
    int i;
    TSockManage * ptSockManage = NULL;
    TSockNode * ptNewNode = NULL;
    int nFindNode = -1;
    u32 dwGlbID = 0;
    ptSockManage = (TSockManage *)hSockMng;
	CHECK_NULL_RET_INVALID(ptSockManage);

    ptNewNode = ptSockManage->m_ptSockNodeArr;
    for (i = 0; i < ptSockManage->m_nTotalNode; i++)
    {
        //这里每次自加貌似没错，其实造成累加
        //		ptNewNode = ptNewNode + i;//NB BUG 
        ptNewNode = &ptSockManage->m_ptSockNodeArr[i];
        if (!ptNewNode->m_bIsUsed)//find a node whitch have not used yet
        {
			ACL_DEBUG(E_MOD_NETWORK, E_TYPE_DEBUG, "[aclInsertSelectLoop] SocketMng Index:[%d] has not used SEL TYPE: [%s]\n", i, mapSelectTypePrint[eSelType].c_str());
            ptNewNode->m_eWaitEvent = eSelType;
            if (ESELECT_READ == eSelType)
            {
                //只有纯recv类型的socekt才需要包缓冲管理器，用于管理黏包，半包
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
			ptNewNode->m_nHBCount = 0;
            ptNewNode->m_dwNodeNum = tNodeInfo.m_dwNodeSSID;// Node start at 1
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
        ACL_DEBUG(E_MOD_NETWORK, E_TYPE_WARNNING, "[aclInsertSelectLoopUnsafe] have not find new node to insert\n");
        return ACL_ERROR_OVERFLOW;
    }
	ptNewNode->m_eNodeType = tNodeInfo.m_eNodeType;
	//存放全局ID
	ptSockManage->m_dwNodeMap[nFindNode] = tNodeInfo.m_dwNodeSSID;

	ACL_DEBUG(E_MOD_NETWORK, E_TYPE_DEBUG, "[aclInsertSelectLoop] insert a Node SSID: [%d] type : [%s]\n", 
		tNodeInfo.m_dwNodeSSID, mapNodeTypePrint[tNodeInfo.m_eNodeType].c_str());

    return tNodeInfo.m_dwNodeSSID;// return GLBID
}



//删除指定的socket
//HSockManage hSockMng     节点管理句柄
//H_ACL_SOCKET hSock       需要删除的Socket
ACL_API int aclRemoveSelectLoop(HSockManage hSockMng, H_ACL_SOCKET hSock, bool bCloseSock, bool bThreadSafe)
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
	if (bThreadSafe)
	{
		lockLock(ptSockManage->m_hLock);
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
			//根据需要，决定是否需要重置Socket
			aclResetSockNode(hSockMng, i, bCloseSock);
			nFindNode = i;
            ptNewNode->m_bIsUsed = FALSE;
			break;
		}
	}
	if (bThreadSafe)
	{
		unlockLock(ptSockManage->m_hLock);
	}
	return nFindNode;
}


//heart beat confirmed
//=============================================================================
//函 数 名：aclHBConfirm
//功	    能：心跳响应
//算法实现：
//全局变量：
//参	    数：hSockMng: 所属Socket管理器句柄
//             nNode: 确认连接的节点号
//注    意: 
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


//节点映射设置函数，将内部节点与全局节点映射，返回全局节点
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
//函 数 名：aclConnect3AThread
//功	    能：3A检测管理线程
//算法实现：
//全局变量：
//参	    数：pParam： socket管理对象指针
//注    意: 此线程用于3A验证，所有的客户端请求先由3A线程处理，通过验证则投送到
//          数据处理线程，此函数有定时断链功能，一定时间内如果通不过3A，则直接断链
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
		lockLock(ptSockManage->m_hLock);
		for (i = 0; i < ptSockManage->m_nTotalNode; i++)
		{
			//是读且不可以为Conn的对象，需要进行超时检测
			if ((ptSockNode[i].m_eWaitEvent & ESELECT_READ) &&
				!(ptSockNode[i].m_eWaitEvent & ESELECT_CONN))
			{
				if (ptSockNode[i].m_nSelectCount * SELECT_3A_INTERVAL > CHECK_3A_TIMEOUT)
				{
					//time is up, remove current 3A check
					ACL_DEBUG(E_MOD_MANAGE, E_TYPE_KEY, "[aclConnect3AThread] 3A CheckSocket [0X%X], Select Count:[%d], timeout, Delete it \n",
						ptSockNode[i].m_hSock, ptSockNode[i].m_nSelectCount);
					aclResetSockNode((HSockManage)ptSockManage, i, true);
				}
			}
		}
		unlockLock(ptSockManage->m_hLock);
		aclSelectLoop(ptSockManage, SELECT_3A_INTERVAL);
		
	}

    //3A Thread is exit , reset all SockNode
	lockLock(ptSockManage->m_hLock);
    for (i = 0; i < ptSockManage->m_nTotalNode; i++)
    {
        if (ptSockNode[i].m_eWaitEvent & ESELECT_READ)
        {
                //reset
				ACL_DEBUG(E_MOD_MANAGE, E_TYPE_KEY, "[aclConnect3AThread] aclConnect3AThread terminated, Read event reset Socket,  Pos: [%d]\n");
                aclResetSockNode((HSockManage)ptSockManage, i, true);
        }
        //socket has already finish 3A, reset it now
        if (ptSockNode[i].m_eWaitEvent & ESELECT_CONN && INVALID_SOCKET == ptSockNode[i].m_hSock)
        {
			ACL_DEBUG(E_MOD_MANAGE, E_TYPE_KEY, "[aclConnect3AThread] aclConnect3AThread terminated, Connect event reset Socket reset Socket Pos: [%d]\n");
            aclResetSockNode((HSockManage)ptSockManage, i, true);
        }
    }
    ptSockManage->m_eMainTaskStatus = E_TASK_ALREADY_EXIT;
	unlockLock(ptSockManage->m_hLock);
	ACL_DEBUG(E_MOD_MANAGE, E_TYPE_KEY,"[aclConnect3AThread] aclConnect3AThread terminated\n");
	return ACL_ERROR_NOERROR;
}

//=============================================================================
//函 数 名：aclDataProcThread
//功	    能：数据收发线程
//算法实现：
//全局变量：
//参	    数：pParam： socket管理对象指针
//注    意: 此线程用于接收所有网络接收的消息，并将其发往指定的APP Instance中
//          Telnet调试的监听也由此线程维护，收到telnet请求则也会发往对应的调试APP中
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
//函 数 名：aclHBDetectThread
//功	    能：心跳检测线程
//算法实现：
//全局变量：
//参	    数：pParam： socket管理对象指针
//注    意: 此线程管理所有通过3A验证会话的连接情况，和线程检测线程同时创建
//          客户端(改为服务端更合适)一方发起心跳检测，CS各自检测超时则自动断链
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
  函数名：IsBigEndian
  功能：判断当前系统大端还是小端
  算法实现：利用大小端地址分配时高低字节位置的原理判断
  输入参数说明：
			u8 * pBuffer 需要处理的数据数组
			u8 byByteOffset 指定字节起始于pBuffer间的偏移
			u8 dwBitsLen 需要截取字节的长度
  返回值说明: TRUE 大端 FALSE 小端
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
            aclPrintf(TRUE, FALSE, "SSID: [%d]\tSocket: [%X]\t NodeType: [%s]\n", 
               ptSockManage->m_ptSockNodeArr[i].m_dwNodeNum,
               ptSockManage->m_ptSockNodeArr[i].m_hSock,
               (E_NT_LISTEN == ptSockManage->m_ptSockNodeArr[i].m_eNodeType)? "NODE_LISTEN":\
               (E_NT_CLIENT == ptSockManage->m_ptSockNodeArr[i].m_eNodeType)? "NODE_CLIENT":"NODE_SERVER");
        }
    }
    aclPrintf(TRUE, FALSE, "*********************Node List end  *********************\n");

}


ACL_API void aclShow3ASocket()
{
	TSockManage *  ptSockManage = (TSockManage *)getSock3AManger();
	int i = 0;
	CHECK_NULL_RET(ptSockManage);

	aclPrintf(TRUE, FALSE, "*********************Node List start*********************\n");
	for (i = 0; i < ptSockManage->m_nTotalNode; i++)
	{
		if (E_NT_INITED != ptSockManage->m_ptSockNodeArr[i].m_eNodeType)
		{
			aclPrintf(TRUE, FALSE, "SSID: [%d]\tSocket: [%X]\t NodeType: [%s]\n",
				ptSockManage->m_ptSockNodeArr[i].m_dwNodeNum,
				ptSockManage->m_ptSockNodeArr[i].m_hSock,
				(E_NT_LISTEN == ptSockManage->m_ptSockNodeArr[i].m_eNodeType) ? "NODE_LISTEN" : \
				(E_NT_CLIENT == ptSockManage->m_ptSockNodeArr[i].m_eNodeType) ? "NODE_CLIENT" : "NODE_SERVER");
		}
	}
	aclPrintf(TRUE, FALSE, "*********************Node List end  *********************\n");
}

/*====================================================================
  函数名：aclFindSocketNode
  功能：获得Socket节点地址
  输入参数说明：
       hSockMng Socket管理器
	      hSock 目标Socket
    pptSockNode <输出>获得节点地址，空表示没找到
  返回值说明: 请参照ACL错误说明
             
  备注：此函数仅限内部使用，是线程不安全的，需要在具体场景搭配锁后使用
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
		if (ptNewNode->m_bIsUsed && hSock == ptNewNode->m_hSock)//find out this socket
		{
			* pptSockNode = ptNewNode;
			return ACL_ERROR_NOERROR;
			break;
		}
	}
	return ACL_ERROR_EMPTY;
}


/*====================================================================
  函数名：aclCheckPktBufMngContent
  功能：检查是否有可用的包被合成
  输入参数说明：
      ptNewNode Socket节点指针
	     pnSize 输出值，用于获得可用尺寸，如果没有则返回0
       
  返回值说明: 请参照ACL错误说明
             
  备注：内部使用，是线程不安全的
  ====================================================================*/
ACL_API int aclCheckPktBufMngContent(TPktBufMng * ptPktMng, int * pnSize)
{
	TAclMessage * ptAclMsg = NULL;

	CHECK_NULL_RET_ERR_PARAM(ptPktMng);
	CHECK_NULL_RET_ERR_PARAM(pnSize);
	
	if (ptPktMng->dwCurePBMSize < sizeof(TAclMessage))
	{
		//数据包头的大小都没达到，无法判断包状态
		return ACL_ERROR_FAILED;
	}
	ptAclMsg = (TAclMessage *)ptPktMng->pPktBufMng;

	if (ptAclMsg->m_dwPackLen <= ptPktMng->dwCurePBMSize)
	{
		//发现完整包,可以输出
		ACL_DEBUG(E_MOD_MSG, E_TYPE_NOTIF,"[aclCheckPktBufMngContent] find a new packet At Receive Cache,  BufData:[%d], PktLen: [%d], Splicing MsgType: [%d],\n", ptPktMng->dwCurePBMSize, ptAclMsg->m_dwPackLen, ptAclMsg->m_wMsgType);
		* pnSize = ptAclMsg->m_dwPackLen;
		return ACL_ERROR_NOERROR;
	}
	ACL_DEBUG(E_MOD_MSG, E_TYPE_NOTIF,"[aclCheckPktBufMngContent] no new packet found yet firstPKT Len %d, PMB Size %d\n", ptAclMsg->m_dwPackLen, ptPktMng->dwCurePBMSize);

	//目前还没有组成一包
	return ACL_ERROR_FAILED;
}


/*====================================================================
  函数名：aclInsertPBMAndSend
  功能：将接收到的ACL消息数据插入缓冲包,并择机发送
  输入参数说明：
           data 输入的数据
	   nDataLen 输入数据的长度

  返回值说明: 
             >0 出现了正常的包，
			 =0 正常获得一包数据，且后面没有可用包了。
  备注：发现正常接收ACL消息的时，出现大量黏包，半包现象，使用此函数实现包数据的管理
        另:黏包，半包都会顺序发生，因此只有当接收到的数据不正常的时候才使用包管理器
		  :目前只是用包长定位完整包，之后需要考虑使用checksum，更安全
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
	//lockLock(ptSockManage->m_hLock);
	nRet = aclFindSocketNode(hSockMng, hSock, &ptNewNode);
	if(ACL_ERROR_NOERROR != nRet || NULL == ptNewNode)
	{
		//寻找SocketNode出现错误
		ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[aclInsertPBMAndSend] cannot find target socket node SOCK[%d]\n",hSock);
		//unlockLock(ptSockManage->m_hLock);
		return ACL_ERROR_FAILED;
	}

	//将新的数据插入包缓冲

	if ((nDataLen + ptNewNode->tPktBufMng.dwCurePBMSize) > INIT_PACKET_BUFFER_MANAGER)
	{
		//即将超过PBM上限，怎么办呢?根据当前实际需要的大小，分配更高的空间
		void * pTmp = aclMallocClr(nDataLen + ptNewNode->tPktBufMng.dwCurePBMSize);
		if (NULL == pTmp)
		{
			//分配内存失败
			ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[aclInsertPktBufMng] alloc large memory<%d Byte> error\n", nDataLen + ptNewNode->tPktBufMng.dwCurePBMSize);
			//unlockLock(ptSockManage->m_hLock);
			return ACL_ERROR_MALLOC;
		}
		//数据迁移
		memcpy(pTmp, ptNewNode->tPktBufMng.pPktBufMng, ptNewNode->tPktBufMng.dwCurePBMSize);
		//释放旧内存
		aclFree(ptNewNode->tPktBufMng.pPktBufMng);
		//设置新内存块
		ptNewNode->tPktBufMng.pPktBufMng = pTmp;
		ACL_DEBUG(E_MOD_MSG, E_TYPE_NOTIF, "[aclInsertPktBufMng] realloc large memory <%d byte>\n",nDataLen + ptNewNode->tPktBufMng.dwCurePBMSize);
	}
	//插入新的包片段
	memcpy((unsigned char *)ptNewNode->tPktBufMng.pPktBufMng + ptNewNode->tPktBufMng.dwCurePBMSize, data, nDataLen);
	//更新PBMSize
	ptNewNode->tPktBufMng.dwCurePBMSize += nDataLen;

	//开始检查是否有可用的包
	nRet = aclCheckPktBufMngContent(&ptNewNode->tPktBufMng, &nCheckSize);
	while(0 == nRet && 0 != nCheckSize)
	{
		TAclMessage * ptAclMsg = NULL;
		//发现可用的包,直接发送出去
		ptAclMsg = (TAclMessage *)ptNewNode->tPktBufMng.pPktBufMng;
		ptAclMsg->m_pContent = (u8 *)((char *)ptAclMsg + sizeof(TAclMessage));

		//as a local message handle
		aclMsgPush(ptAclMsg->m_dwSrcIID, 
			ptAclMsg->m_dwDstIID, 
			ptAclMsg->m_dwSessionID, 
			ptAclMsg->m_wMsgType, 
			ptAclMsg->m_pContent, 
			ptAclMsg->m_dwContentLen, E_PMT_N_L);
		
		//重新调整包缓冲中数据的位置
		memmove(ptNewNode->tPktBufMng.pPktBufMng,
			    (unsigned char *)((unsigned char *)ptNewNode->tPktBufMng.pPktBufMng + nCheckSize), 
			    ptNewNode->tPktBufMng.dwCurePBMSize - nCheckSize);

		//重新调整PMBSize
		ptNewNode->tPktBufMng.dwCurePBMSize -= nCheckSize;

		nCheckSize = 0;
		nRet = 0;
		//再次检查
		nRet = aclCheckPktBufMngContent(&ptNewNode->tPktBufMng, &nCheckSize);
	}

	//unlockLock(ptSockManage->m_hLock);
	return ACL_ERROR_NOERROR;
}


/*====================================================================
函数名：aclInsertPBMAndSend
功能：返回数据包管理器当前缓冲数据的大小
====================================================================*/
ACL_API int aclGetPBMLeftDataSize(HSockManage hSockMng, H_ACL_SOCKET hSock, int & nLeftDataLen)
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
	//lockLock(ptSockManage->m_hLock);
	nRet = aclFindSocketNode(hSockMng, hSock, &ptNewNode);
	if (ACL_ERROR_NOERROR != nRet || NULL == ptNewNode)
	{
		//寻找SocketNode出现错误
		ACL_DEBUG(E_MOD_MSG, E_TYPE_ERROR, "[aclInsertPBMAndSend] cannot find target socket node SOCK[%d]\n", hSock);
		//unlockLock(ptSockManage->m_hLock);
		return ACL_ERROR_FAILED;
	}

	nLeftDataLen = ptNewNode->tPktBufMng.dwCurePBMSize;
	return ACL_ERROR_NOERROR;
}