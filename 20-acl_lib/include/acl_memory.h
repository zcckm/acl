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
#ifndef _ACL_MEMORY_H_
#define _ACL_MEMORY_H_

// #ifdef __cplusplus
// extern "C" {
// #endif //extern "C"


//内存分配
void * aclMalloc(int nSize);

void * aclMallocClr(int nSize);

void aclFree(void * memory);

// #ifdef __cplusplus
// }
// #endif //extern "C"

#endif