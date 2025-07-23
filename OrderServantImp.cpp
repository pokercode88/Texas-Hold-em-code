#include "OrderServantImp.h"
#include "servant/Application.h"
#include "uuid.h"
#include "DataProxyProto.h"
#include "ServiceUtil.h"
#include "LogComm.h"
#include "globe.h"
#include "OrderServer.h"
#include "util/tc_hash_fun.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "HallServant.h"
#include <jwt-cpp/jwt.h>
#include <iostream>
#include <chrono>
#include "LogDefine.h"
#include "Processor.h"
#include "util/tc_md5.h"

#include "CommonCode.pb.h"

#include "XGameHttp.pb.h"
#include "utils/HttpClient.h"

#include "pay/AliPay.h"
#include "pay/WePay.h"
#include "pay/IosPay.h"

//
using namespace std;

//
using namespace dataproxy;
using namespace rapidjson;

#define PAGE_COUNT 10

//////////////////////////////////////////////////////
void OrderServantImp::initialize()
{
    //initialize servant here:
    //...

    IosPaySingleton::getInstance()->init(g_app.getOuterFactoryPtr()->getIosVerifyUrl());

    AliPaySingleton::getInstance()->init(g_app.getOuterFactoryPtr()->getAliVerifyUrl());

    WePaySingleton::getInstance()->init(g_app.getOuterFactoryPtr()->getAliVerifyUrl());
}

//////////////////////////////////////////////////////
void OrderServantImp::destroy()
{
    //destroy servant here:
    //...
}


//http请求处理接口 curl -X POST -H "Content-Type: application/json" -d '{"name": "Jason", "email": "jason@example.com"}' http://10.10.10.143:22222/order
tars::Int32 OrderServantImp::doRequest(const vector<tars::Char> &reqBuf, const map<std::string, std::string> &extraInfo,
                                       vector<tars::Char> &rspBuf, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");

    int iRet = 0;

    __TRY__

    string sReqData(reqBuf.begin(), reqBuf.end());
    ROLLLOG_DEBUG << "reqBuf: " << sReqData <<", url: "<< extraInfo.at("VisitUrl") << endl;

    if(extraInfo.at("VisitUrl").find("/alipay") != string::npos)
    {
        LOG_DEBUG << "alipay callback notify."<< endl;
    }
    else if(extraInfo.at("VisitUrl").find("/wecpay") != string::npos)
    {
        LOG_DEBUG << "wepay callback notify."<< endl;
    }
    else
    {
        ROLLLOG_ERROR << "url err. url:"<< extraInfo.at("VisitUrl") << endl;
    }

    string sPostData = "{\"code\": 200, \"msg\":\"success\"}";

    rspBuf.assign(sPostData.c_str(), sPostData.c_str() + sPostData.size());

    __CATCH__;
    FUNC_EXIT("", iRet);
    return iRet;
}

