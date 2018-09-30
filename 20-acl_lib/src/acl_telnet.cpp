//******************************************************************************
//ģ����	�� acl_telnet
//�ļ���	�� acl_telnet.h
//����	�� zcckm
//�汾	�� 1.0
//�ļ�����˵��:
//ACL telnet ����
//------------------------------------------------------------------------------
//�޸ļ�¼:
//2015-01-20 zcckm ����
//******************************************************************************

#include "acl_telnet.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "acl_task.h"
#include "acl_manage.h"
#include "acl_socket.h"

/* max argument number of input command */
#define MAX_ARGC			10
#define CMDLINE_LEN         32
#define CMD_PROMPT_LEN      64

#define ACL_TELNET_PORT_NUM 10
#define ACL_TEL_PROMPT_LEN  255
#define ACL_TEL_CMD_LEN     255
#define MAX_CMD_LIST        100

typedef struct tagAclTelInfo TAclTel;
typedef struct tagCmdEntry TCmdEntry;

const char * p_EDEBUG_TYPE[] = 
{
    "E_TYPE_NONE",
	"E_TYPE_ERROR",
	"E_TYPE_WARNNING",
	"E_TYPE_NOTIF",
    "E_TYPE_DEBUG",
    "E_TYPE_RESERVED_0",
    "E_TYPE_RESERVED_1",
    "E_TYPE_RESERVED_2",
    "E_TYPE_ALL"
};


const char  * p_EDEBUG_MODULE[] = 
{
    "E_MOD_NONE",
    "E_MOD_LOCK",//��
    "E_MOD_NETWORK",//�����շ���أ�ƫ����Socket
    "E_MOD_HB",//����
    "E_MOD_MSG",//message,��Ϣ�շ��߼����
    "E_MOD_MANAGE",//����
    "E_MOD_TELNET",//telnet
    "E_MOD_TIMER",//��ʱ��
    "E_MOD_ALL" //����ģ��
};

int g_DebugType = E_TYPE_NOTIF;
int g_DebugMod = E_MOD_MANAGE;

BOOL g_bDebugInfoWriteInFile = FALSE;


typedef int (* REGCBFUN)(int,int,int,int,int,int,int,int,int,int);
struct tagCmdEntry {
	char command[CMDLINE_LEN];
	int (*function)(int,int,int,int,int,int,int,int,int,int);
	//	void (*function)(int, char **);
	char prompt[CMD_PROMPT_LEN];
};


typedef s32 (*FTelCmd)(void *);

struct tagAclTelInfo
{
	u16 m_wTelPort;
	char m_szPrompt[ACL_TEL_PROMPT_LEN];
	FTelCmd m_cbTelCmd;

	char m_szCmd[ACL_TEL_CMD_LEN];
	int nPos;
};

//just support 1 telnet port now
static TAclTel g_szAclTelList[ACL_TELNET_PORT_NUM];
static TCmdEntry g_szCmdList[MAX_CMD_LIST];
static H_ACL_SOCKET g_telFd = INVALID_SOCKET;

#define HISTORY_CMD_LEN 32
#define HISTORY_CMD_NUM 20

//history input command support max 
static char szHistoryCmd[HISTORY_CMD_NUM][HISTORY_CMD_LEN + 1];
static int nCmdCursor = 0;
static int nDspCursor = 0;
#ifdef _MSC_VER

HANDLE hShellProc = NULL;
HANDLE hShellThread = NULL;

#endif
ACL_API int aclECHO(H_ACL_SOCKET nFd, const char * param,...);
void chearLine(H_ACL_SOCKET nFd);
void aclTelnetRegFunc();

ACL_API int aclPrintf(int bOpenPrint,int bSaveLog,const char * param,...)
{
    va_list arg_ptr;
    char s[1024];
    int strLen = 0;
    static FILE * fpLog = NULL;

    va_start(arg_ptr,param);
    
    if (!bOpenPrint)
    {
        return -1;
    }
    vsnprintf(s,sizeof(s),param,arg_ptr);

	va_end(arg_ptr);
    strLen = strlen(s);
    if (strLen <= 0)
    {
        return ACL_ERROR_NOERROR;
    }
    if ('\n' == s[strLen - 1])
    {
        s[strLen - 1] = '\r';
        s[strLen] = '\n';
        s[strLen + 1] = '\0';
    }
	if(INVALID_SOCKET != g_telFd)
	{
        aclTcpSend(g_telFd, s,strlen(s));
	}
	//printf("%s",s);
    

    if (bSaveLog)//need write in log file
    {
        if (NULL == fpLog)
        {
            fpLog = fopen("watch.log","ab");
        }
        fprintf(fpLog,"%s",s);
    }
    return ACL_ERROR_NOERROR;
}


