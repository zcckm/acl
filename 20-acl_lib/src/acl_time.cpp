//******************************************************************************
//模块名	： acl_time
//文件名	： acl_time.c
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL 时间相关
//------------------------------------------------------------------------------
//修改记录:
//2015-01-23 zcckm 创建
//******************************************************************************

#include "acl_time.h"
#include "acl_telnet.h"
#include "acl_lock.h"
#include "acl_manage.h"
#include "acl_memory.h"
#include "acl_msgqueue.h"
#include "acl_task.h"
//timer global 
static TACL_THREAD g_tTimerManagerThread;
static TACL_THREAD g_tTimerClockThread;
static PTMSG_QUEUE g_ptTimerMsgQue = NULL;


static ETASK_STATUS g_TmMng = E_TASK_IDLE;
static ETASK_STATUS g_TmClk = E_TASK_IDLE;
static BOOL g_bIsTimerInited = FALSE;
#ifdef WIN32
static H_ACL_SEM g_hTimerSem = NULL;
#elif defined (_LINUX_)
static H_ACL_SEM g_hTimerSem;
#endif


//TIMER
#define  MAX_TIMER_SUPT 1000

typedef struct  
{
    u32 m_dwAppInsAddr;       // dst app instance
    u16 m_wMsgType;          // message type for pushed
    int m_nLeftTime;          //ms unit <= 0 means time is up
    int m_nSetBaseTime;       //ms unit, set count time
    BOOL32 m_bIsRepead;       // is repead Notify
    BOOL32 m_bIsEnabled;      //is enabled
    PF_TIMER_NTF m_pf_CbNtf;  //registered fun for notification
    void * m_pExtInfo;        //ext Ntf info
    u16 m_wExtInfoLen;        //ext info len
}T_ACL_TIMER;

#ifdef WIN32
ACL_API int getcurrenttime(LARGE_INTEGER * ulValue);
#elif defined(_LINUX_)
ACL_API void aclUDelay(u32 dwUsecs);
#endif

//TIMER


typedef struct
{
    PF_TIMER_NTF pfTimerNtf;
    void * pExtParam;
}T_TIMER_CB;

#ifdef WIN32

ACL_API int getcurrenttime(LARGE_INTEGER * ulValue)
{
    QueryPerformanceCounter(ulValue);
    return 0;
}
#endif

#ifdef _LINUX_
ACL_API void aclUDelay(u32 dwUsecs)
{
	/*
    struct timeval delay;  
    delay.tv_sec = 0;  
    delay.tv_usec = dwUsecs;
    select(0, NULL, NULL, NULL, &delay);  
	*/
	usleep(dwUsecs);
}
#endif

ACL_API void aclDelay(u32 dwMsecs)
{
#ifdef _MSC_VER
	Sleep(dwMsecs);
#endif

#ifdef _VXWORKS_
	taskDelay( msToTick(dwMsecs) );
#endif

#ifdef _LINUX_
/*
        struct timeval delay;  
        delay.tv_sec = 0;  
        delay.tv_usec = dwMsecs * 1000;
        select(0, NULL, NULL, NULL, &delay);  
		*/
		usleep(dwMsecs * 1000);
#endif

}

//=============================================================================
//函 数 名：cbTimerRegCBProc
//功	    能：定时器注册回调函数处理线程
//注	    意：
//算法实现：
//全局变量：
//参	  数：
//        ptMsg:定时器到时间了，消息处理
//        hInst:对应处理的Instance号

//注    意:
//=============================================================================
void cbTimerRegCBProc(TAclMessage *ptMsg, HACLINST hInst)
{
    T_TIMER_CB * ptTimerCB = (T_TIMER_CB *)ptMsg->m_pContent;
    (*ptTimerCB->pfTimerNtf)(ptTimerCB->pExtParam);
}



#define TIMER_HANDLE_TIMEVAL 1//定时器轮循处理间隔
//定时器管理线程，核心计时线程

//=============================================================================
//函 数 名：aclTimerManage
//功	    能：定时器管理线程，核心计时线程
//注	    意：
//算法实现：
//全局变量：
//参	  数： pParam: 没用到