//tcp请求处理接口
tars::Int32 OrderServantImp::onRequest(tars::Int64 lUin, const std::string &sMsgPack, const std::string &sCurServrantAddr, const JFGame::TClientParam &stClientParam, const JFGame::UserBaseInfoExt &stUserBaseInfo, tars::TarsCurrentPtr current)
{
    int iRet = 0;

    try
    {
        ROLLLOG_DEBUG << "recv msg, uid : " << lUin << ", addr : " << stClientParam.sAddr << endl;

        OrderServantImp::async_response_onRequest(current, 0);

        XGameComm::TPackage pkg;
        pbToObj(sMsgPack, pkg);
        if (pkg.vecmsghead_size() == 0)
        {
            ROLLLOG_DEBUG << "package empty." << endl;
            return -1;
        }

        ROLLLOG_DEBUG << "recv msg, uid : " << lUin << ", msg : " << logPb(pkg) << endl;

        for (int i = 0; i < pkg.vecmsghead_size(); ++i)
        {
            int64_t ms1 = TNOWMS;

            auto &head = pkg.vecmsghead(i);
            switch (head.nmsgid())
            {
            case XGameProto::ActionName::ORDRE_PRODUCT_LIST:  //// 商品查询
            {
                OrderProto::OrderProductListReq orderProductListReq;
                pbToObj(pkg.vecmsgdata(i), orderProductListReq);
                iRet = onQueryProductList(pkg, sCurServrantAddr, orderProductListReq, current);
                break;
            }
            case XGameProto::ActionName::ORDRE_CREATE_ORDER:  //// 创建商品订单
            {
                OrderProto::OrderCreateReq orderCreateReq;
                pbToObj(pkg.vecmsgdata(i), orderCreateReq);
                iRet = onCreateOrder(pkg, sCurServrantAddr, orderCreateReq, current);
                break;
            }
            case XGameProto::ActionName::ORDRE_VERIFY_ORDER:  //// 验证商品订单
            {
                OrderProto::OrderVerifyReq orderVerifyReq;
                pbToObj(pkg.vecmsgdata(i), orderVerifyReq);
                iRet = onVerifyOrder(pkg, sCurServrantAddr, orderVerifyReq, current);
                break;
            }
            case XGameProto::ActionName::ORDER_LIST_QUERY:  //// 订单明细查询
            {
                OrderProto::OrderListQueryReq orderListQueryReq;
                pbToObj(pkg.vecmsgdata(i), orderListQueryReq);
                iRet = onQueryOrderList(pkg, sCurServrantAddr, orderListQueryReq, current);
                break;
            }
            default:
            {
                ROLLLOG_ERROR << "invalid msg id, uid: " << lUin << ", msg id: " << head.nmsgid() << endl;
                break;
            }
            }

            if (iRet != 0)
            {
                ROLLLOG_ERROR << "msg process fail, uid: " << lUin << ", msg id: " << head.nmsgid() << ", iRet: " << iRet << endl;
            }

            int64_t ms2 = TNOWMS;
            if ((ms2 - ms1) > COST_MS)
            {
                ROLLLOG_WARN << "@Performance, msgid:" << head.nmsgid() << ", costTime:" << (ms2 - ms1) << endl;
            }
        }
    }
    catch (const std::exception &e)
    {
        ROLLLOG_ERROR << e.what() << endl;
        iRet = -1;
    }

    return iRet;
}


tars::Int32 OrderServantImp::manOrder(tars::Int64 lUin, tars::Int64 iPropsID, tars::Int64 iPropsCount, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");

    int iRet = 0;

    auto pServantPrx = g_app.getOuterFactoryPtr()->getHallServantPrx(lUin);
    if(!pServantPrx)
    {
        LOG_ERROR << "pServantPrx is nullptr." << endl;
        return -1;
    }

    vector<map<string, string>> vecRecord;
    string inOrderId = L2S(TNOW) + L2S(lUin) + I2S(iPropsID);
    iRet = ProcessorSingleton::getInstance()->selectOrder(inOrderId, vecRecord);
    if(iRet != 0 || vecRecord.size() > 0)
    {
        LOG_ERROR << "inOrderId exist. inOrderId:"<< inOrderId << endl;
        return XGameRetCode::ORDER_FREQUENT_OPERATION;
    }

    //生成订单
    std::map<string, string> mapOrderInfo = {
        { "uid", L2S(lUin) },
        { "in_order_id", inOrderId },
        { "out_order_id", "" },
        { "product_id", I2S(iPropsID) },
        { "product_count", I2S(iPropsCount) },
        { "pay_type", I2S(int(OrderProto::E_PAY_TYPE::E_PAY_MAN)) },
        { "exchange_index", "0" },
        { "order_state",  "1"},
        { "pay_time",  ServiceUtil::CurTimeFormat() },
        { "create_time", ServiceUtil::CurTimeFormat() }
    };
    iRet = ProcessorSingleton::getInstance()->createOrder(mapOrderInfo);
    if(iRet != 0)
    {
        LOG_ERROR << "create order err. iRet:"<< iRet << endl;
        return iRet;
    }

    userinfo::ModifyUserPropsReq req;
    userinfo::ModifyUserPropsInfo info;
    info.uid = lUin;
    info.changeType = XGameProto::GOLDFLOW_ID_ORDER_MAN;
    info.propsInfo.propsID = iPropsID;
    info.propsInfo.iState = 0;
    info.propsCount = iPropsCount;
    info.paraExt = inOrderId;
    req.updateInfo.push_back(info);

    pServantPrx->async_modifyUserProps(NULL, req);

    FUNC_EXIT("", iRet);

    return iRet;

}

