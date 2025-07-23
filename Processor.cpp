#include <algorithm>
#include <openssl/pem.h>
#include "Processor.h"
#include "globe.h"
#include "LogComm.h"
#include "DataProxyProto.h"
#include "ServiceDefine.h"
#include "util/tc_hash_fun.h"
#include "uuid.h"
#include "util/tc_md5.h"
#include "util/tc_base64.h"
#include "OrderServer.h"

//
using namespace std;
using namespace dataproxy;
using namespace dbagent;

#define SPLIT_LEN 117

static std::map<string, dbagent::Eum_Col_Type> tbOrderList = {
        { "uid", dbagent::BIGINT },
        { "in_order_id", dbagent::STRING },
        { "out_order_id", dbagent::STRING },
        { "product_id", dbagent::STRING },
        { "product_count", dbagent::INT },
        { "pay_type", dbagent::INT },
        { "exchange_index", dbagent::INT },
        { "order_state", dbagent::INT },
        { "pay_time", dbagent::STRING },
        { "create_time", dbagent::STRING }
};

/**
 * 
*/
Processor::Processor()
{
}

/**
 * 
*/
Processor::~Processor()
{
}

void Processor::init()
{

}

//
int Processor::createOrder(const map<string, string>& mapOrderInfo)
{
    auto it = mapOrderInfo.find("in_order_id");
    if(it == mapOrderInfo.end())
    {
        LOG_ERROR << "in_order_id noe exist."<< endl;
        return -1;
    }

    dataproxy::TWriteDataReq wdataReq;
    wdataReq.resetDefautlt();
    wdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(ORDER_LIST) + ":" + it->second;
    wdataReq.operateType = E_REDIS_INSERT;
    wdataReq.paraExt.resetDefautlt();
    wdataReq.paraExt.queryType = dbagent::E_UPDATE;
    wdataReq.clusterInfo.resetDefautlt();
    wdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    wdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    wdataReq.clusterInfo.frageFactor = tars::hash<string>()(it->second);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    for(auto item : tbOrderList)
    {
        auto it = mapOrderInfo.find(item.first);
        if(it == mapOrderInfo.end())
        {
            continue;
        }
        tfield.colName = item.first;
        tfield.colType = item.second;
        tfield.colValue = it->second;
        fields.push_back(tfield);

    }
    wdataReq.fields = fields;

    TWriteDataRsp wdataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(it->second)->redisWrite(wdataReq, wdataRsp);
    if (iRet != 0 || wdataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "redisWrite failed, iRet: " << iRet << ", wdataRsp.iResult: " << wdataRsp.iResult << endl;
        return -2;
    }
    return 0;
}

//
int Processor::selectOrder(const string& inOrderId, vector<map<string, string>>&vecRecord)
{
    map<string, string> recode;

    dataproxy::TReadDataReq rdataReq;
    rdataReq.resetDefautlt();
    rdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(ORDER_LIST) + ":" + inOrderId;
    rdataReq.operateType = E_REDIS_READ;
    rdataReq.paraExt.resetDefautlt();
    rdataReq.paraExt.queryType = dbagent::E_UPDATE;
    rdataReq.clusterInfo.resetDefautlt();
    rdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    rdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    rdataReq.clusterInfo.frageFactor = tars::hash<string>()(inOrderId);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    for(auto item : tbOrderList)
    {
        tfield.colName = item.first;
        tfield.colType = item.second;
        fields.push_back(tfield);

    }
    rdataReq.fields = fields;

    TReadDataRsp rdataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(inOrderId)->redisRead(rdataReq, rdataRsp);
    if (iRet != 0 || rdataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "redisRead failed, iRet: " << iRet << ", wdataRsp.iResult: " << rdataRsp.iResult << endl;
        return -2;
    }

    for (auto it = rdataRsp.fields.begin(); it != rdataRsp.fields.end(); ++it)
    {
        map<string, string> mapRecord;
        for (auto itTField = it->begin(); itTField != it->end(); ++itTField)
        {
            mapRecord.insert(std::make_pair(itTField->colName, itTField->colValue));
        }
        vecRecord.push_back(mapRecord);
    }
    return 0;
}