//注    意:此函数是定时器核心定时线程，用于更新各注册定时器的触发剩余时间
//         时间到了后，定时器会将消息推送给指定的AI，如果定时器注册了回调响应,
//         则会推送给专门处理定时器回调的APP
//         定时器计时循环为 1ms，因为精度原因，定时器定时最好要 > 50ms
//=============================================================================
#ifdef WIN32
unsigned int __stdcall  aclTimerManage(void * pParam)
#elif defined (_LINUX_)
void * aclTimerManage(void * pParam)
#endif
{
    PTQUEUE_MEMBER ptHandleMember = NULL;
    T_ACL_TIMER * ptAclTimer = NULL;
    TQUEUE_MEMBER tCurePoint;
    int nRet = 0;

    //创建信号量， 初始化信号量值为1，然后立刻获得信号，
    //利用trywait超时作为定时器，而且可以使用信号控制线程退出
    if (ACL_ERROR_NOERROR != aclCreateSem(&g_hTimerSem, 1))
    {
        ACL_DEBUG(E_MOD_TIMER, E_TYPE_ERROR, "[aclTimerManage] init failed\n");
#ifdef WIN32
        return ACL_ERROR_FAILED;
#elif defined (_LINUX_)
        return (void *)ACL_ERROR_FAILED;
#endif
        
    }
    //此APP当前只处理注册回调函数的定时器的响应，以保证核心计时线程的准确度
    nRet = aclCreateApp__(TIMER_CB_APP_NUM, "timer Callback", TIMER_CB_INST_NUM, 0, cbTimerRegCBProc);
    if (ACL_ERROR_NOERROR != nRet)
    {
        ACL_DEBUG(E_MOD_TIMER, E_TYPE_ERROR, "[aclTimerManage] create timer proc APP failed\n");
#ifdef WIN32
        return ACL_ERROR_FAILED;
#elif defined (_LINUX_)
        return (void *)ACL_ERROR_FAILED;
#endif
    }

    memset(&tCurePoint,0,sizeof(tCurePoint));
    ACL_DEBUG(E_MOD_TIMER, E_TYPE_NOTIF, "[aclTimerManage] aclTimerManage is running\n");
    while(E_TASK_RUNNING == g_TmMng)
    {
        aclCheckGetSem_b(&g_hTimerSem, INFINITE);
        //get base signal
        {
            lockLock(g_ptTimerMsgQue->hQueLock);
            if (NULL == g_ptTimerMsgQue->LACP)
            {
                unlockLock(g_ptTimerMsgQue->hQueLock);
                continue;
            }
            ptHandleMember = g_ptTimerMsgQue->LACP;
            
            do 
            {
                tCurePoint.ptNext = ptHandleMember->ptNext;
                
                ptAclTimer = (T_ACL_TIMER *)ptHandleMember->pContent;
				if (!ptAclTimer)
				{
					ACL_DEBUG(E_MOD_TIMER, E_TYPE_ERROR, "get Timer Failed\n");
					continue;
				}
//                aclPrintf(TRUE,FALSE,"[timer Manage] aclTimerManage %d %d %d %d\n",g_ptTimerMsgQue->m_nCurQueMembNum,count++,ptAclTimer->m_nLeftTime,ptAclTimer->m_dwMsgType);
                if(ptAclTimer->m_nLeftTime > 0)//not at time still wait
                {
                    ptAclTimer->m_nLeftTime -= TIMER_HANDLE_TIMEVAL;
                }

                if (ptAclTimer->m_nLeftTime <= 0)// time is up
                {
                    if (NULL != ptAclTimer->m_pf_CbNtf)// if cb fun existed call it and ignore post msg
                    {
                        if (ptAclTimer->m_bIsEnabled)
                        {
                            T_TIMER_CB tTimerCB;
                            tTimerCB.pExtParam = ptAclTimer->m_pExtInfo;
                            tTimerCB.pfTimerNtf = ptAclTimer->m_pf_CbNtf;
                            aclMsgPush(0,MAKEID(TIMER_CB_APP_NUM,INST_RANDOM), 0, ptAclTimer->m_wMsgType, (void *)&tTimerCB, sizeof(T_TIMER_CB), E_PMT_L_L);
                        }
                    }
                    else// push timer message to instance
                    {
                        if (ptAclTimer->m_bIsEnabled)
                            aclMsgPush(0,ptAclTimer->m_dwAppInsAddr, 0, ptAclTimer->m_wMsgType, ptAclTimer->m_pExtInfo, ptAclTimer->m_wExtInfoLen, E_PMT_L_L);
                    }
                    if (ptAclTimer->m_bIsRepead && ptAclTimer->m_bIsEnabled)// timer already ntf but need repead
                    {
                        ptAclTimer->m_nLeftTime = ptAclTimer->m_nSetBaseTime;
                    }
                    else
                    {
                        //三个方面的内存释放问题
                        //1.ptHandleMember 表示 malloc的 链表节点 PTQUEUE_MEMBER，这里需要释放
                        //2.ptHandleMember->pContent 表示 T_ACL_TIMER 无论深浅拷贝都需要释放
                        //3.ptAclTimer->m_pExtInfo 表示定时器附带信息，应该移交给 浅拷贝模式的 Instance
                        
                        //改为直接deletAclDLList利用函数，其中多重锁问题，在单个线程是没关系的，可以正常返回
                        //其他线程如果调用则需要release两次,Linux也适用
                        deletAclDLList(g_ptTimerMsgQue, ptHandleMember);

                    }
                }
                ptHandleMember = tCurePoint.ptNext;

            } while (NULL != ptHandleMember);

            unlockLock(g_ptTimerMsgQue->hQueLock);
         }
    }

    //del rest timer in manage list
    lockLock(g_ptTimerMsgQue->hQueLock);
    if (NULL == g_ptTimerMsgQue->LACP)
    {
        unlockLock(g_ptTimerMsgQue->hQueLock);
        g_TmMng = E_TASK_ALREADY_EXIT;
#ifdef WIN32
        return ACL_ERROR_NOERROR;
#elif defined (_LINUX_)
        return (void *)ACL_ERROR_NOERROR;
#endif
    }
    ptHandleMember = g_ptTimerMsgQue->LACP;
    do 
    {
        tCurePoint.ptNext = ptHandleMember->ptNext;
        ptAclTimer = (T_ACL_TIMER *)ptHandleMember->pContent;
        deletAclDLList(g_ptTimerMsgQue, ptHandleMember);
        ptHandleMember = tCurePoint.ptNext;
    } while (NULL != ptHandleMember);



    unlockLock(g_ptTimerMsgQue->hQueLock);

    g_TmMng = E_TASK_ALREADY_EXIT;
#ifdef WIN32
    return ACL_ERROR_NOERROR;
#elif defined (_LINUX_)
    return (void *)ACL_ERROR_NOERROR;
#endif
}