int OrderServantImp::onQueryProductList(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, const OrderProto::OrderProductListReq &req, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");

    int iRet = 0;

    OrderProto::OrderProductListResp resp;
    try
    {
        auto productList = g_app.getOuterFactoryPtr()->getProductList();
        for(auto item : productList.data)
        {
            if(item.second.productType != int(req.eproducttype()))
            {
                continue;
            }
            getProductInfo(item.second.productId, resp.add_vproductinfos());
        }
    }
    catch (const std::exception &e)
    {
        iRet = -1;
        ROLLLOG_ERROR << e.what() << endl;
    }

    resp.set_iresultid(iRet);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::ORDRE_PRODUCT_LIST, XGameComm::MSGTYPE_RESPONSE, resp);

    FUNC_EXIT("", iRet);

    return iRet;
}

int OrderServantImp::onCreateOrder(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, const OrderProto::OrderCreateReq &req, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");

    OrderProto::OrderCreateResp resp;
    int iRet = doCreateOrder(pkg.stuid().luid(), req, resp);
    if(iRet != 0)
    {
        LOG_ERROR << "create order err. iRet:"<< iRet << ", req:"<< logPb(req)<< endl;
    }

    resp.set_iresultid(iRet);
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::ORDRE_CREATE_ORDER, XGameComm::MSGTYPE_RESPONSE, resp);

    FUNC_EXIT("", iRet);

    return iRet;
}

int OrderServantImp::onVerifyOrder(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, const OrderProto::OrderVerifyReq &req, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");

    OrderProto::OrderVerifyResp resp;
    int iRet = doVerifyOrder(pkg.stuid().luid(), req, resp);
    if(iRet != 0)
    {
        LOG_ERROR << "verify order err. iRet:"<< iRet << ", req:"<< logPb(req)<< endl;
    }

    resp.set_iresultid(iRet);
    resp.mutable_morderinfo()->set_soutorderid(req.morderinfo().soutorderid());
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::ORDRE_VERIFY_ORDER, XGameComm::MSGTYPE_RESPONSE, resp);

    FUNC_EXIT("", iRet);

    return iRet;
}

int OrderServantImp::onQueryOrderList(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, const OrderProto::OrderListQueryReq &req, tars::TarsCurrentPtr current)
{
    FUNC_ENTRY("");

    vector<map<string, string>> vecRecord;
    int iRet = ProcessorSingleton::getInstance()->selectOrderByPayType(pkg.stuid().luid(), req.epaytype(), vecRecord);
    if(iRet != 0)
    {
        LOG_ERROR << "update order stats err. iRet:"<< iRet << endl;
        return iRet;
    }
    OrderProto::OrderListQueryResp resp;

    unsigned int startIndex = (req.ipage() - 1) * PAGE_COUNT;
    for(unsigned int index = startIndex; index < vecRecord.size(); index++)
    {
        if(resp.vecorderinfo_size() >= PAGE_COUNT)
        {
            break;
        }

        auto info = resp.add_vecorderinfo();
        info->set_sinorderid(vecRecord[index]["in_order_id"]);
        info->set_soutorderid(vecRecord[index]["out_order_id"]);
        info->set_epaytype(OrderProto::E_PAY_TYPE(S2I(vecRecord[index]["pay_type"])));
        info->set_iexchangeindex(S2I(vecRecord[index]["exchange_index"]));
        info->set_iorderstate(S2I(vecRecord[index]["order_state"]));
        info->set_lordertime(ServiceUtil::GetTimeStamp(vecRecord[index]["create_time"]));
        info->set_iproductcount(S2I(vecRecord[index]["product_count"]));
        getProductInfo(vecRecord[index]["product_id"], info->mutable_mproductinfo());
    }

    resp.set_iresultid(iRet);
    resp.set_ipage(req.ipage());
    resp.set_itotalpage(vecRecord.size() / PAGE_COUNT + (vecRecord.size() % PAGE_COUNT != 0 ? 1 : 0));
    toClientPb(pkg, sCurServrantAddr, XGameProto::ActionName::ORDER_LIST_QUERY, XGameComm::MSGTYPE_RESPONSE, resp);

    FUNC_EXIT("", iRet);

    return iRet;
}

