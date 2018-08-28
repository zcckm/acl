//******************************************************************************
//模块名	： acltype
//文件名	： acltype.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//acl通用类型定义
//------------------------------------------------------------------------------
//修改记录:
//2014-12-30 zcckm 创建
//******************************************************************************
#ifndef _ACL_TYPE_H_
#define _ACL_TYPE_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/*-----------------------------------------------------------------------------
系统公用文件，开发人员严禁修改
------------------------------------------------------------------------------*/

typedef int      s32,BOOL32;
typedef unsigned long   u32;
typedef unsigned char   u8;
typedef unsigned short  u16;
typedef short           s16;
typedef char            s8;

#ifdef _MSC_VER
typedef __int64         s64;
#else 
typedef long long       s64;
#endif 

#ifdef _MSC_VER
typedef unsigned __int64 u64;
#else 
typedef unsigned long long u64;
#endif

#ifndef _MSC_VER
#ifndef LPSTR
#define LPSTR   char *
#endif
#ifndef LPCSTR
#define LPCSTR  const char *
#endif
#endif
/*
#ifndef NULL
#define NULL (void *)0
#endif
*/

#ifdef WIN32

#else
#ifndef BOOL
#define BOOL int
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif

#ifndef INFINITE
#define INFINITE            (u32)-1  // Infinite timeout
#endif


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* _ACL_TYPE_H_ */

/* end of file acltype.h */