ACL_API u32 aclGetTickCount()
{
#ifdef WIN32
    return GetTickCount();
#elif defined (_LINUX_)
    struct timeval tv_now = {0};
    gettimeofday (&tv_now , NULL);
    return tv_now.tv_sec * 1000 + (tv_now.tv_usec + 500)/1000;
#endif
}


//定时器时钟线程，任务就是在配置的时间内准时释放信号量，作为基准时钟，
//让定时器管理线程准时工作。
#ifdef WIN32
unsigned int __stdcall  aclTimerClock(void * pParam)
#elif defined (_LINUX_)
void * aclTimerClock(void * pParam)
#endif
{
#ifdef _LINUX_
    struct timeval tv_old = {0},tv_now = {0};
#endif
    while(E_TASK_RUNNING == g_TmClk)
    {
#ifdef WIN32
        aclDelay(TIMER_HANDLE_TIMEVAL);
#endif
#ifdef _LINUX_

// 方案1
//linux系统延迟的误差很大，利用循环获取时间的方法进行验证，且加上最后一次
//调用gettimeofday的时间以及误差等因素，考虑使用945ms
        if (0 == tv_old.tv_sec)//null
        {
            gettimeofday (&tv_old , NULL);
        }
        gettimeofday (&tv_now , NULL);
        if(((tv_now.tv_sec - tv_old.tv_sec)*1000000 + tv_now.tv_usec - tv_old.tv_usec) >= 945)
        {
            //time is up
            tv_old = tv_now;
        }
        else
        {
            aclUDelay(10);
            continue;
        }
		
//方案2
//		aclUDelay(1000);

#endif
        aclReleaseSem(&g_hTimerSem);
    }
    g_TmClk = E_TASK_ALREADY_EXIT;
#ifdef WIN32
    return ACL_ERROR_NOERROR;
#elif defined (_LINUX_)
    return (void *)ACL_ERROR_NOERROR;
#endif
}

ACL_API void initTimer()
{
    TTHREAD_PARAM tThreadParam;
    ACL_THREAD_ATTR tthreadAttr;
    memset(&tthreadAttr,0,sizeof(tthreadAttr));

    if (g_bIsTimerInited)
    {
        //already inited
        return;
    }
    if (NULL == g_ptTimerMsgQue)
    {
        g_ptTimerMsgQue = createAclDLList(MAX_TIMER_SUPT,ACL_QUE_DEEP_COPYMODE);
    }

    g_TmMng = E_TASK_RUNNING;
    g_TmClk = E_TASK_RUNNING;
    tThreadParam.aclThreadAttr = tthreadAttr;
    tThreadParam.dwStackSize = 0;
    tThreadParam.nThreadRun = 1;
    // init acl timer thread
    aclCreateThread_b(&g_tTimerManagerThread,
        &tThreadParam,
        aclTimerManage,
        NULL);
    aclCreateThread_b(&g_tTimerClockThread,
        &tThreadParam,
        aclTimerClock,
        NULL);
    ACL_DEBUG(E_MOD_TIMER, E_TYPE_NOTIF, "[initTimer]aclTimer init\n");

    g_bIsTimerInited = TRUE;
}