int OrderServantImp::doCreateOrder(const long lPlayerID, const OrderProto::OrderCreateReq &req, OrderProto::OrderCreateResp &resp)
{
    FUNC_ENTRY("");
    int iRet = 0;

    if(!g_app.getOuterFactoryPtr()->isPayOpen())
    {
        return XGameRetCode::ORDER_SERVER_CLOSE;
    }

    LOG_ERROR << "req"<< logPb(req)<< endl;

    auto productList = g_app.getOuterFactoryPtr()->getProductList();
    auto it = productList.data.find(req.sproductid());
    if(it == productList.data.end())
    {
        LOG_ERROR << "product not exist. productId:"<< req.sproductid() << endl;
        return XGameRetCode::ORDER_PRODUCT_NOT_EXIST;
    }

    vector<map<string, string>> vecRecord;
    string inOrderId = ServiceUtil::generateUUIDStr();
    iRet = ProcessorSingleton::getInstance()->selectOrder(inOrderId, vecRecord);
    if(iRet != 0 || vecRecord.size() > 0)
    {
        LOG_ERROR << "inOrderId exist. inOrderId:"<< inOrderId << endl;
        return XGameRetCode::ORDER_FREQUENT_OPERATION;
    }

    //生成订单
    std::map<string, string> mapOrderInfo = {
        { "uid", L2S(lPlayerID) },
        { "in_order_id", inOrderId },
        { "out_order_id", "" },
        { "product_id", req.sproductid() },
        { "product_count", I2S(req.iproductcount() <= 0 ? 1 : req.iproductcount()) },
        { "pay_type", I2S(int(req.epaytype())) },
        { "exchange_index", I2S(req.iexchangeindex()) },
        { "order_state",  "0"},
        { "pay_time",  "0000-00-00 00:00:00" },
        { "create_time", ServiceUtil::CurTimeFormat() }
    };
    iRet = ProcessorSingleton::getInstance()->createOrder(mapOrderInfo);
    if(iRet != 0)
    {
        LOG_ERROR << "create order err. iRet:"<< iRet << endl;
        return iRet;
    }

    switch(req.epaytype())
    {
        case OrderProto::E_PAY_TYPE::E_PAY_ALI:
        {
            //TODO ALI PAY
            break;
        }
        case OrderProto::E_PAY_TYPE::E_PAY_WECHAT:
        {
            //TODO WECHAT PAY
            break;
        }
        default:
        {
            break;
        }
    }

    //消费商品
    if(g_app.getOuterFactoryPtr()->isSandBox() /*|| req.epaytype() == OrderProto::E_PAY_TYPE::E_PAY_ALI || req.epaytype() == OrderProto::E_PAY_TYPE::E_PAY_WECHAT8*/)
    {
        iRet = cashPruduct(lPlayerID, inOrderId, "");
        if(iRet != 0)
        {
            LOG_ERROR << "modify props err. iRet:"<< iRet << endl;
        }
    }

    resp.mutable_morderinfo()->set_sinorderid(inOrderId);
    resp.mutable_morderinfo()->set_soutorderid("");
    resp.mutable_morderinfo()->set_epaytype(req.epaytype());
    resp.mutable_morderinfo()->set_iexchangeindex(req.iexchangeindex());
    resp.mutable_morderinfo()->set_iorderstate(1);
    resp.mutable_morderinfo()->set_lordertime(TNOW);
    resp.mutable_morderinfo()->set_iproductcount(req.iproductcount());
    getProductInfo( req.sproductid(), resp.mutable_morderinfo()->mutable_mproductinfo());

    FUNC_EXIT("", iRet);
    return iRet;
}

