namespace service_nodes {

#define TRADE_OGRE_API "https://tradeogre.com/api/v1"
#define BITLIBER_API "https://bitliber.com/api/v1"
#define COINBASE_PRO "https://api.pro.coinbase.com"
#define GEMINI_API "https://api.gemini.com/v1"
#define TRITON_EX "http://134.209.50.66/api"

struct exchange_trade {
  uint64_t date;
  std::string type;
  double price;
  double quantity;
};

struct exchange_order {
  double price;
  double quantity;
};

struct adjusted_liquidity {
  double liquid;
  double price;
};

//Trade API functions
//--XTRI--
bool get_trades_from_ogre(std::vector<exchange_trade> *trades);
bool get_trades_from_tritonex(std::vector<exchange_trade> *trades);

bool get_orders_from_ogre(std::vector<exchange_order> *orders);

//--BITCOIN USD--
double get_coinbase_pro_btc_usd();
double get_gemini_btc_usd();

//--Trade Functions
std::vector<exchange_trade> trades_during_latest_1_block(std::vector<exchange_trade> trades, cryptonote::Blockchain* chain);

//Price Functions
double get_usd_average(double gemini_usd, double coinbase_pro_usd);
double price_over_x_blocks(int blocks);
double create_ribbon_red();
double create_ribbon_green(std::vector<exchange_trade> trades);
double create_ribbon_blue(std::vector<exchange_trade> trades);
double filter_trades_by_deviation(std::vector<exchange_trade> trades);
double trades_weighted_mean(std::vector<exchange_trade> trades);

//Liquid Functions
std::vector<adjusted_liquidity> create_adjusted_liqudities(std::vector<exchange_order> orders, double spot);
double create_mac(std::vector<adjusted_liquidity> adj_liquids);

}