ACL_API int ACL_DEBUG(int MODULE, int TYPE,const char * param,...)
{
    static FILE * fpLog = NULL;

	//���ڵ���E_MOD_ALL����Ϣ����ȫ���ſ���ӡ
	//С��E_MOD_ALL����Ϣ����ģ���ӡ

	//С�ڵ��ڵ������Ϳ���ֵ����Ϣȫ����ӡ
	if ((g_DebugMod < E_MOD_ALL && MODULE != g_DebugMod)
		|| g_DebugType < TYPE)
	{
		return ACL_ERROR_FILTER;
	}
	if (1)
    {
        int strLen = 0;
        va_list arg_ptr;
        char s[1024];
        va_start(arg_ptr,param);
        vsnprintf(s,sizeof(s),param,arg_ptr);
        va_end(arg_ptr);
        strLen = strlen(s);
        if (strLen <= 0)
        {
            return ACL_ERROR_NOERROR;
        }
        if ('\n' == s[strLen - 1])
        {
            s[strLen - 1] = '\r';
            s[strLen] = '\n';
            s[strLen + 1] = '\0';
        }
        
        if(INVALID_SOCKET != g_telFd)
        {
            aclTcpSend(g_telFd, s,strlen(s));
        }
        //printf("%s",s);
        if (g_bDebugInfoWriteInFile)//need write in log file
        {
            if (NULL == fpLog)
            {
                fpLog = fopen("log.log","ab");
            }
            fprintf(fpLog,"%s",s);
        }
    }
    return ACL_ERROR_NOERROR;
}


//-->����ע�����
enum
{
	ARGV_STRING = 1,
	ARGV_INTEGER = 2,
};

//��ָ�������������argvָ��������
//���ز�������+1(argv[0]�Ǻ�����)
int parse_param(char *buf, char **argv, int * argvType)
{
	int argc = 0;
	int nParamType = 0;

	CHECK_NULL_RET_INVALID(buf);

	//filter front space
	while(*buf == ' ')
	{
		buf++;
	}
	//filter func name
	if ('\0' != buf)
	{
		argv[argc] = buf;
		argc++;
	}
	
	while((*buf != '\0'))
	{
		if (*buf != ' ')
		{
			buf ++;
		}
		else
		{
			*buf = '\0';
			buf ++;
			break;
		}
	}

	while((argc < MAX_ARGC) && (*buf != '\0')) 
	{
		if (ARGV_STRING == nParamType)
		{
			BOOL bFindEnd = FALSE;
			argv[argc] = buf;//start of string(started at A: "ABCD")
			argvType[argc] = ARGV_STRING;
			while(*buf != '\0')
			{
				if( '"' == *buf )
				{
					bFindEnd = TRUE;
					break;
				}
				else
					buf++;
			}
			if (bFindEnd)//find end of string
			{
				*buf = '\0';
				argc++;
			}
			else//invalid param( "ABCD )
			{
				return -1;
			}
		}
		if ('"' == *buf)//it is a string type 
		{
			nParamType = ARGV_STRING;
		}
		else// it is a integer type
		{
			BOOL bFindEnd = FALSE;
			argv[argc] = buf;
			argvType[argc] = ARGV_INTEGER;
			while(*buf != '\0')
			{
				if (*buf != ' ')
				{
					buf++;
				}
				else
				{
					bFindEnd = TRUE;
					argc++;
					*buf = '\0';
					break;
				}
			}
			if (!bFindEnd)
			{
				argc++;
			}
		}
		buf++;
	}

	return argc;
}


void checkCommandHelp(void)
{
	TCmdEntry * ptCmdEntry =  g_szCmdList;
	int i = 0;
	CHECK_NULL_RET(ptCmdEntry);

	aclPrintf(TRUE,FALSE, "================help info ==================\n\n");
	for (i = 0; i < MAX_CMD_LIST; i++)
	{
		if (NULL == ptCmdEntry[i].function)//end of reg list
		{
			aclPrintf(TRUE,FALSE, "\n================help info ==================\n\r");
			return;
		}
		aclPrintf(TRUE,FALSE, "\r%s\t Info:%s\n", ptCmdEntry[i].command, ptCmdEntry[i].prompt);
	}
}

//��������������Ƿ��������б���
//������ִ��ע��ĺ���
BOOL checkCommand(char * cmd)
{
	char *argv[MAX_ARGC + 1] = {NULL};
	int argvType[MAX_ARGC + 1] = {0};
	int param[MAX_ARGC] = {0};
	int i = 0;
	int nFind = -1;
	int argc = 0;
	TCmdEntry * ptCmdEntry =  g_szCmdList;
	CHECK_NULL_RET_FALSE(ptCmdEntry);
	argc = parse_param(cmd, argv, argvType);
	if (-1 == argc)//invalid param
	{
		return FALSE;
	}	 
	if (0 == argc) // no command, all ' '
	{
		return TRUE;
	}
	for(i = 0; i < MAX_ARGC; i++)
	{
		if (ARGV_STRING == argvType[i + 1])
		{
			param[i] = *((int*)&argv[i + 1]);
		}
		else if (ARGV_INTEGER == argvType[i + 1])
		{
			param[i] = atoi(argv[i + 1]);
		}
		else
			break;
	}
	
	for (i = 0; i < MAX_CMD_LIST; i++)
	{
#ifdef WIN32
        if (0 == _stricmp(argv[0], ptCmdEntry[i].command)) // find command
#elif defined (_LINUX_)
        if (0 == strcasecmp(argv[0], ptCmdEntry[i].command)) // find command
#endif
		{
			(*ptCmdEntry[i].function)(param[0], param[1], param[2], param[3], param[4], param[5], param[6], param[7], param[8], param[9]);
			nFind = i;
		}
	}
	if (-1 == nFind) // no find cmd
	{
		return FALSE;
	}
	return TRUE;
}

