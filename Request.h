#ifndef __REQUEST_H__
#define __REQUEST_H__

#include "curl/curl.h"
#include <functional>
#include <string>
#include "LogComm.h"
#include <stdexcept>

// TODO : POST 和 GET 请求封装为异步的方式l
class Request
{
public:
    Request()
    {

    }
    ~Request()
    {

    }

public:
    // url: 请求的url
    // param: 请求的参数
    // 这个接口有待商榷 TODO
    static string get(const std::string &url, const std::string &httpProxy = "", const std::vector<std::string> &headers = std::vector<std::string>())
    {
        ROLLLOG_DEBUG << "GET url : " << url << endl;

        auto easy_handle = curl_easy_init();
        if (!easy_handle)
        {
            ROLLLOG_ERROR << "get a easy handle failed." << endl;
            throw logic_error("curl_easy_init failed!");
        }

        //构建HTTP报文头
        struct curl_slist *pHeaders = NULL;
        for (const auto &header : headers)
        {
            pHeaders = curl_slist_append(pHeaders, header.c_str());
        }

        curl_easy_setopt(easy_handle, CURLOPT_HTTPHEADER, pHeaders);

        std::string	responseBody;
        std::vector<std::string> _headers;

        // 设置easy handle属性
        curl_easy_setopt(easy_handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, &Request::read_body);
        curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(easy_handle, CURLOPT_HEADERFUNCTION, &Request::read_headers);
        curl_easy_setopt(easy_handle, CURLOPT_HEADERDATA, &_headers);

        if (false == httpProxy.empty())
        {
            curl_easy_setopt(easy_handle, CURLOPT_PROXY, httpProxy.c_str());//http://127.0.0.1:80
            curl_easy_setopt(easy_handle, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);// 设置代理类型为http
            curl_easy_setopt(easy_handle, CURLOPT_HTTPPROXYTUNNEL, 1L);//隧道转发流量
        }

        // 设置超时
        curl_easy_setopt(easy_handle, CURLOPT_CONNECTTIMEOUT, 13);	//连接超时
        curl_easy_setopt(easy_handle, CURLOPT_TIMEOUT, 10);		// 接收数据超时

        // 设置错误信息的存储位置
        char errorBuffer[CURL_ERROR_SIZE];
        auto code = curl_easy_setopt(easy_handle, CURLOPT_ERRORBUFFER, errorBuffer);
        if (code != CURLE_OK)
        {
            ROLLLOG_ERROR << "Failed to set error buffer " << code << endl;
        }

        // 执行数据请求
        auto res = curl_easy_perform(easy_handle);
        if (res != CURLE_OK)
        {
            ROLLLOG_ERROR << "errorBuffer : " << errorBuffer << endl;
            ROLLLOG_ERROR << "curl_easy_perform() failed:" << curl_easy_strerror(res) << endl;
            throw logic_error("curl_easy_perform failed!");
        }

        // 响应为200的时候才处理,如果是404,302之类的直接抛出异常
        long statusCode = -1;
        code = curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &statusCode);
        (void)code;
        if (statusCode != 200)
        {
            ROLLLOG_ERROR << "statusCode is not 200! statusCode : " << statusCode << endl;
            throw logic_error("statusCode is not 200!");
        }

        if (pHeaders)
        {
            curl_slist_free_all(pHeaders);
        }

