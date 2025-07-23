#include <sstream>
#include "OuterFactoryImp.h"
#include "LogComm.h"
#include "OrderServer.h"
#include "util/tc_hash_fun.h"

using namespace wbl;

/**
 *
*/
OuterFactoryImp::OuterFactoryImp() : _pFileConf(NULL)
{
    createAllObject();
}

/**
 *
*/
OuterFactoryImp::~OuterFactoryImp()
{
    deleteAllObject();
}

void OuterFactoryImp::deleteAllObject()
{
    if (_pFileConf)
    {
        delete _pFileConf;
        _pFileConf = NULL;
    }
}

void OuterFactoryImp::createAllObject()
{
    try
    {
        //
        deleteAllObject();

        //本地配置文件
        _pFileConf = new tars::TC_Config();
        if (!_pFileConf)
        {
            ROLLLOG_ERROR << "create config parser fail, ptr null." << endl;
            terminate();
        }

        //tars代理Factory,访问其他tars接口时使用
        _pProxyFactory = new OuterProxyFactory();
        if ((long int)NULL == _pProxyFactory)
        {
            ROLLLOG_ERROR << "create outer proxy factory fail, ptr null." << endl;
            terminate();
        }

        FDLOG_RECHARGE_LOG_FORMAT;
        LOG_DEBUG << "init proxy factory succ." << endl;

        //读取所有配置
        load();
    }
    catch (TC_Exception &ex)
    {
        LOG->error() << ex.what() << endl;
    }
    catch (exception &e)
    {
        LOG->error() << e.what() << endl;
    }
    catch (...)
    {
        LOG->error() << "unknown exception." << endl;
    }

    return;
}