int OrderServantImp::doVerifyOrder(const long lPlayerID, const OrderProto::OrderVerifyReq &req, OrderProto::OrderVerifyResp &resp)
{
    FUNC_ENTRY("");
    int iRet = 0;

    LOG_DEBUG << "req:"<< logPb(req)<< endl;

    vector<map<string, string>> vecRecord;
    iRet = ProcessorSingleton::getInstance()->selectOrder(req.morderinfo().sinorderid(), vecRecord);
    if(vecRecord.size() != 1)
    {
        LOG_ERROR << "order not exist. inOrderId:"<< req.morderinfo().sinorderid() << endl;
        return XGameRetCode::ORDER_NOT_EXIST;
    }

    if(S2I(vecRecord[0]["order_status"]) != 0)
    {
        return XGameRetCode::ORDER_STATUS_FAIL;
    }

    if(req.morderinfo().epaytype() == OrderProto::E_PAY_TYPE::E_PAY_IOS)
    {
        iRet = IosPaySingleton::getInstance()->verifyOrder(req.morderinfo().scredential(),req.morderinfo().soutorderid(), vecRecord[0]["product_id"]);
        if(iRet != 0)
        {
            LOG_ERROR << "ios verify order err. iRet:"<< iRet << endl;
            return iRet;
        }
    }
    else if (req.morderinfo().epaytype() == OrderProto::E_PAY_TYPE::E_PAY_ALI)
    {
        iRet = AliPaySingleton::getInstance()->verifyOrder(req.morderinfo().scredential(), req.morderinfo().sinorderid(), vecRecord[0]["product_id"]);
        if (iRet != 0)
        {
            LOG_ERROR << "ali verify order err. iRet:" << iRet << endl;
            return iRet;
        }
    }
    else if (req.morderinfo().epaytype() == OrderProto::E_PAY_TYPE::E_PAY_WECHAT)
    {
        iRet = WePaySingleton::getInstance()->verifyOrder(req.morderinfo().scredential(), req.morderinfo().soutorderid(), vecRecord[0]["product_id"]);
        if (iRet != 0)
        {
            LOG_ERROR << "wechat verify order err. iRet:" << iRet << endl;
            return iRet;
        }
    }

    iRet = cashPruduct(lPlayerID, vecRecord[0]["in_order_id"], req.morderinfo().soutorderid());
    if(iRet != 0)
    {
        LOG_ERROR << "modify props err. iRet:"<< iRet << endl;
    }

    FUNC_EXIT("", iRet);
    return iRet;
}