//=============================================================================
//�� �� ����aclRegCommand
//��	    �ܣ����Ժ���ע��
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����pCmd: ע����ú���������
//         pFunc: ע�ắ��ָ��
//       pPrompt: �������ʾ
//ע    ��:ע�ắ��֧�ֲ�����10��������ע���ͨ��Telnet�ͻ��˵��� ע�����ʱ��
//         ���������Ҫ��ע�����ƥ�䣬��������������
//=============================================================================
ACL_API int aclRegCommand(const char * pCmd, void * pFunc, const char * pPrompt)
{
	int i = 0;
	static int nCurPos = 0;
	TCmdEntry * ptCmdEntry = NULL;
	ptCmdEntry = g_szCmdList;
	CHECK_NULL_RET_INVALID(ptCmdEntry);

	CHECK_NULL_RET_ERR_PARAM(pCmd);
	CHECK_NULL_RET_ERR_PARAM(pFunc);

	if (nCurPos < MAX_CMD_LIST)
	{
		ptCmdEntry[nCurPos].function = (REGCBFUN)pFunc;
		strncpy(ptCmdEntry[nCurPos].command, pCmd, sizeof(ptCmdEntry[i].command));
		strncpy(ptCmdEntry[nCurPos].prompt, pPrompt, sizeof(ptCmdEntry[i].prompt));
	}
	nCurPos++;
	/*
	for (i = 0; i < MAX_CMD_LIST; i++)
	{
		if (NULL == ptCmdEntry[i].function)
		{
			ptCmdEntry[i].function = pFunc;
			strncpy(ptCmdEntry[i].command, pCmd, sizeof(ptCmdEntry[i].command));
			strncpy(ptCmdEntry[i].prompt, pPrompt, sizeof(ptCmdEntry[i].prompt));
			return ACL_ERROR_NOERROR;
		}
	}
	*/
	return ACL_ERROR_OVERFLOW;
}



TAclTel * getIdleTelnet( u16 wPort )
{
	int i = 0;
	int nFindPos = -1;
	TAclTel * ptAclTel = NULL;
	ptAclTel = g_szAclTelList;
	CHECK_NULL_RET_NULL(ptAclTel);
	for (i = 0; i< ACL_TELNET_PORT_NUM; i++)
	{
		if (wPort == ptAclTel[i].m_wTelPort)
		{
			//conflict port existed
			return NULL;
		}
		if (0 == ptAclTel[i].m_wTelPort)
		{
			nFindPos = i;
		}
	}
	if (-1 == nFindPos)
	{
		return NULL;
	}
	ptAclTel[nFindPos].m_wTelPort = wPort;
	//set default prompt
	strcpy(ptAclTel[nFindPos].m_szPrompt,"acl_Telnet->");
	return &ptAclTel[nFindPos];
}


ACL_API int setTelnetPrompt(int nPort, const char * pPrompt)
{
	int i = 0;
	TAclTel * ptAclTel = NULL;
	ptAclTel = g_szAclTelList;
	CHECK_NULL_RET_INVALID(ptAclTel);
	if (0 == nPort || NULL == pPrompt)
	{
		return ACL_ERROR_PARAM;
	}
	//set max is 10 character
	if (strlen(pPrompt) > ACL_TEL_PROMPT_LEN)
	{
		//prompt is so long
		return ACL_ERROR_PARAM;
	}
	for (i = 0; i< ACL_TELNET_PORT_NUM; i++)
	{
		if (ptAclTel[i].m_wTelPort == nPort)
		{
			memset(ptAclTel[i].m_szPrompt,0,ACL_TEL_PROMPT_LEN);
			strcpy(ptAclTel[i].m_szPrompt, pPrompt);
			return ACL_ERROR_NOERROR;
		}
	}
	return ACL_ERROR_FAILED;
}

ACL_API char * getTelnetPrompt(u16 wPort)
{
	int i = 0;
	TAclTel * ptAclTel = NULL;
	ptAclTel = g_szAclTelList;
	CHECK_NULL_RET_NULL(ptAclTel);
	if (0 == wPort)
	{
		return NULL;
	}
	for (i = 0; i< ACL_TELNET_PORT_NUM; i++)
	{
		if (ptAclTel[i].m_wTelPort == wPort)
		{
			return ptAclTel[i].m_szPrompt;
		}
	}
	return NULL;
}


ACL_API int aclECHO(H_ACL_SOCKET nFd, const char * param,...)
{
	va_list arg_ptr;
	char s[255];
	memset(s,0,sizeof(s));
	va_start(arg_ptr,param);
	vsnprintf(s,sizeof(s),param,arg_ptr);
	va_end(arg_ptr);
//	printf("%s",s);
	return aclTcpSend(nFd, s,strlen(s));
}



