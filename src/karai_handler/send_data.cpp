// Copyright (c) 2020 Harrison Hesslink
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers
    

#include <math.h>
#include <iostream>

#include "rapidjson/document.h"
#include "net/http_client.h"
#include "send_data.h"
#include "int-util.h"
#include "vector"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "delfi/price_provider.h"

namespace karai {

    static crypto::hash make_data_hash(crypto::public_key const &pubkey, std::string data)
	{
		char buf[256] = "SUP"; // Meaningless magic bytes
		crypto::hash result;
		memcpy(buf + 4, reinterpret_cast<const void *>(&pubkey), sizeof(pubkey));
		memcpy(buf + 4 + sizeof(pubkey), reinterpret_cast<const void *>(&data), sizeof(data));
		crypto::cn_fast_hash(buf, sizeof(buf), result);

		return result;
	}

    void handle_block(const cryptonote::block &b, const cryptonote::block &last_block, const crypto::public_key &my_pubc_key, const crypto::secret_key &my_sec, const std::vector<crypto::public_key> &node_keys)
    {

        crypto::public_key last_winner_pubkey = cryptonote::get_service_node_winner_from_tx_extra(last_block.miner_tx.extra);
        crypto::public_key winner_pubkey = cryptonote::get_service_node_winner_from_tx_extra(b.miner_tx.extra);

        std::vector<price_provider::contract_data> contracts = price_provider::getContractKarai();

        

        if (epee::string_tools::pod_to_hex(my_pubc_key) == epee::string_tools::pod_to_hex(last_winner_pubkey)) 
        {
            //we are winner send consensus tx

            std::vector<std::pair<std::string, std::string>> payload_data;
            std::string nodes_json_string;
            std::vector <std::string> nodes_on_network;
            nodes_on_network.reserve(node_keys.size());

            for (auto key : node_keys) 
            {
                nodes_on_network.push_back(epee::string_tools::pod_to_hex(key));
                nodes_json_string += epee::string_tools::pod_to_hex(key);
            }

            crypto::hash nodes_hash = make_data_hash(my_pubc_key, nodes_json_string);

            crypto::signature signature;
            crypto::generate_signature(nodes_hash, my_pubc_key, my_sec, signature);

            payload_data.push_back(std::make_pair("hash", epee::string_tools::pod_to_hex(nodes_hash)));
            payload_data.push_back(std::make_pair("height", std::to_string(cryptonote::get_block_height(b))));
            payload_data.push_back(std::make_pair("pubkey", epee::string_tools::pod_to_hex(my_pubc_key)));
            payload_data.push_back(std::make_pair("signature", epee::string_tools::pod_to_hex(signature)));
            payload_data.push_back(std::make_pair("task", "Consensus"));

            //bool 
            bool r = karai::send_consensus_data(payload_data, nodes_on_network);
        } 
        else 
        {
            if (contracts.size() > 0) {
                for (auto contract : contracts) {
                    std::string price = price_provider::getTradeOgrePrice(std::make_pair(contract.pair.second, contract.pair.first));
                    crypto::hash price_hash = make_data_hash(my_pubc_key, price);

                    crypto::signature signature;
                    crypto::generate_signature(price_hash, my_pubc_key, my_sec, signature);

                    std::vector<std::pair<std::string, std::string>> payload_data;

                    payload_data.push_back(std::make_pair("hash", epee::string_tools::pod_to_hex(price_hash)));
                    payload_data.push_back(std::make_pair("height", std::to_string(cryptonote::get_block_height(b))));
                    payload_data.push_back(std::make_pair("pubkey", epee::string_tools::pod_to_hex(my_pubc_key)));
                    payload_data.push_back(std::make_pair("signature", epee::string_tools::pod_to_hex(signature)));
                    payload_data.push_back(std::make_pair("data", price));
                    payload_data.push_back(std::make_pair("task", contract.pair.first + "/" + contract.pair.second));
                    payload_data.push_back(std::make_pair("epoc", contract.contract_epoc));
                    payload_data.push_back(std::make_pair("source", "tradeogre.com"));

                    bool r = karai::send_oracle_data(payload_data);
                }
            } else {
                LOG_PRINT_L1("No Contracts on Karai!");
            }
        }


    }


    bool send_oracle_data(const std::vector<std::pair<std::string, std::string>> data)
    {

        LOG_PRINT_L1("Sending Oracle data!");
        std::string body = create_json(data);
        bool r = make_request(body, "/api/v1/new_tx");
        return r;
    }

    bool send_consensus_data(const std::vector<std::pair<std::string, std::string>> data, const std::vector<std::string> &nodes_on_network)
    {
        LOG_PRINT_L1("Sending Consensus data!");
        std::string body = create_consensus_json(data, nodes_on_network);
        
        LOG_PRINT_L1("Data: " + body);

        bool r = make_request(body, "/api/v1/new_consensus_tx");

        return r;
    }

    bool make_request(std::string body, std::string uri)
    {
        epee::net_utils::http::http_simple_client http_client;
        epee::net_utils::http::http_response_info res;
        const epee::net_utils::http::http_response_info *res_info = &res;
        epee::net_utils::http::fields_list fields;

        std::string url = "127.0.0.1";
        http_client.set_server(url, "4203",  boost::none);

        bool r =  http_client.invoke_post(uri, body, std::chrono::seconds(10), &res_info, fields);
        
        return r;
    }


    std::string create_json(const std::vector<std::pair<std::string, std::string>> data)
    {
        rapidjson::Document d;

        d.SetObject();
        rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

        for (auto it : data) 
        {

            if (it.second == "")
                return "";

            rapidjson::Value v;
            rapidjson::Value k;
            k.SetString(it.first.c_str(), allocator);
            v.SetString(it.second.c_str(), allocator);
            d.AddMember(k, v, allocator);
        }

        return jsonString(d);
    }

     std::string create_consensus_json(const std::vector<std::pair<std::string, std::string>> data, const std::vector <std::string> &nodes_on_network)
    {
        rapidjson::Document d;

        d.SetObject();
        rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

        rapidjson::Value node_json(rapidjson::kArrayType);

        for (auto it : nodes_on_network) 
        {
            rapidjson::Value v;
            v.SetString(it.c_str(), allocator);
            node_json.PushBack(v, allocator);
        }


        for (auto it : data) 
        {
            if (it.second == "")
                return "";

            rapidjson::Value v;
            rapidjson::Value k;
            k.SetString(it.first.c_str(), allocator);
            v.SetString(it.second.c_str(), allocator);
            d.AddMember(k, v, allocator);
        }

        d.AddMember("data", node_json, allocator);

        return jsonString(d);
    }


    std::string jsonString(const rapidjson::Document& d)
    {
        rapidjson::StringBuffer strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
        d.Accept(writer);

        return strbuf.GetString();
    }

}