        curl_easy_cleanup(easy_handle);
        return responseBody;
    }

    // @params
    // url    : 请求url
    // body   : 包体
    // headers : http 头
    // 代理
    static string post(const std::string &url, const std::string &body, const std::string &httpProxy = "",
                       const std::vector<std::string> &headers = std::vector<std::string>())
    {
        ROLLLOG_DEBUG << "POST url : " << url << " body : " << body << endl;

        struct curl_slist *pHeaders = NULL;
        auto conn = curl_easy_init();
        if (!conn)
        {
            ROLLLOG_ERROR << "get a easy handle failed." << endl;
            throw logic_error("curl_easy_init failed!");
        }


        curl_easy_setopt(conn, CURLOPT_URL, url.c_str());

        for (const auto &header : headers)
        {
            pHeaders = curl_slist_append(pHeaders, header.c_str());
        }

        curl_easy_setopt(conn, CURLOPT_HTTPHEADER, pHeaders);
        curl_easy_setopt(conn, CURLOPT_POST, 1);//设置为非0表示本次操作为POST
        curl_easy_setopt(conn, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(conn, CURLOPT_POSTFIELDSIZE, body.size());

        string responseBody;
        std::vector<std::string> _headers;
        curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, &Request::read_body);
        curl_easy_setopt(conn, CURLOPT_WRITEDATA, &responseBody);//设置写数据
        curl_easy_setopt(conn, CURLOPT_HEADERFUNCTION, &Request::read_headers);
        curl_easy_setopt(conn, CURLOPT_HEADERDATA, &_headers);

        if (httpProxy.empty() == false)
        {
            curl_easy_setopt(conn, CURLOPT_PROXY, httpProxy.c_str());//http://127.0.0.1:80
            curl_easy_setopt(conn, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);// 设置代理类型为http
            curl_easy_setopt(conn, CURLOPT_HTTPPROXYTUNNEL, 1L);//隧道转发流量
        }

        // 设置超时
        curl_easy_setopt(conn, CURLOPT_CONNECTTIMEOUT, 13);	//连接超时
        curl_easy_setopt(conn, CURLOPT_TIMEOUT, 10);		// 接收数据超时

        // 设置错误信息的存储位置
        char errorBuffer[CURL_ERROR_SIZE];
        auto code = curl_easy_setopt(conn, CURLOPT_ERRORBUFFER, errorBuffer);
        if (code != CURLE_OK)
        {
            ROLLLOG_ERROR << "Failed to set error buffer " << code << endl;
        }

        auto res = curl_easy_perform(conn);
        if (res != CURLE_OK)
        {
            ROLLLOG_ERROR << "errorBuffer : " << errorBuffer << endl;
            ROLLLOG_ERROR << "curl_easy_perform() failed:" << curl_easy_strerror(res) << endl;
            throw logic_error("curl_easy_perform failed!");
        }

        // 响应为200的时候才处理,如果是404,302之类的直接抛出异常
        long statusCode = -1;
        code = curl_easy_getinfo(conn, CURLINFO_RESPONSE_CODE, &statusCode);
        if (statusCode != 200)
        {
            ROLLLOG_ERROR << "statusCode is not 200! statusCode : " << statusCode << endl;
            throw logic_error("statusCode is not 200!");
        }

        if (pHeaders)
        {
            curl_slist_free_all(pHeaders);
        }

        curl_easy_cleanup(conn);
        return responseBody;
    }

    //URL编码
    static string urlEncode(const string &input)
    {
        auto escape_control = curl_escape(input.c_str(), input.size());
        if (!escape_control)
            throw logic_error("urlEncode failed!");

        return string(escape_control);
    }

    //URL编码
    static string urlEncode(const std::vector<std::pair<std::string, std::string>> &params)
    {
        ostringstream os;
        size_t len = params.size();
        size_t i = 0;
        for (const auto &_pair : params)
        {
            const string &key = _pair.first;
            const string &value = _pair.second;
            string tmpKey = urlEncode(key);
            string tmpValue = urlEncode(value);
            os << tmpKey << "=" << tmpValue;
            if (i != len - 1)
                os << "&";
            i += 1;
        }

        return os.str();
    }

    //URL解码
    static string urlDecode(const string &input)
    {
        auto unescape_control = curl_unescape(input.c_str(), input.size());
        if (!unescape_control)
            throw logic_error("urlEncode failed!");

        return string(unescape_control);
    }

private:
    /**
     * [read_body description]
     * @param  buffer [description]
     * @param  size   [description]
     * @param  nmemb  [description]
     * @param  param  [description]
     * @return        [description]
     */
    static size_t read_body(void *buffer, size_t size, size_t nmemb, void *param)
    {
        auto pBody = (std::string *)param;
        pBody->assign((const char *)buffer, (const char *)buffer + size * nmemb);
        return size * nmemb;
    }

    /**
     * [read_headers description]
     * @param  buffer [description]
     * @param  size   [description]
     * @param  nmemb  [description]
     * @param  param  [description]
     * @return        [description]
     */
    static size_t read_headers(void *buffer, size_t size, size_t nmemb, void *param)
    {
        auto pHeaders  = (std::vector<std::string> *)param;
        string header((const char *)buffer, (const char *)buffer + size * nmemb);
        pHeaders->push_back(header);
        return size * nmemb;
    }
};

#endif // !__REQUEST_H__
