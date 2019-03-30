//******************************************************************************
//模块名	： acl_telnet
//文件名	： acl_telnet.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL telnet 调试
//------------------------------------------------------------------------------
//修改记录:
//2015-01-20 zcckm 创建
//******************************************************************************

#include "acl_telnet.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "acl_task.h"
#include "acl_manage.h"
#include "acl_socket.h"
#include "version.h"
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
    "E_MOD_LOCK",//锁
    "E_MOD_NETWORK",//网络收发相关，偏向于Socket
    "E_MOD_HB",//心跳
    "E_MOD_MSG",//message,消息收发逻辑相关
    "E_MOD_MANAGE",//管理
    "E_MOD_TELNET",//telnet
    "E_MOD_TIMER",//定时器
    "E_MOD_ALL" //所有模块
};

int g_DebugType = E_TYPE_KEY;
int g_DebugMod = E_MOD_ALL;

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

	//大于等于E_MOD_ALL的消息，则全部放开打印
	//小于E_MOD_ALL的消息，则按模块打印

	//小于等于调试类型控制值得消息全部打印
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
                fpLog = fopen("log.log","a");
            }
            fprintf(fpLog,"%s",s);
        }
    }
    return ACL_ERROR_NOERROR;
}


//-->函数注册相关
enum
{
	ARGV_STRING = 1,
	ARGV_INTEGER = 2,
};

//拆分各个参数设置于argv指针数组中
//返回参数个数+1(argv[0]是函数名)
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
	if ('\0' != *buf)
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

