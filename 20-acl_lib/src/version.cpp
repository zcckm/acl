//******************************************************************************
//ģ����  �� version
//�ļ���  �� version.c
//����    �� zhoucc
//�汾    �� 1.0
//�ļ�����˵��:
//�ն˰汾�Ź���
//------------------------------------------------------------------------------
//�޸ļ�¼:
//2018-07-20 zhoucc ����
//******************************************************************************
#include "version.h"
#define VER_MAIN 0  //�����汾
#define VER_SUB1 5  //��������޸ģ��Ż�
#define VER_SUB2 12 //Bug����

#define STR(s)     #s
#define VERSION(a,b,c)  "ACL V:" STR(a) "." STR(b) "." STR(c) " " __DATE__
//#define VERSTR  "ACL V: V2.0.1.2017.01.01"
const char * getAclVersion()
{
	return VERSION(VER_MAIN, VER_SUB1, VER_SUB2);
}