void insertHistoryList(char * pNewCmd)
{
	CHECK_NULL_RET(pNewCmd);

	strncpy(szHistoryCmd[nCmdCursor], pNewCmd, HISTORY_CMD_LEN);

	if(HISTORY_CMD_NUM == nCmdCursor + 1)//at the end of array
	{
		nCmdCursor = 0;
		nDspCursor = 0;
	}
	else
	{
		nCmdCursor += 1;
		nDspCursor = nCmdCursor;
		//seems it is a bug at windows
		//nDspCursor += 1;
	}
}

void * dispArrowUp(H_ACL_SOCKET hfd, char * pPrompt)
{
	void * pCmd = NULL;
	if (INVALID_SOCKET == hfd)
	{
		return NULL;
	}

	// --dispCursor
	if (0 != nDspCursor)
	{
		//disp not at the end of circle list and last circle is not null
		if(nDspCursor - 1 != nCmdCursor && '\0' != szHistoryCmd[nDspCursor - 1][0])
			--nDspCursor;
	}
	else// 0 == nDspCursor 
	{
		if (HISTORY_CMD_NUM - 1 != nCmdCursor && '\0' != szHistoryCmd[HISTORY_CMD_NUM - 1][0])
		{
			nDspCursor = HISTORY_CMD_NUM - 1;
		}
	}
	chearLine(hfd);
	if (NULL != pPrompt)
	{
		aclECHO(hfd, "\r%s", pPrompt);
	}
	aclECHO(hfd, "%s", szHistoryCmd[nDspCursor]);
	pCmd = szHistoryCmd[nDspCursor];
	return pCmd;
}

void * dispArrowDown(H_ACL_SOCKET hfd, char * pPrompt)
{
	void * pCmd = NULL;
	if (INVALID_SOCKET == hfd)
	{
		return NULL;
	}

	// ++dispCursor
	if (HISTORY_CMD_NUM - 1 != nDspCursor)
	{
		//disp not at the end of circle list and last circle is not null
		if((nDspCursor != nCmdCursor) && '\0' != szHistoryCmd[nDspCursor + 1][0])
			++nDspCursor;
	}
	else// HISTORY_CMD_NUM - 1 == nDspCursor //end of the pos
	{
		if (0 != nCmdCursor && '\0' != szHistoryCmd[0][0])
		{
			nDspCursor = 0;
		}
	}
	chearLine(hfd);
	if (NULL != pPrompt)
	{
		aclECHO(hfd, "\r%s", pPrompt);
	}
	aclECHO(hfd, "%s", szHistoryCmd[nDspCursor]);
	pCmd = szHistoryCmd[nDspCursor];
	return pCmd;
}

#define PARSE_ENTER      ((u32)0x01)
#define PARSE_BACKSPACE  ((u32)0x01 << 1)
#define PARSE_ARROUP     ((u32)0x01 << 2)
#define PARSE_ARRODOWN   ((u32)0x01 << 3)
int parseInput(char * pBuf, int nCharLen, u32 * pdwParseParam)
{
	int i = 0;
	int nPos = 0;
	int nEntNum = 0, nBackNum = 0;
	CHECK_NULL_RET_INVALID(pdwParseParam);
	if (NULL == pBuf)
	{
		return 0;
	}
	do 
	{
		if (pBuf[i] >= 0x20 && pBuf[i] <= 0x7E)
		{
			pBuf[nPos] = pBuf[i];
			++nPos;
		}
		else
		{
			//backspace
			if (0x08 == pBuf[i])
			{
				nBackNum++;
				if (0 == nPos)
				{
				}
				else
				{
					pBuf[nPos] = 0;
					--nPos;
				}
			}
			else if (0x0D == pBuf[i])
			{
				nEntNum++;
			}
			else if (0x1B == pBuf[i] && i + 2 < nCharLen )
			{
				if (0x5B == pBuf[i + 1])
				{
					pBuf[i + 1] = 0x01;// filter it
					if (0x41 == pBuf[i + 2])
					{
						pBuf[i + 2] = 0x01;// filter it
						*pdwParseParam |= PARSE_ARROUP;
					}
					else if (0x42 == pBuf[i + 2])
					{
						pBuf[i + 2] = 0x01;// filter it
						*pdwParseParam |= PARSE_ARRODOWN;
					}

				}
			}
		}
	} while (0 != pBuf[i++]);
	if (nEntNum)
	{
		*pdwParseParam |= PARSE_ENTER;
	}
	if (nBackNum)
	{
		*pdwParseParam |= PARSE_BACKSPACE;
	}
	return nPos;
}



#define TELCMD_WILL    (u8)251
#define TELCMD_WONT    (u8)252
#define TELCMD_DO      (u8)253
#define TELCMD_DONT    (u8)254
#define TELCMD_IAC     (u8)255

#define AUTHORIZATION_NAME_SIZE 20

#define TELOPT_ECHO     (u8)1
#define TELOPT_SGA      (u8)3
#define TELOPT_LFLOW    (u8)33
#define TELOPT_NAWS     (u8)34


