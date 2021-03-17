#include "crypto/hash.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"

  struct COMMAND_RPC_MINT_WXEQ
  {
    struct request_t
    {
      std::string account;
      std::string amount;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(account)
        KV_SERIALIZE(amount)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string 	 result;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(result)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };