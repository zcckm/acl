//******************************************************************************
//模块名	： acl_memory
//文件名	： acl_memory.h
//作者	： zcckm
//版本	： 1.0
//文件功能说明:
//ACL内存管理
//------------------------------------------------------------------------------
//修改记录:
//2015-01-19 zcckm 创建
//******************************************************************************
#include "acl_memory.h"
#include <malloc.h>
#include <memory.h>
#include "acl_common.h"
void * aclMalloc(int nSize)
{
    return malloc(nSize);
}

void * aclMallocClr(int nSize)
{
    void * pAlloc = malloc(nSize);
    memset(pAlloc,0,nSize);
    return pAlloc;
}

void aclFree(void * memory)
{
    if (memory)
    {
        free(memory);
        memory = NULL;
    }
}

#ifdef  _MSC_VER

#endif