static void SendIAC(H_ACL_SOCKET nFd, s8 cmd, s8 opt)
{
	s8 buf[5];
	buf[0] = TELCMD_IAC;
	buf[1] = cmd;
	buf[2] = opt;
	aclTcpSend(nFd, buf,3);
	return;
}

void chearLine(H_ACL_SOCKET nFd)
{
	aclECHO(nFd, "\r                                    \r");
	
}


//reset connect status when telnet is disconnected
s32 telResetParam(HAclTel hAclTel)
{
	TAclTel * ptAclTel = (TAclTel *)hAclTel;
	CHECK_NULL_RET_INVALID(ptAclTel);

	g_telFd = INVALID_SOCKET;
	memset(ptAclTel->m_szCmd, 0, ACL_TEL_CMD_LEN);
	ptAclTel->nPos = 0;

	memset(szHistoryCmd, 0, HISTORY_CMD_NUM * (HISTORY_CMD_LEN + 1));
	nCmdCursor = 0;
	nDspCursor = 0;
	return ACL_ERROR_NOERROR;
}

//new send telnet will send according to this callback function
//=============================================================================
//�� �� ����newTelMsgProcess
//��	    �ܣ�telnet���ݴ�����
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����nFd�� ������socket
//       eEvent: �����¼�
//     pContext: ���Ӳ���
//ע    ��: �˺������aclInsertSelectLoopʹ�ã���ʱtelnet�Ѿ������ϣ����ﴦ��
//          ����telnet����������
//=============================================================================
s32 newTelMsgProcess(H_ACL_SOCKET nFd, ESELECT eEvent, void* pContext)
{
	s8 szRcvData[MAX_RECV_PACKET_SIZE] = {0};
	TAclTel * ptAclTel = (TAclTel *)pContext;
	int nRcvSize = 0, i = 0;
	u32 dwParseValue = 0;
	int nRealNum = 0;
	CHECK_NULL_RET_INVALID(ptAclTel);
	if (ESELECT_READ != eEvent)
	{
        ACL_DEBUG(E_MOD_TELNET, E_TYPE_WARNNING, "[newTelMsgProcess] new message but not read???\n");
		return ACL_ERROR_INVALID;
	}

	nRcvSize = aclTcpRecv(nFd, szRcvData, MAX_RECV_PACKET_SIZE);

	if (0 == nRcvSize)//attempt to disconnect current connect
	{
        ACL_DEBUG(E_MOD_TELNET, E_TYPE_WARNNING, "[newTelMsgProcess] telnet is disconnected\n");
        TSockManage * ptSockManage = (TSockManage *)getSockDataManger();
        lockLock(ptSockManage->m_hLock);
		aclRemoveSelectLoop(getSockDataManger(), nFd);
        unlockLock(ptSockManage->m_hLock);
		telResetParam((HAclTel)ptAclTel);
		return ACL_ERROR_NOERROR;
	}


	nRealNum = parseInput(szRcvData, nRcvSize, &dwParseValue);

	//avoid array overflow
	if (nRealNum + ptAclTel->nPos < ACL_TEL_CMD_LEN)
	{
		for (i = 0; i < nRealNum; i++)
		{
			ptAclTel->m_szCmd[ptAclTel->nPos++] = szRcvData[i];
		}
	}

	if (dwParseValue & PARSE_ARROUP)//arrow up
	{
		char * pCmd = (char *)dispArrowUp(nFd, ptAclTel->m_szPrompt);
		if (strlen(pCmd) < ACL_TEL_CMD_LEN)
		{
			memset(ptAclTel->m_szCmd, 0, ACL_TEL_CMD_LEN);
			strncpy(ptAclTel->m_szCmd, pCmd, strlen(pCmd));
			ptAclTel->nPos = strlen(pCmd);
		}

	}
	else if (dwParseValue & PARSE_ARRODOWN)//arrow down
	{
		char * pCmd = (char *)dispArrowDown(nFd, ptAclTel->m_szPrompt);
		if (strlen(pCmd) < ACL_TEL_CMD_LEN)
		{
			memset(ptAclTel->m_szCmd, 0, ACL_TEL_CMD_LEN);
			strncpy(ptAclTel->m_szCmd, pCmd, strlen(pCmd));
			ptAclTel->nPos = strlen(pCmd);
		}
	}

	if ((dwParseValue & PARSE_ENTER) && ptAclTel->nPos)
	{
		aclECHO(nFd, "\r\n");
		//input new function start to search new command
		if (0 == strcmp(ptAclTel->m_szCmd, "bye"))
		{
            ACL_DEBUG(E_MOD_TELNET, E_TYPE_DEBUG, "[newTelMsgProcess] recv CMD:bye SOCK:%X\n",nFd);
            TSockManage * ptSockManage = (TSockManage *)getSockDataManger();
            lockLock(ptSockManage->m_hLock);
			aclRemoveSelectLoop(getSockDataManger() ,nFd);
            unlockLock(ptSockManage->m_hLock);
			telResetParam((HAclTel)ptAclTel);
			return 0;
		}
        //checkCommand will break m_szCmd,so should insert history list earlier
        insertHistoryList(ptAclTel->m_szCmd);

		if (!checkCommand(ptAclTel->m_szCmd))
		{
			aclECHO(nFd, "invalid param:%s\r\n", ptAclTel->m_szCmd);
		}
        aclECHO(nFd, "\r\n");
        aclECHO(nFd, ptAclTel->m_szPrompt);
        
		memset(ptAclTel->m_szCmd, 0, ACL_TEL_CMD_LEN);
		ptAclTel->nPos = 0;

	}
	else
	{
		//just press enter,need change space
		if ((dwParseValue & PARSE_ENTER) && 0 == ptAclTel->nPos)
		{
			aclECHO(nFd, "\r\n");
		}
		if (0 == nRealNum && (dwParseValue & PARSE_BACKSPACE))//nothing
		{
			chearLine(nFd);
			aclECHO(nFd, ptAclTel->m_szPrompt);
			//if buf have value, del one and disp
			//if have no value,just disp enpty
			if (ptAclTel->nPos)//just has a back space
			{
				ptAclTel->m_szCmd[--ptAclTel->nPos] = 0;

			}
			aclECHO(nFd, ptAclTel->m_szCmd);

		}
		else
		{
			//new input char ,need clear and disp all command in buffer
			chearLine(nFd);
			aclECHO(nFd, ptAclTel->m_szPrompt);
			aclECHO(nFd, ptAclTel->m_szCmd);
		}
	}

	//check new input char from 


	return ACL_ERROR_NOERROR;
}