int Processor::selectOrderByPayType(const long lPlayerID, const OrderProto::E_PAY_TYPE ePayType, vector<map<string, string>>&vecRecord)
{
    dbagent::TDBReadReq rDataReq;
    rDataReq.keyIndex = 0;
    rDataReq.queryType = dbagent::E_SELECT;
    rDataReq.tableName = "tb_order_list";

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    for(auto item : tbOrderList)
    {
        tfield.colName = item.first;
        tfield.colType = item.second;
        fields.push_back(tfield);

    }
    rDataReq.fields = fields;

    vector<dbagent::ConditionGroup> conditionGroups;
    dbagent::ConditionGroup conditionGroup;
    conditionGroup.relation = dbagent::AND;
    vector<dbagent::Condition> conditions;

    dbagent::Condition condition;
    condition.condtion = dbagent::E_EQ;
    condition.colType = dbagent::BIGINT;
    condition.colName = "uid";
    condition.colValues = I2S(lPlayerID);
    conditions.push_back(condition);

    condition.condtion = ePayType == OrderProto::E_PAY_TYPE::E_PAY_EXCHANGE ? dbagent::E_EQ : dbagent::E_NE;
    condition.colType = dbagent::STRING;
    condition.colName = "pay_type";
    condition.colValues = I2S(int(OrderProto::E_PAY_TYPE::E_PAY_EXCHANGE));
    conditions.push_back(condition);

    conditionGroup.condition = conditions;
    conditionGroups.push_back(conditionGroup);
    rDataReq.conditions = conditionGroups;

    vector<dbagent::OrderBy> orderBys;
    dbagent::OrderBy orderBy;
    orderBy.sort = dbagent::DESC;
    orderBy.colName = "create_time";
    orderBys.push_back(orderBy);
    rDataReq.orderbyCol = orderBys;

    dbagent::TDBReadRsp dataRsp;
    int iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(0)->read(rDataReq, dataRsp);
    if (iRet != 0 || dataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "read data from dbagent failed, rDataReq:" << printTars(rDataReq) << ",dataRsp: " << printTars(dataRsp) << endl;
        return -1;
    }

    for (auto it = dataRsp.records.begin(); it != dataRsp.records.end(); ++it)
    {
        map<string, string> mapRecord;
        for (auto itfield = it->begin(); itfield != it->end(); ++itfield)
        {
            mapRecord.insert(std::make_pair(itfield->colName, itfield->colValue));
        }
        vecRecord.push_back(mapRecord);
    }

    return 0;
}

//
int Processor::updateOrder(const string& inOrderId, const map<string, string>& mapOrderInfo)
{
    int iRet = 0;

    ROLLLOG_DEBUG<< "inOrderId: "<< inOrderId << endl;

    dataproxy::TWriteDataReq wdataReq;
    wdataReq.resetDefautlt();
    wdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(ORDER_LIST) + ":" + inOrderId;
    wdataReq.operateType = E_REDIS_WRITE;
    wdataReq.paraExt.resetDefautlt();
    wdataReq.paraExt.queryType = dbagent::E_UPDATE;
    wdataReq.clusterInfo.resetDefautlt();
    wdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    wdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    wdataReq.clusterInfo.frageFactor = tars::hash<string>()(inOrderId);;

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    for(auto item : tbOrderList)
    {
        auto it = mapOrderInfo.find(item.first);
        if(it == mapOrderInfo.end())
        {
            continue;
        }
        tfield.colName = item.first;
        tfield.colType = item.second;
        tfield.colValue = it->second;
        fields.push_back(tfield);

    }
    wdataReq.fields = fields;

    TWriteDataRsp wdataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(inOrderId)->redisWrite(wdataReq, wdataRsp);
    if (iRet != 0 || wdataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "redisWrite failed, iRet: " << iRet << ", wdataRsp.iResult: " << wdataRsp.iResult << endl;
        return iRet;
    }
    return 0;
}

