#ifndef _TEST_ERROR_H_
#define _TEST_ERROR_H_

#define TERR_DB_SN_NOT_EXISTS       10001 // 标签上的SN号码不在数据库中
#define TERR_DB_IMEI_NOT_MATCH      10002 // 数据库和标签上的IMEI不匹配
#define TERR_MD_READ_SN             10003 // 从模块读取SN失败
#define TERR_MD_READ_IMEI           10004 // 从模块读取IMEI失败
#define TERR_MD_READ_CCID           10006 //
#define TERR_MD_SN_NOT_MATCH        10101 // 模块内和标签上的SN不匹配
#define TERR_MD_IMEI_NOT_MATCH      10102 // 模块内和标签上的IMEI不匹配
#define TERR_MD_CCID_NOT_MATCH      10103 //
#define TERR_MD_WITHOUT_CALIBRATE   10201 // 模块没有进行校准综测
#define TERR_MD_TEST_FUNCTION       10202 // 模块功能测试失败

#endif // _TEST_ERROR_H_