//读取所有配置
void OuterFactoryImp::load()
{
    __TRY__

    //拉取远程配置
    g_app.addConfig(ServerConfig::ServerName + ".conf");

    wbl::WriteLocker lock(m_rwlock);

    _pFileConf->parseFile(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");
    LOG_DEBUG << "init config file succ:" << ServerConfig::BasePath + ServerConfig::ServerName + ".conf" << endl;

    //代理配置
    readPrxConfig();
    printPrxConfig();

    //支付配置
    readPayConfig();
    printPayConfig();

    //
    readProductConfig();
    printProductConfig();


    if(getConfigServantPrx()->listProps(mapPropsConfig) != 0)
    {
        LOG_ERROR << "read props err."<< endl;
        return;
    }

    __CATCH__
}

//代理配置
void OuterFactoryImp::readPrxConfig()
{
    //配置服务
    _DBAgentServantObj = (*_pFileConf).get("/Main/Interface/DBAgentServer<ProxyObj>", "");
    _HallServantObj = (*_pFileConf).get("/Main/Interface/HallServer<ProxyObj>", "");
    _Log2DBServantObj = (*_pFileConf).get("/Main/Interface/Log2DBServer<ProxyObj>", "");
    _ConfigServantObj = (*_pFileConf).get("/Main/Interface/ConfigServer<ProxyObj>", "");

}

//打印代理配置
void OuterFactoryImp::printPrxConfig()
{
    FDLOG_CONFIG_INFO << "_DBAgentServantObj ProxyObj:" << _DBAgentServantObj << endl;
    FDLOG_CONFIG_INFO << "_HallServantObj ProxyObj:" << _HallServantObj << endl;
    FDLOG_CONFIG_INFO << "_ConfigServantObj ProxyObj:" << _ConfigServantObj << endl;
    FDLOG_CONFIG_INFO << "_Log2DBServantObj ProxyObj : " << _Log2DBServantObj << endl;
}

//代理配置
void OuterFactoryImp::readPayConfig()
{
    _bPayOpen = S2I((*_pFileConf).get("/Main/Pay<open>", "1")) == 1;
    _bSandBox = S2I((*_pFileConf).get("/Main/Pay<sandBox>", "1")) == 1;
    _sIosVerifyUrl = (*_pFileConf).get("/Main/Pay/Ios<verifyUrl>", "");
    _sAliVerifyUrl = (*_pFileConf).get("/Main/Pay/Ali<verifyUrl>", "");
}

//打印代理配置
void OuterFactoryImp::printPayConfig()
{
    FDLOG_CONFIG_INFO << "_bPayOpen:" << _bPayOpen << ", _bSandBox: "<< _bSandBox << ", _sIosVerifyUrl:"<< _sIosVerifyUrl << endl;
}

void OuterFactoryImp::readProductConfig()
{
    auto pConfigServantPrx = getConfigServantPrx();
    if(!pConfigServantPrx)
    {
        LOG_ERROR << "pConfigServantPrx is nullptr "<<endl;
        return ;
    }

    config::ListProductCfgResp resp;
    if(pConfigServantPrx->listProduct(resp) != 0)
    {
        LOG_ERROR << "read product err."<< endl;
        return;
    }

    productList.data.clear();
    for(auto info : resp.data)
    {
        productList.data.insert(std::make_pair(info.first, info.second));
    }
    return;
}

void OuterFactoryImp::printProductConfig()
{
    FDLOG_CONFIG_INFO << "Product size: " << productList.data.size() << endl;
}

const config::ConfigServantPrx OuterFactoryImp::getConfigServantPrx()
{
    if (!_ConfigServantPrx)
    {
        _ConfigServantPrx = Application::getCommunicator()->stringToProxy<config::ConfigServantPrx>(_ConfigServantObj);
        LOG_DEBUG << "Init _ConfigServantObj succ, _ConfigServantObj: " << _ConfigServantObj << endl;
    }

    return _ConfigServantPrx;
}

//数据库代理服务代理
const DBAgentServantPrx OuterFactoryImp::getDBAgentServantPrx(const long uid)
{
    if (!_DBAgentServerPrx)
    {
        _DBAgentServerPrx = Application::getCommunicator()->stringToProxy<dbagent::DBAgentServantPrx>(_DBAgentServantObj);
        ROLLLOG_DEBUG << "Init _DBAgentServantObj succ, _DBAgentServantObj:" << _DBAgentServantObj << endl;
    }

    if (_DBAgentServerPrx)
    {
        return _DBAgentServerPrx->tars_hash(uid);
    }

    return NULL;
}

//数据库代理服务代理
const DBAgentServantPrx OuterFactoryImp::getDBAgentServantPrx(const string key)
{
    if (!_DBAgentServerPrx)
    {
        _DBAgentServerPrx = Application::getCommunicator()->stringToProxy<dbagent::DBAgentServantPrx>(_DBAgentServantObj);
        ROLLLOG_DEBUG << "Init _DBAgentServantObj succ, _DBAgentServantObj:" << _DBAgentServantObj << endl;
    }

    if (_DBAgentServerPrx)
    {
        return _DBAgentServerPrx->tars_hash(tars::hash<string>()(key));
    }

    return NULL;
}

//
const hall::HallServantPrx OuterFactoryImp::getHallServantPrx(const long uid)
{
    if (!_HallServerPrx)
    {
        _HallServerPrx = Application::getCommunicator()->stringToProxy<hall::HallServantPrx>(_HallServantObj);
        ROLLLOG_DEBUG << "Init _HallServantObj succ, _HallServantObj:" << _HallServantObj << endl;
    }

    if (_HallServerPrx)
    {
        return _HallServerPrx->tars_hash(uid);
    }

    return NULL;
}

//
const hall::HallServantPrx OuterFactoryImp::getHallServantPrx(const string key)
{
    if (!_HallServerPrx)
    {
        _HallServerPrx = Application::getCommunicator()->stringToProxy<hall::HallServantPrx>(_HallServantObj);
        ROLLLOG_DEBUG << "Init _HallServantObj succ, _HallServantObj:" << _HallServantObj << endl;
    }

    if (_HallServerPrx)
    {
        return _HallServerPrx->tars_hash(tars::hash<string>()(key));
    }

    return NULL;
}

//日志入库服务代理
const DaqiGame::Log2DBServantPrx OuterFactoryImp::getLog2DBServantPrx(const long uid)
{
    if (!_Log2DBServerPrx)
    {
        _Log2DBServerPrx = Application::getCommunicator()->stringToProxy<DaqiGame::Log2DBServantPrx>(_Log2DBServantObj);
        ROLLLOG_DEBUG << "Init _Log2DBServantObj succ, _Log2DBServantObj : " << _Log2DBServantObj << endl;
    }

    if (_Log2DBServerPrx)
    {
        return _Log2DBServerPrx->tars_hash(uid);
    }

    return NULL;
}

//日志入库
void OuterFactoryImp::asyncLog2DB(const int64_t uid, const DaqiGame::TLog2DBReq &req)
{
    getLog2DBServantPrx(uid)->async_log2db(NULL, req);
}

//
string OuterFactoryImp::getPorpsSmallIconByID(const int iPropsID)
{
    auto it = mapPropsConfig.data.find(iPropsID);
    if(it == mapPropsConfig.data.end())
    {
        return "";
    }
    return it->second.propsIconSmall;
}

string OuterFactoryImp::getPorpsBigIconByID(const int iPropsID)
{
    auto it = mapPropsConfig.data.find(iPropsID);
    if(it == mapPropsConfig.data.end())
    {
        return "";
    }
    return it->second.propsIconBig;
}

string OuterFactoryImp::getPorpsNameByID(const int iPropsID)
{
    auto it = mapPropsConfig.data.find(iPropsID);
    if(it == mapPropsConfig.data.end())
    {
        return "";
    }
    return it->second.propsName;
}
////////////////////////////////////////////////////////////////////////////////


