#ifndef _Processor_H_
#define _Processor_H_

//
#include <util/tc_singleton.h>
#include "OrderServant.h"
#include <curl/curl.h>
#include <json/json.h>
#include "globe.h"

#include "Order.pb.h"

//
using namespace tars;

/**
 *请求处理类
 *
 */
class Processor
{
public:
	/**
	 * 
	*/
	Processor();

	/**
	 * 
	*/
	~Processor();

public:
	void init();

public:
	int createOrder(const map<string, string>& mapOrderInfo);
	//
	int selectOrder(const string& inOrderId, vector<map<string, string>>&vecRecord);
	//
	int selectOrderByPayType(const long lPlayerID, const OrderProto::E_PAY_TYPE ePayType, vector<map<string, string>>&vecRecord);
	//
	int updateOrder(const string& inOrderId, const map<string, string>& mapOrderInfo);
	//
	int updateOrderStats(const string& productId, const int count = 1);
	//
	int selectOrderStats(const string& productId, map<string, string>& mapRecord);
public:
	//
	int isHot(const string& productId);
};

//singleton
typedef TC_Singleton<Processor, CreateStatic, DefaultLifetime> ProcessorSingleton;

#endif