int Processor::updateOrderStats(const string& productId, const int count)
{
    int iRet = 0;

    auto productList = g_app.getOuterFactoryPtr()->getProductList();
    auto it = productList.data.find(productId);
    if(it == productList.data.end())
    {
        LOG_ERROR << "product not exist. productId:"<< productId << endl;
        return -1;
    }

    map<string, string> mapRecord;
    iRet = selectOrderStats(productId, mapRecord);
    if(iRet != 0)
    {
        LOG_DEBUG << "select order stats err. productId:"<< productId << endl;
        return iRet;
    }

    int buyCount = count == 0 ? 0 : S2I(mapRecord["buy_count"]) + count;
    int hot = count == 0 ? S2I(mapRecord["hot"]) - 10 : S2I(mapRecord["hot"]) + 5;
    hot = hot < it->second.hot ? it->second.hot : hot;

    dataproxy::TWriteDataReq wdataReq;
    wdataReq.resetDefautlt();
    wdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(ORDER_STATS) + ":" + productId;
    wdataReq.operateType = E_REDIS_WRITE;
    wdataReq.paraExt.resetDefautlt();
    wdataReq.paraExt.queryType = dbagent::E_REPLACE;
    wdataReq.clusterInfo.resetDefautlt();
    wdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    wdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    wdataReq.clusterInfo.frageFactor = tars::hash<string>()(productId);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "product_id";
    tfield.colType = dbagent::STRING;
    tfield.colValue = productId;
    fields.push_back(tfield);

    tfield.colName = "buy_count";
    tfield.colType = dbagent::INT;
    tfield.colValue = I2S(buyCount);
    fields.push_back(tfield);

    tfield.colName = "hot";
    tfield.colType = dbagent::INT;
    tfield.colValue = I2S(hot);
    fields.push_back(tfield);

    wdataReq.fields = fields;
    TWriteDataRsp wdataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(productId)->redisWrite(wdataReq, wdataRsp);
    if (iRet != 0 || wdataRsp.iResult != 0)
    {
        ROLLLOG_ERROR << "redisWrite failed, iRet: " << iRet << ", wdataRsp.iResult: " << wdataRsp.iResult << endl;
        return -1;
    }
    return iRet;
}

int Processor::selectOrderStats(const string& productId, map<string, string>& mapRecord)
{
    int iRet = 0;
    dataproxy::TReadDataReq rdataReq;
    rdataReq.resetDefautlt();
    rdataReq.keyName = I2S(E_REDIS_TYPE_HASH) + ":" + I2S(ORDER_STATS) + ":" + productId;
    rdataReq.operateType = E_REDIS_READ;
    rdataReq.paraExt.resetDefautlt();
    rdataReq.paraExt.queryType = dbagent::E_SELECT;
    rdataReq.clusterInfo.resetDefautlt();
    rdataReq.clusterInfo.busiType = E_REDIS_PROPERTY;
    rdataReq.clusterInfo.frageFactorType = E_FRAGE_FACTOR_STRING;
    rdataReq.clusterInfo.frageFactor = tars::hash<string>()(productId);

    vector<TField> fields;
    TField tfield;
    tfield.colArithType = E_NONE;
    tfield.colName = "product_id";
    tfield.colType = dbagent::STRING;
    fields.push_back(tfield);

    tfield.colName = "buy_count";
    tfield.colType = dbagent::INT;
    fields.push_back(tfield);

    tfield.colName = "hot";
    tfield.colType = dbagent::INT;
    fields.push_back(tfield);

    rdataReq.fields = fields;

    TReadDataRsp rdataRsp;
    iRet = g_app.getOuterFactoryPtr()->getDBAgentServantPrx(productId)->redisRead(rdataReq, rdataRsp);
    if (iRet != 0 || rdataRsp.iResult != 0 || rdataRsp.fields.size() > 1)
    {
        ROLLLOG_ERROR << "redisWrite failed, iRet: " << iRet << ", rdataRsp.iResult: " << rdataRsp.iResult << endl;
        return iRet;
    }

    for (auto it = rdataRsp.fields.begin(); it != rdataRsp.fields.end(); ++it)
    {
        for (auto itTField = it->begin(); itTField != it->end(); ++itTField)
        {
            mapRecord.insert(std::make_pair(itTField->colName, itTField->colValue));
        }
    }
    return 0;
}

int Processor::isHot(const string& productId)
{
    auto productList = g_app.getOuterFactoryPtr()->getProductList();
    auto it = productList.data.find(productId);
    if(it == productList.data.end())
    {
        LOG_ERROR << "product not exist. productId:"<< productId << endl;
        return 0;
    }

    map<string, string> mapRecord;
    int iRet = selectOrderStats(productId, mapRecord);
    if(iRet != 0)
    {
        LOG_DEBUG << "select order stats err. productId:"<< productId << endl;
        return 0;
    }

    int curHot = mapRecord.size() == 0 ? it->second.hot : S2I(mapRecord["hot"]);

    LOG_DEBUG << "hotLimit:"<< it->second.hotLimit << ", curHot:"<< curHot << ", productId:"<< productId<< ", mapRecord size:"<< mapRecord.size() << endl;
    return curHot >= it->second.hotLimit ? 1 : 0;
}
/***************************支付接口*****************************/