//proc new telnet connect
//=============================================================================
//�� �� ����newTelConnProc
//��	    �ܣ��µ�telnet���Ӵ�����
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����nFd�� ������socket
//       eEvent: �����¼�
//     pContext: ���Ӳ���
//ע    ��: �˺������aclInsertSelectLoopʹ�ã���Ϊtelnet�ļ����ڵ㣬�����µ�
//          telnet��������
//=============================================================================
s32 newTelConnProc(H_ACL_SOCKET nFd, ESELECT eEvent, void* pContext)
{
	H_ACL_SOCKET hConnSock = INVALID_SOCKET;
	TAclTel * ptAclTel = (TAclTel *)pContext;
	char szMarkBuf[RS_SKHNDBUF_LEN] = {0};
	u32 dwConnIP = 0;
	u16 wConnPort = 0;
	int nSendDataLen = 0;
    ACL_DEBUG(E_MOD_TELNET, E_TYPE_DEBUG,"[newTelConnProc] new telnet connect\n");
	if (ESELECT_READ != eEvent)
	{
		return ACL_ERROR_INVALID;
	}
	//new connect 
	hConnSock = aclTcpAccept(nFd, &dwConnIP, &wConnPort);

	//�������ó�֧�ֵ����̻��߶����telnet�ͻ��ˣ���ǰ֧�ֵ�����


	if (INVALID_SOCKET == hConnSock)//connect is failed
	{
        ACL_DEBUG(E_MOD_TELNET, E_TYPE_ERROR,"[newTelConnProc] telnet connect is failed\n");
		return ACL_ERROR_FAILED;
	}
	//���ڿ���ͨ����չ������չtelnet��֤Ҫ��
	//nInnerNodeID �Ծ�����Ϊ-1 ��Ϊtelnet�޷�ͨ��������֤�������Ҫ���䵱�������ڵ㴦��
	/* ����TELE���ԣ���ӡ��ӭ���*/

	
//	SendIAC(hConnSock, TELCMD_DO, TELOPT_ECHO);
//	SendIAC(hConnSock, TELCMD_DO, TELOPT_NAWS);
//	SendIAC(hConnSock, TELCMD_DO, TELOPT_LFLOW);
//	SendIAC(hConnSock, TELCMD_WILL, TELOPT_ECHO);
	SendIAC(hConnSock, TELCMD_WILL, TELOPT_SGA);
	if (INVALID_SOCKET != g_telFd)//already connected
	{
        //disconnect old telnet connect
        //aclRemoveSelectLoop(getSockDataManger(), g_telFd);
        aclCloseSocket(g_telFd);
	}
	g_telFd = hConnSock;

	aclECHO(hConnSock, "*===============================================================*\r\n");
	aclECHO(hConnSock, "***==============  ��ӭʹ��ACL Telnet Server  ================***\r\n");
	aclECHO(hConnSock, "*===============================================================*\r\n");
	aclECHO(hConnSock, ptAclTel->m_szPrompt);
	aclInsertSelectLoop(getSockDataManger(), hConnSock, newTelMsgProcess, ESELECT_READ, -1, pContext);
	//	getTelnetPrompt(wConnPort);
	ACL_DEBUG(E_MOD_TELNET, E_TYPE_DEBUG, "[newTelConnProc]  new telnet connected and Port %d\n",wConnPort);
	aclTcpSend(hConnSock, szMarkBuf, nSendDataLen);

	return ACL_ERROR_NOERROR;
}

