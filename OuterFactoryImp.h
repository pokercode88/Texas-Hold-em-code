#ifndef _OUTER_FACTORY_IMP_H_
#define _OUTER_FACTORY_IMP_H_

#include <string>
#include <map>
#include "servant/Application.h"
#include "globe.h"
#include "OuterFactory.h"
#include "LogComm.h"

//wbl
#include <wbl/regex_util.h>

//配置服务
#include "DBAgentServant.h"
#include "OrderServant.h"
#include "HallServant.h"
#include "ConfigServant.h"
#include "Log2DBServant.h"

//
using namespace dataproxy;
using namespace dbagent;

//时区
#define ONE_DAY_TIME (24*60*60)
#define ZONE_TIME_OFFSET (8*60*60)

//
class OuterFactoryImp;
typedef TC_AutoPtr<OuterFactoryImp> OuterFactoryImpPtr;

/**
 * 外部工具接口对象工厂
 */
class OuterFactoryImp : public OuterFactory
{
private:
    /**
     *
    */
    OuterFactoryImp();

    /**
     *
    */
    ~OuterFactoryImp();

    /**
     *
     */
    friend class OrderServantImp;

    /**
     *
     */
    friend class OrderServer;

public:
    //框架中用到的outer接口(不能修改):
    const OuterProxyFactoryPtr &getProxyFactory() const
    {
        return _pProxyFactory;
    }

    tars::TC_Config &getConfig() const
    {
        return *_pFileConf;
    }

public:
    //读取所有配置
    void load();
    //代理配置
    void readPrxConfig();
    void printPrxConfig();
    //支付配置
    void readPayConfig();
    void printPayConfig();
    //获取商品配置
    void readProductConfig();
    void printProductConfig();

private:
    //
    void createAllObject();
    //
    void deleteAllObject();

public:
    //游戏配置服务代理
    const config::ConfigServantPrx getConfigServantPrx();
    //数据库代理服务代理
    const DBAgentServantPrx getDBAgentServantPrx(const long uid);
    //数据库代理服务代理
    const DBAgentServantPrx getDBAgentServantPrx(const string key);
    //广场服务代理
    const hall::HallServantPrx getHallServantPrx(const long uid);
    //广场服务代理
    const hall::HallServantPrx getHallServantPrx(const string key);
    //日志入库服务代理
    const DaqiGame::Log2DBServantPrx getLog2DBServantPrx(const long uid);

public:
    const bool isPayOpen()
    {
        return _bPayOpen;
    }

    const bool isSandBox()
    {
        return _bSandBox;
    }

    const string getIosVerifyUrl()
    {
        return _sIosVerifyUrl;
    }

    const string getAliVerifyUrl()
    {
        return _sAliVerifyUrl;
    }

public:
    const config::ListProductCfgResp getProductList()
    {
        return productList;
    }

public:
    void asyncLog2DB(const int64_t uid, const DaqiGame::TLog2DBReq &req);

    string getPorpsSmallIconByID(const int iPropsID);

    string getPorpsBigIconByID(const int iPropsID);

    string getPorpsNameByID(const int iPropsID);

private:
    //读写锁，防止脏读
    wbl::ReadWriteLocker m_rwlock;

private:
    //框架用到的共享对象(不能修改):
    tars::TC_Config *_pFileConf;
    //
    OuterProxyFactoryPtr _pProxyFactory;

private:
    //数据库代理服务
    std::string _DBAgentServantObj;
    DBAgentServantPrx _DBAgentServerPrx;

    std::string _HallServantObj;
    hall::HallServantPrx _HallServerPrx;

    //
    std::string _ConfigServantObj;
    config::ConfigServantPrx _ConfigServantPrx;

    //日志入库服务
    std::string _Log2DBServantObj;
    DaqiGame::Log2DBServantPrx _Log2DBServerPrx;

public:
    bool _bPayOpen; //是否开启支付
    bool _bSandBox; //是否开启沙盒
    string _sIosVerifyUrl; //Ios 验证订单
    string _sAliVerifyUrl; //ali 验证订单url

public:
    config::ListProductCfgResp productList; //商品列表

    config::ListPropsInfoCfgResp mapPropsConfig;

};

////////////////////////////////////////////////////////////////////////////////
#endif


