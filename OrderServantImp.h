#ifndef _OrderServantImp_H_
#define _OrderServantImp_H_

#include "servant/Application.h"
#include "OrderServant.h"
#include "XGameComm.pb.h"
#include "CommonStruct.pb.h"
#include "Order.pb.h"
#include "Push.h"

/**
 *订单服务接口
 *
 */
class OrderServantImp : public order::OrderServant
{
public:
    /**
     *
     */
    virtual ~OrderServantImp() {}

    /**
     *
     */
    virtual void initialize();

    /**
     *
     */
    virtual void destroy();

    //http请求处理接口
    virtual tars::Int32 doRequest(const vector<tars::Char> &reqBuf, const map<std::string, std::string> &extraInfo,
                                  vector<tars::Char> &rspBuf, tars::TarsCurrentPtr current);

    //tcp请求处理接口
    virtual tars::Int32 onRequest(tars::Int64 lUin, const std::string &sMsgPack, const std::string &sCurServrantAddr, const JFGame::TClientParam &stClientParam,  const JFGame::UserBaseInfoExt &stUserBaseInfo, tars::TarsCurrentPtr current);

    //人工订单
    virtual tars::Int32 manOrder(tars::Int64  lUin, tars::Int64  iPropsID, tars::Int64 iPropsCount, tars::TarsCurrentPtr current);

public:
    //查询商品列表
    int onQueryProductList(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, const OrderProto::OrderProductListReq &req, tars::TarsCurrentPtr current);
    //创建订单
    int onCreateOrder(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, const OrderProto::OrderCreateReq &req, tars::TarsCurrentPtr current);
    //订单验证
    int onVerifyOrder(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, const OrderProto::OrderVerifyReq &req, tars::TarsCurrentPtr current);
    //订单查询
    int onQueryOrderList(const XGameComm::TPackage &pkg, const std::string &sCurServrantAddr, const OrderProto::OrderListQueryReq &req, tars::TarsCurrentPtr current);

public:
    int doCreateOrder(const long lPlayerID, const OrderProto::OrderCreateReq &req, OrderProto::OrderCreateResp &resp);

    int doVerifyOrder(const long lPlayerID, const OrderProto::OrderVerifyReq &req, OrderProto::OrderVerifyResp &resp);

public:
    int cashPruduct(const long lPlayerID, const string &inOrderId, const string &outOrderId, const int iStatus = 1);

    int modifyUserProps(const long lPlayerID, const string &inOrderId, const string& productId, const int exchangeIndex, const int productCount);

    int getProductInfo(const string& productId, OrderProto::ProductInfo *productInfo);

private:
    //发送消息到客户端
    template<typename T>
    int toClientPb(const XGameComm::TPackage &tPackage, const std::string &sCurServrantAddr, XGameProto::ActionName actionName, XGameComm::MSGTYPE type, const T &t);
};

/////////////////////////////////////////////////////
#endif