//=============================================================================
//�� �� ����aclTelnetInit
//��	    �ܣ�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����bTelnet: �Ƿ�����telnet�ͻ��������Լ�(��û�㶨����telnet�ͻ��˵�����)
//            wPort: telnet�����˿ں�
//ע    ��: Telnet��������������Ϊ���Դ�ӡ�Ĺ��ߣ���ACL���н���
//=============================================================================
ACL_API int aclTelnetInit( BOOL bTelnet, u16 wPort )
{
	TAclTel * tAclTel = NULL;
	int nErrCode = 0;
	TCmdEntry * ptCmdList = NULL;
	ptCmdList = (TCmdEntry *)g_szCmdList;
	
	if (NULL ==ptCmdList)
	{
		return ACL_ERROR_INIT;
	}
    if (0 == wPort)
    {
        ACL_DEBUG(E_MOD_TELNET, E_TYPE_WARNNING, "[aclTelnetInit] do not create telnet listen node\n");
        return ACL_ERROR_NOERROR;
    }
	//init telnet
    tAclTel = getIdleTelnet(wPort);
    if (NULL == tAclTel)
    {
        ACL_DEBUG(E_MOD_TELNET, E_TYPE_ERROR, "[aclTelnetInit] get idle Telnet failed\n");
        return ACL_ERROR_INIT;
    }
    nErrCode = aclCreateNode(getSockDataManger(), "0.0.0.0", wPort, newTelConnProc, tAclTel);
    ACL_DEBUG(E_MOD_TELNET, E_TYPE_NOTIF, "[aclTelnetInit]aclTelnetInit port %d\n",wPort);

	//inner command reg:
	aclRegCommand("help",(void *)checkCommandHelp,"show reg func help info");
    aclTelnetRegFunc();
	if (bTelnet)
	{
		//if telnet need to started delay and running local telnet
		//just windows is effected
#ifdef _MSC_VER
		char command[100];

		PROCESS_INFORMATION piProcInfo; 
		STARTUPINFO siStartInfo;	

		sprintf((char*)command, "telnet.exe localhost %d", wPort);

		memset(&siStartInfo, 0, sizeof(STARTUPINFO));
		memset(&piProcInfo, 0, sizeof(PROCESS_INFORMATION));

		siStartInfo.cb = sizeof(STARTUPINFO); 
		if( !CreateProcess(NULL, (LPWSTR)command, NULL, NULL, FALSE, 
			CREATE_NEW_CONSOLE, NULL, NULL, &siStartInfo, &piProcInfo) )
		{
            ACL_DEBUG(E_MOD_TELNET, E_TYPE_ERROR, "[aclTelnetInit]telnet.exe init failed: LE:%X\n",GetLastError());
            ACL_DEBUG(E_MOD_TELNET, E_TYPE_ERROR, "[aclTelnetInit] EXEC:%s\n",command);
            //telnet ��ʹʧ��Ҳ���ܷ��ش�����Ϊ��ǰtelnetģ���Ѿ���ʼ���ɹ���
			//return ACL_ERROR_FAILED;
		}

		hShellProc = piProcInfo.hProcess;
		hShellThread = piProcInfo.hThread;
#endif

	}
	return ACL_ERROR_NOERROR;
}


//telnet�ĸ����ڵ������ݴ����߳��˳�������ȫ���ã����TelnetExit
//ֻ��Ҫ����Telnet�����б������
//=============================================================================
//�� �� ����aclTelnetExit
//��	    �ܣ�
//�㷨ʵ�֣�
//ȫ�ֱ�����
//��	    ����
//ע    ��: �˺������aclInsertSelectLoopʹ�ã���Ϊtelnet�ļ����ڵ㣬�����µ�
//          telnet��������
//=============================================================================
ACL_API int aclTelnetExit()
{
#ifdef _MSC_VER
	unsigned long exitCode;

	if(hShellProc != NULL)
	{
		GetExitCodeProcess(hShellProc, &exitCode);
		if(exitCode == STILL_ACTIVE)
		{
			TerminateProcess(hShellProc, 0);
		}
		CloseHandle(hShellProc);
		hShellProc = NULL;
	}

	if(hShellThread != NULL)
	{
		CloseHandle(hShellThread);
		hShellThread = NULL;
	}
#endif

    memset(g_szAclTelList, 0, sizeof(g_szAclTelList));
    memset(g_szCmdList, 0, sizeof(g_szCmdList));
    g_telFd = INVALID_SOCKET;

	return ACL_ERROR_NOERROR;
}

