// Copyright (c)      2018, The Loki Project
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

#include "service_node_deregister.h"
#include "service_node_list.h"
#include "cryptonote_config.h"
#include "cryptonote_core.h"
#include "version.h"
#include "ribbon.h"
#include "quorum_cop.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "quorum_cop"

namespace service_nodes
{
	quorum_cop::quorum_cop(cryptonote::core& core)
		: m_core(core), m_last_height(0)
	{
		init();
	}

	void quorum_cop::init()
	{
		m_last_height = 0;
		m_uptime_proof_seen.clear();
		m_ribbon_data_received.clear();
	}

	void quorum_cop::blockchain_detached(uint64_t height)
	{
		if (m_last_height >= height)
		{
			LOG_ERROR("The blockchain was detached to height: " << height << ", but quorum cop has already processed votes up to " << m_last_height);
			LOG_ERROR("This implies a reorg occured that was over " << REORG_SAFETY_BUFFER_IN_BLOCKS << ". This should never happen! Please report this to the devs.");
			m_last_height = height;
		}
	}

	void quorum_cop::block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs)
	{
		uint64_t const height = cryptonote::get_block_height(block);
		uint8_t version = m_core.get_hard_fork_version(height);

		if (version < 5)
			return;

		crypto::public_key my_pubkey;
		crypto::secret_key my_seckey;
		if (!m_core.get_service_node_keys(my_pubkey, my_seckey))
			return;

		time_t const now = time(nullptr);
		time_t const min_lifetime = 60 * 60 * 2;
		bool alive_for_min_time = (now - m_core.get_start_time()) >= min_lifetime;
		if (!alive_for_min_time)
		{
			return;
		}

		uint64_t const latest_height = std::max(m_core.get_current_blockchain_height(), m_core.get_target_blockchain_height());

		if (latest_height < triton::service_node_deregister::VOTE_LIFETIME_BY_HEIGHT)
			return;

		uint64_t const execute_justice_from_height = latest_height - triton::service_node_deregister::VOTE_LIFETIME_BY_HEIGHT;
		if (height < execute_justice_from_height)
			return;

		if (m_last_height < execute_justice_from_height)
			m_last_height = execute_justice_from_height;

		ribbon_protocol rp(m_core);
		// store trades that have happened during this block to the DB
		if (version > 5){
			std::vector<service_nodes::exchange_trade> trades_during_block = rp.trades_during_latest_1_block();
			m_core.store_trade_history_at_height(trades_during_block, height);
		}

		for (; m_last_height < (height - REORG_SAFETY_BUFFER_IN_BLOCKS); m_last_height++)
		{
			if (m_core.get_hard_fork_version(m_last_height) < 5)
				continue;

			const std::shared_ptr<const quorum_state> state = m_core.get_quorum_state(m_last_height);
			if (!state)
			{
				// TODO(triton): Fatal error
				LOG_ERROR("Quorum state for height: " << m_last_height << "was not cached in daemon!");
				continue;
			}

			auto it = std::find(state->quorum_nodes.begin(), state->quorum_nodes.end(), my_pubkey);
			if (it == state->quorum_nodes.end())
				continue;

			size_t my_index_in_quorum = it - state->quorum_nodes.begin();
			for (size_t node_index = 0; node_index < state->nodes_to_test.size(); ++node_index)
			{
				const crypto::public_key &node_key = state->nodes_to_test[node_index];

				CRITICAL_REGION_LOCAL(m_lock);
				bool uptime_proof_seen = (m_uptime_proof_seen.find(node_key) != m_uptime_proof_seen.end());
				
				if (version > 5)
				{
					crypto::hash pair_hash = make_ribbon_key_hash(node_key, m_last_height);
					bool ribbon_data_seen = (m_ribbon_data_received.find(pair_hash) != m_ribbon_data_received.end());
					LOG_ERROR("Ribbon Data has not been seen! From: " << node_key);

					std::vector<service_nodes::exchange_trade> trades_during_height;
					m_core.get_trade_history_for_height(trades_during_height, m_last_height);
					uint64_t my_ribbon_blue = convert_btc_to_usd(create_ribbon_blue(trades_during_height));
					LOG_ERROR("My Ribbon Data: " << my_ribbon_blue);

					bool ribbon_data_agrees = false;
					if (my_ribbon_blue == m_ribbon_data_received[pair_hash])
						ribbon_data_agrees = true;

					if (uptime_proof_seen && ribbon_data_seen && ribbon_data_agrees)
						continue;
				}
				else
				{
					if (uptime_proof_seen)
						continue;
				}

				triton::service_node_deregister::vote vote = {};
				vote.block_height = m_last_height;
				vote.service_node_index = node_index;
				vote.voters_quorum_index = my_index_in_quorum;
				vote.signature = triton::service_node_deregister::sign_vote(vote.block_height, vote.service_node_index, my_pubkey, my_seckey);

				cryptonote::vote_verification_context vvc = {};
				if (!m_core.add_deregister_vote(vote, vvc))
				{
					LOG_ERROR("Failed to add deregister vote reason: " << print_vote_verification_context(vvc, &vote));
				}
			}
		}
	}

	static crypto::hash make_hash(crypto::public_key const &pubkey, uint64_t timestamp)
	{
		char buf[44] = "SUP"; // Meaningless magic bytes
		crypto::hash result;
		memcpy(buf + 4, reinterpret_cast<const void *>(&pubkey), sizeof(pubkey));
		memcpy(buf + 4 + sizeof(pubkey), reinterpret_cast<const void *>(&timestamp), sizeof(timestamp));
		crypto::cn_fast_hash(buf, sizeof(buf), result);

		return result;
	}
	
	crypto::hash make_ribbon_hash(uint64_t timestamp, uint64_t height, uint64_t ribbon_green, uint64_t ribbon_blue, crypto::public_key pubkey)
	{
		char buf[256] = "RIB";
		
		memcpy(buf + 4, reinterpret_cast<const void *>(&timestamp), sizeof(timestamp));
		memcpy(buf + 4 + sizeof(timestamp), reinterpret_cast<const void *>(&height), sizeof(height));
		memcpy(buf + 4 + sizeof(timestamp) + sizeof(height), reinterpret_cast<const void *>(&ribbon_green), sizeof(ribbon_green));
		memcpy(buf + 4 + sizeof(timestamp) + sizeof(height) + sizeof(ribbon_green), reinterpret_cast<const void *>(&ribbon_blue), sizeof(ribbon_blue));
		memcpy(buf + 4 + sizeof(timestamp) + sizeof(height) + sizeof(ribbon_green) + sizeof(ribbon_blue), reinterpret_cast<const void *>(&pubkey), sizeof(pubkey));
		
		crypto::hash result;
		crypto::cn_fast_hash(buf, sizeof(buf), result);
		return result;
	}
	
	crypto::hash quorum_cop::make_ribbon_key_hash(crypto::public_key pubkey, uint64_t height)
	{
		char buf[64];
		memcpy(buf, reinterpret_cast<const void *>(&pubkey), sizeof(pubkey));
		memcpy(buf + 4 + sizeof(pubkey), reinterpret_cast<const void *>(&height), sizeof(height));
		crypto::hash result;
		crypto::cn_fast_hash(buf, sizeof(buf), result);
		return result;
	}

	bool quorum_cop::handle_uptime_proof(const cryptonote::NOTIFY_UPTIME_PROOF::request &proof)
	{
		uint64_t now = time(nullptr);

		uint64_t timestamp = proof.timestamp;
		const crypto::public_key& pubkey = proof.pubkey;
		const crypto::signature& sig = proof.sig;


		if ((timestamp < now - UPTIME_PROOF_BUFFER_IN_SECONDS) || (timestamp > now + UPTIME_PROOF_BUFFER_IN_SECONDS))
			return false;

		if (!m_core.is_service_node(pubkey))
			return false;

		if (!(proof.snode_version_major == 5 &&
			proof.snode_version_minor == 0 &&
			proof.snode_version_patch == 0))
		{
			return false;
		}


		CRITICAL_REGION_LOCAL(m_lock);
		if (m_uptime_proof_seen[pubkey] >= now - (UPTIME_PROOF_FREQUENCY_IN_SECONDS / 2))
			return false; // already received one uptime proof for this node recently.

		crypto::hash hash = make_hash(pubkey, timestamp);
		if (!crypto::check_signature(hash, pubkey, sig))
			return false;

		m_uptime_proof_seen[pubkey] = now;
		return true;
	}
	
	bool quorum_cop::handle_ribbon_data_received(const cryptonote::NOTIFY_RIBBON_DATA::request &data)
	{
		crypto::hash hash = make_ribbon_hash(data.timestamp, data.height, data.ribbon_green, data.ribbon_blue, data.pubkey);
		if (!crypto::check_signature(hash, data.pubkey, data.sig))
			return false;
		
		const crypto::public_key& pubkey = data.pubkey;
		crypto::hash pair_hash = make_ribbon_key_hash(pubkey, data.height);
		
		m_ribbon_data_received[pair_hash] = data.ribbon_blue;
		return true;
	}

	void generate_uptime_proof_request(const crypto::public_key& pubkey, const crypto::secret_key& seckey, cryptonote::NOTIFY_UPTIME_PROOF::request& req)
	{
		req.snode_version_major = static_cast<uint16_t>(TRITON_VERSION_MAJOR);
		req.snode_version_minor = static_cast<uint16_t>(TRITON_VERSION_MINOR);
		req.snode_version_patch = static_cast<uint16_t>(TRITON_VERSION_PATCH);
		req.timestamp = time(nullptr);
		req.pubkey = pubkey;

		crypto::hash hash = make_hash(req.pubkey, req.timestamp);
		crypto::generate_signature(hash, pubkey, seckey, req.sig);
	}
	
	bool quorum_cop::generate_ribbon_data_request(const crypto::public_key& pubkey, const crypto::secret_key& seckey, cryptonote::NOTIFY_RIBBON_DATA::request& req)
	{
		ribbon_protocol rp(m_core);
		req.timestamp = time(nullptr);
		req.height = m_core.get_current_blockchain_height() - 1;

		std::vector<service_nodes::exchange_trade> recent_trades = rp.trades_during_latest_1_block();

		
		if(recent_trades.size() == 0){
			req.ribbon_green = 0;
			req.ribbon_blue = 0;
		}else{
			req.ribbon_green = service_nodes::create_ribbon_green(recent_trades);
			req.ribbon_blue = service_nodes::create_ribbon_blue(recent_trades);
		}
		req.pubkey = pubkey;

		crypto::hash hash = make_ribbon_hash(req.timestamp, req.height, req.ribbon_green, req.ribbon_blue, req.pubkey);
		crypto::generate_signature(hash, pubkey, seckey, req.sig);
		crypto::hash expected_hash = make_ribbon_key_hash(pubkey, req.height);
		std::cout << "Created hash at Height: " << req.height << " with hash of: " << expected_hash << std::endl;  
		return true;
	}


	bool quorum_cop::prune_uptime_proof()
	{
		uint64_t now = time(nullptr);
		const uint64_t prune_from_timestamp = now - UPTIME_PROOF_MAX_TIME_IN_SECONDS;
		CRITICAL_REGION_LOCAL(m_lock);

		for (auto it = m_uptime_proof_seen.begin(); it != m_uptime_proof_seen.end();)
		{
			if (it->second < prune_from_timestamp)
				it = m_uptime_proof_seen.erase(it);
			else
				it++;
		}

		return true;
	}

	uint64_t quorum_cop::get_uptime_proof(const crypto::public_key &pubkey) const
	{
		CRITICAL_REGION_LOCAL(m_lock);

		const auto& it = m_uptime_proof_seen.find(pubkey);
		if (it == m_uptime_proof_seen.end())
		{
			return 0;
		}

		return (*it).second;
	}

    uint64_t quorum_cop::get_ribbon_data(const crypto::public_key &pubkey, uint64_t height)
    {
      CRITICAL_REGION_LOCAL(m_lock);
	  crypto::hash pair_hash = make_ribbon_key_hash(pubkey, height);
	  std::cout << pair_hash << "Height: " << height << std::endl;

	std::unordered_map<crypto::hash, uint64_t>::iterator it1 = m_ribbon_data_received.begin();
	while(it1 != m_ribbon_data_received.end())
	{	
		if(it1->first == pair_hash)
			std::cout << "Found ze winner ja" << std::endl;
	std::cout<<it1->first << " :: "<< it1->second<< std::endl;
	it1++;
	}
     
      const auto& it = m_ribbon_data_received.find(pair_hash);
      if (it != m_ribbon_data_received.end())
      {
        return it->second;
      }

      return 0;
    }
    
	std::unordered_map<crypto::hash, uint64_t> quorum_cop::get_all_ribbon_data()
	{
		return m_ribbon_data_received;
	}
	
	bool quorum_cop::send_out_ribbon(){
		return m_core.submit_ribbon_data();
	}

	void quorum_cop::clear_ribbon_data(){
		m_ribbon_data_received.clear();
	}

}
