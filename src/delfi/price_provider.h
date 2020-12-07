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

#include "int-util.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include <string>
#include <vector>
namespace price_provider{

    struct contract_data {
        std::pair<std::string, std::string> pair;
        std::string contract_epoc;
    };

    std::vector<price_provider::contract_data> getContractKarai();
    uint64_t getCoinbasePrice(std::pair<std::string, std::string> pair /*  BTCUSD  */);
    std::string getTradeOgrePrice(std::pair<std::string, std::string> pair /*  BTC-XEQ  */);
    uint64_t getBittrexPrice(std::pair<std::string, std::string> pair /*  XHV-BTC  */);
    uint64_t getNancePrice(std::pair<std::string, std::string> pair /*  BTCUSDT  */);
    uint64_t getKucoinPrice(std::pair<std::string, std::string> pair /*  BTC-XEQ  */);
    uint64_t getHuobiPrice(std::pair<std::string, std::string> pair /*  BTC-XEQ  */);
    uint64_t getFOREX(std::pair<std::string, std::string> pair /*  BTC-XEQ  */);
    uint64_t getILIFOREX(std::pair<std::string, std::string> pair /*  BTC-XEQ  */);
}