/*
����: dbghelp
����: ��ӡ����ģ����Ϣ��ӡ�����ʹ�ӡ�������ӡ�ȿ�������
*/
ACL_API void aclTelnetDebugHelp()
{
    int i = 0;
    aclPrintf(TRUE, FALSE, "--------------------------------------------\n");
    aclPrintf(TRUE, FALSE, "EDEBUG_MODULE: setpm (0 - %d)\n",sizeof(p_EDEBUG_MODULE)/ sizeof(char *) - 1);
    for (i = 0; i < sizeof(p_EDEBUG_MODULE)/ sizeof(char *); i++)
    {
        aclPrintf(TRUE, FALSE, "\t%s \t %d\n", p_EDEBUG_MODULE[i], i);
    }
    aclPrintf(TRUE, FALSE, "\nDEBUG TYPE: setpt (0 - %d)\n", sizeof(p_EDEBUG_TYPE)/ sizeof(char *) - 1);
    for (i = 0; i < sizeof(p_EDEBUG_TYPE)/ sizeof(char *); i++)
    {
        aclPrintf(TRUE, FALSE, "\t%s \t %d\n", p_EDEBUG_TYPE[i], i);
    }

    aclPrintf(TRUE, FALSE, "\nCOMB_CMD: setpmt (0 - %d) (0 - %d)\n", 
        sizeof(p_EDEBUG_MODULE)/ sizeof(char *) - 1, sizeof(p_EDEBUG_TYPE)/ sizeof(char *) - 1);
    aclPrintf(TRUE, FALSE, "--------------------------------------------\n");
}


/*
����: setpm
����: ���ô�ӡģ����Ϣ
*/
ACL_API void aclTelnet_SetPm(int module)
{
    if (module < 0 || module >= sizeof(p_EDEBUG_MODULE)/ sizeof(char *))
    {
        aclPrintf(TRUE, FALSE, "invalid module code(0 - %d)\n", sizeof(p_EDEBUG_MODULE)/ sizeof(char *) - 1);
        return;
    }
    g_DebugMod = module;
    aclPrintf(TRUE,FALSE, "set debug module[%d] %s\n",module, p_EDEBUG_MODULE[module]);
}

/*
����: setpt
����: ���ô�ӡ������Ϣ
*/
ACL_API void aclTelnet_SetPt(int type)
{
    if (type < 0 || type >= sizeof(p_EDEBUG_TYPE)/ sizeof(char *))
    {
        aclPrintf(TRUE, FALSE, "invalid type code (0 - %d)\n", sizeof(p_EDEBUG_TYPE)/ sizeof(char *) - 1);
        return;
    }
    g_DebugType = type;
    aclPrintf(TRUE,FALSE, "set debug type[%d] %s\n",type, p_EDEBUG_TYPE[type]);
}

/*
����: setpmt
����: ͬʱ�������ô�ӡģ��ʹ�ӡ��Ϣ
*/
ACL_API void aclTelnet_SetPmt(int module, int type)
{
    if (type < 0 || type >= sizeof(p_EDEBUG_TYPE)/ sizeof(char *))
    {
        aclPrintf(TRUE, FALSE, "invalid type code (0 - %d)\n", sizeof(p_EDEBUG_TYPE)/ sizeof(char *) - 1);
        return;
    }
    if (module < 0 || module >= sizeof(p_EDEBUG_MODULE)/ sizeof(char *))
    {
        aclPrintf(TRUE, FALSE, "invalid module code(0 - %d)\n", sizeof(p_EDEBUG_MODULE)/ sizeof(char *) - 1);
        return;
    }
    g_DebugType = type;
    g_DebugMod = module;
    
    aclPrintf(TRUE,FALSE, "set debug module[%d] %s || debug type[%d] %s\n",
        module, p_EDEBUG_MODULE[module],type, p_EDEBUG_TYPE[type]);
}

/*
����: getps
����: get print set,��ӡ��ǰ���ֵ�������
*/
ACL_API void aclTelnet_GetPs()
{
    aclPrintf(TRUE, FALSE, "---------------------------------------\n");
    aclPrintf(TRUE, FALSE, "DEBUG MODULE:\t[%d] \t %s\n", g_DebugMod, p_EDEBUG_MODULE[g_DebugMod]);
    aclPrintf(TRUE, FALSE, "DEBUG TYPE:\t[%d] \t %s\n", g_DebugType, p_EDEBUG_TYPE[g_DebugType]);
    aclPrintf(TRUE, FALSE, "---------------------------------------\n");
}


/*
����: setwf
����: get print set,��ӡ��ǰ���ֵ�������
*/
ACL_API void aclTelWriteInFile(int nWrite)
{
    g_bDebugInfoWriteInFile = nWrite;
    if (nWrite)
    {
        aclPrintf(TRUE, FALSE, "write DebugInfo to LogFile:log.log\n");
    }
    else
    {
        aclPrintf(TRUE, FALSE, "Do not write DebugInfo\n");
    }
    
}

/*
Telnet ����ע�ắ��
*/
void aclTelnetRegFunc()
{
    aclRegCommand("dbghelp", (void *)aclTelnetDebugHelp, "debug filter set help");
    aclRegCommand("setpm",(void *)aclTelnet_SetPm, "set print module ->setpm [module]");
    aclRegCommand("setpt",(void *)aclTelnet_SetPt, "set print type ->setpt [type]");
    aclRegCommand("setpmt",(void *)aclTelnet_SetPmt, "set print module & type ->setpt [module type]");
    aclRegCommand("getps",(void *)aclTelnet_GetPs, "get print set ->getps");
    aclRegCommand("setwf",(void *)aclTelWriteInFile, "write DebugInfo to LogFile:log.log ->setwf [1/0]");
}