//兑现商品
int OrderServantImp::cashPruduct(const long lPlayerID, const string &inOrderId, const string &outOrderId, const int iStatus)
{
    FUNC_ENTRY("");
    vector<map<string, string>> vecRecord;
    int iRet = ProcessorSingleton::getInstance()->selectOrder(inOrderId, vecRecord);
    if(iRet != 0 || vecRecord.size() != 1)
    {
        LOG_ERROR << "select order err. inOrderId:"<< inOrderId << endl;
        return iRet;
    }

    map<string, string> updateInfo = {
        { "out_order_id", outOrderId },
        { "order_state",  I2S(iStatus)},
        { "pay_time",  ServiceUtil::CurTimeFormat()},
    };
    iRet = ProcessorSingleton::getInstance()->updateOrder(inOrderId, updateInfo);
    if(iRet != 0 )
    {
        LOG_ERROR << "update order err. inOrderId:"<< inOrderId << endl;
        return iRet;
    }

    iRet = ProcessorSingleton::getInstance()->updateOrderStats(vecRecord[0]["product_id"], S2I(vecRecord[0]["product_count"]));
    if(iRet != 0)
    {
        LOG_ERROR << "update order stats err. iRet:"<< iRet << endl;
        return iRet;
    }

    //同步物品变更
    iRet = modifyUserProps(lPlayerID, inOrderId, vecRecord[0]["product_id"], S2I((vecRecord[0]["exchange_index"])), S2I(vecRecord[0]["product_count"]));
    if(iRet != 0)
    {
        LOG_ERROR << "modify props err. iRet:"<< iRet << endl;
        return iRet;
    }

    return 0;
}

int OrderServantImp::modifyUserProps(const long lPlayerID, const string &inOrderId, const string& productId, const int exchangeIndex, const int productCount)
{
    FUNC_ENTRY("");
    auto pServantPrx = g_app.getOuterFactoryPtr()->getHallServantPrx(lPlayerID);
    if(!pServantPrx)
    {
        LOG_ERROR << "pServantPrx is nullptr." << endl;
        return -1;
    }

    auto productList = g_app.getOuterFactoryPtr()->getProductList();
    auto itProduct = productList.data.find(productId);
    if(itProduct == productList.data.end())
    {
        LOG_ERROR << "product not exist. productId:"<< productId << endl;
        return -2;
    }

    userinfo::ModifyUserPropsReq req;
    if(itProduct->second.productType == 1)//兑换
    {
        if(exchangeIndex < 0 || itProduct->second.costGroup.size() <= 0 || exchangeIndex > int(itProduct->second.costGroup.size() - 1))
        {
            LOG_DEBUG << "exchange err. exchangeIndex:"<< exchangeIndex << endl;
            return XGameRetCode::ORDER_EXCHANGE_NOT_EXIST;
        }
        auto costProps = itProduct->second.costGroup[exchangeIndex];
        if(costProps.propsCount > pServantPrx->getUserPropsById(lPlayerID, costProps.propsId))
        {
            LOG_DEBUG << "props count lack. lPlayerID:"<< lPlayerID<< ", propsId:"<< costProps.propsId << endl;
            return XGameRetCode::ROOM_FEE_LACK;
        }
        userinfo::ModifyUserPropsInfo info;
        info.uid = lPlayerID;
        info.changeType = itProduct->second.productType == 1 ? XGameProto::GOLDFLOW_ID_ORDER_EXCHANGE : XGameProto::GOLDFLOW_ID_ORDER_PAY;
        info.propsInfo.propsID = costProps.propsId;
        info.propsInfo.iState = 1;
        info.propsCount = -costProps.propsCount * productCount;
        info.paraExt = inOrderId;
        req.updateInfo.push_back(info);
    }

    vector<config::PropsInfo> vecPropsGroup;
    vecPropsGroup.insert(vecPropsGroup.begin(), itProduct->second.buyGroup.begin(), itProduct->second.buyGroup.end());
    vecPropsGroup.insert(vecPropsGroup.begin(), itProduct->second.extraGroup.begin(), itProduct->second.extraGroup.end());
    for(auto item : vecPropsGroup)
    {
        userinfo::ModifyUserPropsInfo info;
        info.uid = lPlayerID;
        info.changeType = itProduct->second.productType == 1 ? XGameProto::GOLDFLOW_ID_ORDER_EXCHANGE : XGameProto::GOLDFLOW_ID_ORDER_PAY;
        info.propsInfo.propsID = item.propsId;
        info.propsInfo.iState = 0;
        info.propsCount = item.propsCount * productCount;
        info.paraExt = inOrderId;
        req.updateInfo.push_back(info);
    }
    pServantPrx->async_modifyUserProps(NULL, req);
    FUNC_EXIT("", 0);
    return 0;
}