//解析输入的命令是否在命令列表中
//并带参执行注册的函数
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
//函 数 名：aclRegCommand
//功	    能：调试函数注册
//算法实现：
//全局变量：
//参	    数：pCmd: 注册调用函数的命令
//         pFunc: 注册函数指针
//       pPrompt: 命令功能提示
//注    意:注册函数支持不超过10个参数，注册后通过Telnet客户端调用 注意调用时候，
//         输入参数需要和注册参数匹配，否则可能引起崩溃
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
//函 数 名：newTelMsgProcess
//功	    能：telnet数据处理函数
//算法实现：
//全局变量：
//参	    数：nFd： 监听的socket
//       eEvent: 监听事件
//     pContext: 附加参数
//注    意: 此函数配合aclInsertSelectLoop使用，此时telnet已经连接上，这里处理
//          所有telnet发来的数据
//=============================================================================
s32 newTelMsgProcess(H_ACL_SOCKET nFd, ESELECT eEvent, void* pContext)
{
	s8 szRcvData[MAX_RECV_PACKET_SIZE] = {0};
	TAclTel * ptAclTel = (TAclTel *)pContext;
	int nRcvSize = 0, i = 0;
	u32 dwParseValue = 0;
	int nRealNum = 0;
	CHECK_NULL_RET_INVALID(ptAclTel);
// 	if (ESELECT_READ != eEvent)
// 	{
//         ACL_DEBUG(E_MOD_TELNET, E_TYPE_WARNNING, "[newTelMsgProcess] new message but not read???\n");
// 		return ACL_ERROR_INVALID;
// 	}

	nRcvSize = aclTcpRecv(nFd, szRcvData, MAX_RECV_PACKET_SIZE);
	if (0 >= nRcvSize)//attempt to disconnect current connect
	{
        ACL_DEBUG(E_MOD_TELNET, E_TYPE_WARNNING, "[newTelMsgProcess] telnet is disconnected or error Happen, Recv Ret: [%d]\n", nRcvSize);
		printf("remove socket %d\n", nFd);
		aclRemoveSelectLoop(getSock3AManger(), nFd,true,false);
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
			aclRemoveSelectLoop(getSock3AManger() ,nFd,true,false);
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
//函 数 名：newTelConnProc
//功	    能：新的telnet连接处理函数
//算法实现：
//全局变量：
//参	    数：nFd： 监听的socket
//       eEvent: 监听事件
//     pContext: 附加参数
//注    意: 此函数配合aclInsertSelectLoop使用，作为telnet的监听节点，处理新的
//          telnet连接请求
//=============================================================================
s32 newTelConnProc(H_ACL_SOCKET nFd, ESELECT eEvent, void* pContext)
{
	H_ACL_SOCKET hConnSock = INVALID_SOCKET;
	TAclTel * ptAclTel = (TAclTel *)pContext;
	char szMarkBuf[RS_SKHNDBUF_LEN] = {0};
	u32 dwConnIP = 0;
	u16 wConnPort = 0;
	int nSendDataLen = 0;
    ACL_DEBUG(E_MOD_TELNET, E_TYPE_DEBUG,"[newTelConnProc] new telnet connectX\n");
// 	if (ESELECT_READ != eEvent)
// 	{
// 		return ACL_ERROR_INVALID;
// 	}
	//new connect 
	hConnSock = aclTcpAccept(nFd, &dwConnIP, &wConnPort);

	//可以设置成支持单进程或者多进程telnet客户端，当前支持单进程


	if (INVALID_SOCKET == hConnSock)//connect is failed
	{
        ACL_DEBUG(E_MOD_TELNET, E_TYPE_ERROR,"[newTelConnProc] telnet connect is failed\n");
		return ACL_ERROR_FAILED;
	}
	//后期可以通过扩展参数扩展telnet验证要求
	//nInnerNodeID 仍旧设置为-1 因为telnet无法通过心跳验证，因此需要将其当作监听节点处理
	/* 设置TELE属性，打印欢迎语句*/

	
//	SendIAC(hConnSock, TELCMD_DO, TELOPT_ECHO);
//	SendIAC(hConnSock, TELCMD_DO, TELOPT_NAWS);
//	SendIAC(hConnSock, TELCMD_DO, TELOPT_LFLOW);
//	SendIAC(hConnSock, TELCMD_WILL, TELOPT_ECHO);
	SendIAC(hConnSock, TELCMD_WILL, TELOPT_SGA);
	if (INVALID_SOCKET != g_telFd)//already connected
	{
        //disconnect old telnet connect
		aclRemoveSelectLoop(getSock3AManger(), g_telFd,true,false);
        //aclCloseSocket(g_telFd);
		
	}
	g_telFd = hConnSock;

	aclECHO(hConnSock, "*===============================================================*\r\n");
	aclECHO(hConnSock, "***====================   ACL Telnet Server   ================***\r\n");
	aclECHO(hConnSock, "*===============================================================*\r\n");
	aclECHO(hConnSock, ptAclTel->m_szPrompt);

	TNodeInfo tNodeInfo;
	memset(&tNodeInfo, 0, sizeof(tNodeInfo));
	tNodeInfo.m_dwNodeSSID = 0;
	tNodeInfo.m_eNodeType = E_NT_LISTEN;
    aclInsertSelectLoopUnsafe(getSock3AManger(), hConnSock, newTelMsgProcess, ESELECT_CONN, tNodeInfo, pContext);
	//	getTelnetPrompt(wConnPort);
	ACL_DEBUG(E_MOD_TELNET, E_TYPE_DEBUG, "[newTelConnProc]  new telnet connected and Port %d\n",wConnPort);
	aclTcpSend(hConnSock, szMarkBuf, nSendDataLen);

	return ACL_ERROR_NOERROR;
}

#ifdef _MSC_VER
int aclCreateProcess(char * pCommand)
{
	TCHAR szCommandLine[1024] = { 0 };
	int iLength;

	iLength = MultiByteToWideChar(CP_ACP, 0, pCommand, strlen(pCommand) + 1, NULL, 0);
	MultiByteToWideChar(CP_ACP, 0, pCommand, strlen(pCommand) + 1, szCommandLine, iLength);

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	si.dwFlags = STARTF_USESHOWWINDOW;  // 指定wShowWindow成员有效
	si.wShowWindow = TRUE;          // 此成员设为TRUE的话则显示新建进程的主窗口，
									// 为FALSE的话则不显示
	BOOL bRet = ::CreateProcess(
		NULL,           // 不在此指定可执行文件的文件名
		szCommandLine,      // 命令行参数
		NULL,           // 默认进程安全性
		NULL,           // 默认线程安全性
		FALSE,          // 指定当前进程内的句柄不可以被子进程继承
		CREATE_NEW_CONSOLE, // 为新进程创建一个新的控制台窗口
		NULL,           // 使用本进程的环境变量
		NULL,           // 使用本进程的驱动器和目录
		&si,
		&pi);
	return ACL_ERROR_NOERROR;
}
#endif
//=============================================================================
//函 数 名：aclTelnetInit
//功	    能：
//算法实现：
//全局变量：
//参	    数：bTelnet: 是否启动telnet客户端连接自己(还没搞定启动telnet客户端的问题)
//            wPort: telnet监听端口号
//注    意: Telnet服务器在这里作为调试打印的工具，与ACL进行交互
//=============================================================================
ACL_API int aclTelnetInit(BOOL bTelnet, u16 wPort, const char * pListenIP)
{
	TAclTel * tAclTel = NULL;
	int nErrCode = 0;
	TCmdEntry * ptCmdList = NULL;
	ptCmdList = (TCmdEntry *)g_szCmdList;
	
	if (NULL ==ptCmdList)
	{
		return ACL_ERROR_INIT;
	}
	if (!pListenIP)
	{
		return ACL_ERROR_PARAM;
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
    nErrCode = aclCreateNode(getSock3AManger(), pListenIP, wPort, newTelConnProc, tAclTel);
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

		sprintf((char*)command, "telnet.exe 127.0.0.1 %d", wPort);
		memset(&siStartInfo, 0, sizeof(STARTUPINFO));
		memset(&piProcInfo, 0, sizeof(PROCESS_INFORMATION));
		aclCreateProcess(command);
#endif

	}
	return ACL_ERROR_NOERROR;
}


//telnet的各个节点在数据处理线程退出后则被完全重置，因此TelnetExit
//只需要重置Telnet对象列表就行了
//=============================================================================
//函 数 名：aclTelnetExit
//功	    能：
//算法实现：
//全局变量：
//参	    数：
//注    意: 此函数配合aclInsertSelectLoop使用，作为telnet的监听节点，处理新的
//          telnet连接请求
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
命令: dbghelp
功能: 打印所有模块信息打印，类型打印，级别打印等开启方法
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
命令: setpm
功能: 设置打印模块信息
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
命令: setpt
功能: 设置打印类型信息
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
命令: setpmt
功能: 同时设置设置打印模块和打印信息
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
命令: getps
功能: get print set,打印当前各种调试设置
*/
ACL_API void aclTelnet_GetPs()
{
    aclPrintf(TRUE, FALSE, "---------------------------------------\n");
    aclPrintf(TRUE, FALSE, "DEBUG MODULE:\t[%d] \t %s\n", g_DebugMod, p_EDEBUG_MODULE[g_DebugMod]);
    aclPrintf(TRUE, FALSE, "DEBUG TYPE:\t[%d] \t %s\n", g_DebugType, p_EDEBUG_TYPE[g_DebugType]);
    aclPrintf(TRUE, FALSE, "---------------------------------------\n");
}


/*
命令: setwf
功能: get print set,打印当前各种调试设置
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
命令: aclver
功能: 获得ACL版本号
*/
ACL_API void aclShowVersion()
{
	aclPrintf(true, false, "%s\n", getAclVersion());
}

/*
Telnet 命令注册函数
*/
void aclTelnetRegFunc()
{
    aclRegCommand("dbghelp", (void *)aclTelnetDebugHelp, "debug filter set help");
    aclRegCommand("setpm",(void *)aclTelnet_SetPm, "set print module ->setpm [module]");
    aclRegCommand("setpt",(void *)aclTelnet_SetPt, "set print type ->setpt [type]");
    aclRegCommand("setpmt",(void *)aclTelnet_SetPmt, "set print module & type ->setpt [module type]");
    aclRegCommand("getps",(void *)aclTelnet_GetPs, "get print set ->getps");
    aclRegCommand("setwf",(void *)aclTelWriteInFile, "write DebugInfo to LogFile:log.log ->setwf [1/0]");
	aclRegCommand("aclver", (void *)aclShowVersion, "get ACL version");
}