ACL_API int unInitTimer()
{
    //必须先退出定时器管理线程，然后再退出时钟线程
    g_TmMng = E_TASK_ASK_FOR_EXIT;
    WAIT_FOR_EXIT(g_TmMng);
    g_TmMng = E_TASK_IDLE;

    aclDestoryThread(g_tTimerManagerThread.hThread);

    g_TmClk = E_TASK_ASK_FOR_EXIT;
    WAIT_FOR_EXIT(g_TmClk);
    g_TmClk = E_TASK_IDLE;

    aclDestoryThread(g_tTimerClockThread.hThread);

    destroyAclDLList(g_ptTimerMsgQue);
    g_ptTimerMsgQue = NULL;
    aclDestorySem(&g_hTimerSem);

    aclDestroyApp(TIMER_CB_APP_NUM);

    g_bIsTimerInited = FALSE;
    return ACL_ERROR_NOERROR;
}



//default timer is not repead if want repeat ,set again
ACL_API ACL_HANDLE setTimer__(int nTime, u32 wAppInsAddr, u16 wMsgType)
{
    return setTimer_b__(nTime, wAppInsAddr, wMsgType, FALSE,NULL,NULL,0);
}

ACL_API ACL_HANDLE setTimer_f(int nTime, u32 wAppInsAddr, u16 wMsgType)
{
    return setTimer_b__(nTime, wAppInsAddr, wMsgType, FALSE,NULL,NULL,0);
}

//设置定时器,基础函数
//nTime 设置超时时间(单位为毫秒)
//dwDstMailAddr 设置目的邮件地址 APP+Instance
//dwMsgType 消息类型 必须在用户区间(后期会进行限制，当前定时器消息与用于消息先写在一起)
//bRepeat 是否重复
//定时器g_ptTimerMsgQue为链表，使用浅拷贝模式，因此insert T_ACL_TIMER需要malloc
ACL_API ACL_HANDLE setTimer_b__(int nTime,
    u32 dwAppInsAddr,
    u16 wMsgType,
    BOOL32 bRepeat,
    PF_TIMER_NTF pfTimerNtf,
    void * pContent,
    int nContentLen)
{
    T_ACL_TIMER  tAclTimer;
    memset(&tAclTimer, 0, sizeof(T_ACL_TIMER));
    tAclTimer.m_bIsRepead = bRepeat;
    tAclTimer.m_dwAppInsAddr = dwAppInsAddr;
    tAclTimer.m_wMsgType = wMsgType;
    tAclTimer.m_pf_CbNtf = pfTimerNtf;
    tAclTimer.m_nSetBaseTime = nTime;
    tAclTimer.m_nLeftTime = nTime;
    tAclTimer.m_bIsEnabled = TRUE;
    

    CHECK_NULL_RET_NULL(g_ptTimerMsgQue)
    if (NULL != pContent)
    {
        tAclTimer.m_pExtInfo = aclMallocClr(nContentLen);
        tAclTimer.m_wExtInfoLen = nContentLen;
        memcpy(tAclTimer.m_pExtInfo, pContent, nContentLen);
    }

    return (ACL_HANDLE)insertAclDLList(g_ptTimerMsgQue,&tAclTimer,sizeof(T_ACL_TIMER));
}


// kill timer based on MsgNumber
// 两个参数作为搜索条件，如果为零表示缺省
void killTimer_bm(u32 dwAppInsAddr,u32 dwMsgType)
{
    
}

//killtimer using addr and msgtype, before according handle is dangerous
//timer will destory when finish work,then using handle kill time will course crashed
ACL_API int killTimer(u16 wMsgType)
{
    T_ACL_TIMER * ptAclTimer = NULL;
    PTQUEUE_MEMBER ptFirst = NULL,ptEnd = NULL;
    lockLock(g_ptTimerMsgQue->hQueLock);
    ptFirst = g_ptTimerMsgQue->LACP;
    ptEnd = g_ptTimerMsgQue->LAEP;
    if (NULL == ptFirst || NULL == ptEnd)// no node in list
    {
        unlockLock(g_ptTimerMsgQue->hQueLock);
        return ACL_ERROR_NOERROR;
    }

    for (;ptFirst != ptEnd; ptFirst = ptFirst->ptNext)
    {
        ptAclTimer = (T_ACL_TIMER *)ptFirst->pContent;
        if (wMsgType == ptAclTimer->m_wMsgType)
        {
            ptAclTimer->m_bIsEnabled = FALSE;
            ptAclTimer->m_nLeftTime = 0;
            ptAclTimer->m_bIsRepead = FALSE;
        }
    }

    ptAclTimer = (T_ACL_TIMER *)ptEnd->pContent;
    if (wMsgType == ptAclTimer->m_wMsgType)
    {
        ptAclTimer->m_bIsEnabled = FALSE;
        ptAclTimer->m_nLeftTime = 0;
        ptAclTimer->m_bIsRepead = FALSE;
    }

    unlockLock(g_ptTimerMsgQue->hQueLock);



    return ACL_ERROR_NOERROR;
}