int OrderServantImp::getProductInfo(const string& productId, OrderProto::ProductInfo *ptr)
{
    FUNC_ENTRY("");
    auto productList = g_app.getOuterFactoryPtr()->getProductList();
    auto itProduct = productList.data.find(productId);
    if(itProduct == productList.data.end())
    {
        LOG_ERROR << "product not exist. productId:"<< productId << endl;
        return -1;
    }
    ptr->set_sproductid(itProduct->second.productId);
    ptr->set_eproducttype(OrderProto::E_PRODUCT_TYPE(itProduct->second.productType));
    ptr->set_sproductname(itProduct->second.productName);
    ptr->set_sproducticon(itProduct->second.productIcon);
    ptr->set_iprice(itProduct->second.price);
    ptr->set_idiscount(itProduct->second.discount);

    for(auto sub : itProduct->second.buyGroup)
    {
        auto it = ptr->add_vbuygroup();
        it->set_ipropsid(sub.propsId);
        it->set_ipropscount(sub.propsCount);
        it->set_spropsiconsmall(g_app.getOuterFactoryPtr()->getPorpsSmallIconByID(sub.propsId));
        it->set_spropsiconbig(g_app.getOuterFactoryPtr()->getPorpsBigIconByID(sub.propsId));
        it->set_spropsname(g_app.getOuterFactoryPtr()->getPorpsNameByID(sub.propsId));
    }
    for(auto sub : itProduct->second.extraGroup)
    {
        auto it = ptr->add_vextragroup();
        it->set_ipropsid(sub.propsId);
        it->set_ipropscount(sub.propsCount);
        it->set_spropsiconsmall(g_app.getOuterFactoryPtr()->getPorpsSmallIconByID(sub.propsId));
        it->set_spropsiconbig(g_app.getOuterFactoryPtr()->getPorpsBigIconByID(sub.propsId));
        it->set_spropsname(g_app.getOuterFactoryPtr()->getPorpsNameByID(sub.propsId));
    }
    for(auto sub : itProduct->second.costGroup)
    {
        auto it = ptr->add_vcostgroup();
        it->set_ipropsid(sub.propsId);
        it->set_ipropscount(sub.propsCount);
        it->set_spropsiconsmall(g_app.getOuterFactoryPtr()->getPorpsSmallIconByID(sub.propsId));
        it->set_spropsiconbig(g_app.getOuterFactoryPtr()->getPorpsBigIconByID(sub.propsId));
        it->set_spropsname(g_app.getOuterFactoryPtr()->getPorpsNameByID(sub.propsId));
    }
    ptr->set_ihot(ProcessorSingleton::getInstance()->isHot(productId));
    FUNC_EXIT("", 0);
    return 0;
}

//发送消息到客户端
template<typename T>
int OrderServantImp::toClientPb(const XGameComm::TPackage &tPackage, const std::string &sCurServrantAddr, XGameProto::ActionName actionName, XGameComm::MSGTYPE type, const T &t)
{
    XGameComm::TPackage rsp;
    auto mh = rsp.add_vecmsghead();
    mh->set_nmsgid(actionName);
    mh->set_nmsgtype(type); //此处根据实际情况更改
    mh->set_servicetype(XGameComm::SERVICE_TYPE::SERVICE_TYPE_ORDER); //此处根据实际情况更改
    rsp.add_vecmsgdata(pbToString(t));

    auto pPushPrx = Application::getCommunicator()->stringToProxy<JFGame::PushPrx>(sCurServrantAddr);
    if (pPushPrx)
    {
        ROLLLOG_DEBUG << "response : " << t.DebugString() << endl;
        pPushPrx->tars_hash(tPackage.stuid().luid())->async_doPushBuf(NULL, tPackage.stuid().luid(), pbToString(rsp));
    }
    else
    {
        ROLLLOG_ERROR << "pPushPrx is null : " << t.DebugString() << endl;
    }

    return 0;
}

