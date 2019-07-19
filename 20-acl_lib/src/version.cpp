//******************************************************************************
//模块名  ： version
//文件名  ： version.c
//作者    ： zhoucc
//版本    ： 1.0
//文件功能说明:
//终端版本号管理
//------------------------------------------------------------------------------
//修改记录:
//2018-07-20 zhoucc 创建
//******************************************************************************
#include "version.h"
#define VER_MAIN 0 //发布版本
#define VER_SUB1 13 //功能添加修改，优化
#define VER_SUB2 20 //Bug修正


#define STR(s)     #s
#define VERSION(a,b,c)  "ACL V" STR(a) "." STR(b) "." STR(c) " " __DATE__
//#define VERSTR  "ACL V: V2.0.1.2017.01.01"
const char * getAclVersion()
{
	return VERSION(VER_MAIN, VER_SUB1, VER_SUB2);
}

//获取版本号信息
TAclVersionInfo getAclVersionTag()
{
	TAclVersionInfo tVersion;
	memset(&tVersion, 0, sizeof(tVersion));
	tVersion.m_nVerMain = VER_MAIN;
	tVersion.m_nVerSub1 = VER_SUB1;
	tVersion.m_nVerSub2 = VER_SUB2;
	strcpy(tVersion.m_szInfo, VERSION(VER_MAIN, VER_SUB1, VER_SUB2));
	return tVersion;
}