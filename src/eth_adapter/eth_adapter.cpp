#include "eth_adapter.h"
#include "eth_command_defs.h"
#include "rapidjson/document.h"
#include "net/http_client.h"
#include "int-util.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "storages/http_abstract_invoke.h"

    EthAdapter::EthAdapter(){

    }


    // void EthAdapter::checkInboundSwaps()
    // {

    //     // bool r = false;
    //     // epee::net_utils::http::http_simple_client http_client;
    //     // COMMAND_RPC_CHECK_SWAPS::request req = AUTO_VAL_INIT(req);
    //     // COMMAND_RPC_CHECK_SWAPS::request req = AUTO_VAL_INIT(req);

    //     // http_client.set_server("https://kovan.infura.io/v3/079430f0746145d291bba904431ce803", boost::none, epee::net_utils::ssl_support_t::e_ssl_support_autodetect);


    // }

    // void EthAdapter::checkInboundSwaps()
    // {

    //     // bool r = false;
    //     // epee::net_utils::http::http_simple_client http_client;
    //     // COMMAND_RPC_CREATE_SWAP::request req = AUTO_VAL_INIT(req);
    //     // COMMAND_RPC_CREATE_SWAP::response res = AUTO_VAL_INIT(res);

    //     // req.to = "0x18160ddd";
    //     // req.data = "0x18160ddd";

    //     // http_client.set_server("https://kovan.infura.io", boost::none, epee::net_utils::ssl_support_t::e_ssl_support_autodetect);

    //     // bool r = net_utils::invoke_http_json_rpc("/v3/079430f0746145d291bba904431ce803", "eth_call", req, res, *http_client, std::chrono::seconds(10));

    //     // if (!r) {
    //     //     return;
    //     // }




    // }

    std::string jsonString(const rapidjson::Document& d)
    {
        rapidjson::StringBuffer strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
        d.Accept(writer);

        return strbuf.GetString();
    }

    std::string getETHCALL(std::string method, std::string to, std::string data) 
    {
        rapidjson::Document d;

        d.SetObject();
        rapidjson::Document::AllocatorType& a = d.GetAllocator();

        // d.AddMember("id", "1", a);
        // d.AddMember("jsonrpc", "2.0", a);

        rapidjson::Value _to;
        _to.SetString(to.c_str(), a);
        d.AddMember("account", _to, a);

        rapidjson::Value _amount;
        _amount.SetString(data.c_str(), a);
        d.AddMember("amount", _amount, a);

        // rapidjson::Value params(rapidjson::kArrayType);

        // rapidjson::Document param_obj;
        // param_obj.SetObject();
        // rapidjson::Document::AllocatorType& a2 = param_obj.GetAllocator();

        // rapidjson::Value _to;
        // _to.SetString(to.c_str(), a2);
        // param_obj.AddMember("to", _to, a2);

        
        // rapidjson::Value _data;
        // _data.SetString(data.c_str(), a2);
        // param_obj.AddMember("data", _data, a2);

        // params.PushBack(param_obj, a);

        // d.AddMember("params", params, a);

        std::cout << jsonString(d) << std::endl;

        return jsonString(d);
    }

    void EthAdapter::mintWXEQ(std::string account, std::string amount)
    {
        std::cout << "hello" << std::endl;
        epee::net_utils::http::http_simple_client http_client;
        std::string body = getETHCALL("eth_call", "0x85784C07ed86b6225f1128Cfd4fCF858e6Bf10bE", "10023343434343434343340000000000");
        http_client.set_server("127.0.0.1", "9334", boost::none, epee::net_utils::ssl_support_t::e_ssl_support_autodetect);

        COMMAND_RPC_MINT_WXEQ::request req = AUTO_VAL_INIT(req);
        COMMAND_RPC_MINT_WXEQ::response res = AUTO_VAL_INIT(res);
        req.account = account;
        req.amount = amount;
        bool r = true;
        r = epee::net_utils::invoke_http_json("/api/transaction", req, res, http_client, std::chrono::seconds(10), "POST");
    }
