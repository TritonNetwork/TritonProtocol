#include <vector>
#include <curl/curl.h>
#include <iostream>
#include <math.h>

#include "int-util.h"
#include "rapidjson/document.h"
#include "cryptonote_core.h"
#include "ribbon.h"

namespace service_nodes {


size_t curl_write_callback(char *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// TODO: use http_client from net tools
std::string make_curl_http_get(std::string url)
{
  std::string read_buffer;
  CURL* curl = curl_easy_init(); 
  
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); 
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);
  CURLcode res = curl_easy_perform(curl); 
  curl_easy_cleanup(curl);
  
  return read_buffer;
}

ribbon_protocol::ribbon_protocol(cryptonote::core& core) : m_core(core){};

std::vector<exchange_trade> ribbon_protocol::trades_during_latest_1_block()
{
  std::vector<exchange_trade> trades = get_recent_trades();
  uint64_t top_block_height = m_core.get_current_blockchain_height() - 2;
  crypto::hash top_block_hash = m_core.get_block_id_by_height(top_block_height);
  cryptonote::block top_blk;
  m_core.get_block_by_hash(top_block_hash, top_blk);
  uint64_t top_block_timestamp = top_blk.timestamp;

  std::vector<exchange_trade> result;
  for (size_t i = 0; i < trades.size(); i++)
  {
    if (trades[i].date >= top_block_timestamp){
      result.push_back(trades[i]);
    }
  }
  return result;
}

uint64_t ribbon_protocol::create_ribbon_red(uint64_t height){
    uint64_t ma1_sum = 0;
    uint64_t ma1_vol_sum = 0;
    for (size_t i = 1; i <= 960; i++)
    {
      cryptonote::block blk;
      crypto::hash block_hash = m_core.get_block_id_by_height(height - i);
      m_core.get_block_by_hash(block_hash, blk);
      ma1_sum += (blk.ribbon_volume * blk.ribbon_blue);
      ma1_vol_sum += blk.ribbon_volume;
    }
    uint64_t ma1;
    if(ma1_vol_sum > 0)
      ma1 = (ma1_sum / ma1_vol_sum);
    else
      ma1 = 0;
    
    uint64_t ma2_sum = 0;
    uint64_t ma2_vol_sum = 0;
    for (size_t i = 1; i <= 480; i++)
    {
      cryptonote::block blk;
      crypto::hash block_hash = m_core.get_block_id_by_height(height - i);
      m_core.get_block_by_hash(block_hash, blk);
      ma2_sum += (blk.ribbon_volume * blk.ribbon_blue);
      ma2_vol_sum += blk.ribbon_volume;
    }
    uint64_t ma2;
    if(ma2_vol_sum > 0)
      ma2 = (ma2_sum / ma2_vol_sum);
    else 
      ma2 = 0;    

    uint64_t ma3_sum = 0;
    uint64_t ma3_vol_sum = 0;
    for (size_t i = 1; i <= 240; i++)
    {
      cryptonote::block blk;
      crypto::hash block_hash = m_core.get_block_id_by_height(height - i);
      m_core.get_block_by_hash(block_hash, blk);
      ma3_sum += (blk.ribbon_volume * blk.ribbon_blue);
      ma3_vol_sum += blk.ribbon_volume;
    }
     uint64_t ma3;
    if(ma3_vol_sum > 0)
      ma3 = (ma3_sum / ma3_vol_sum);
    
    uint64_t ma4_sum = 0;
    uint64_t ma4_vol_sum = 0;
    for (size_t i = 1; i <= 120; i++)
    {
      cryptonote::block blk;
      crypto::hash block_hash = m_core.get_block_id_by_height(height - i);
      m_core.get_block_by_hash(block_hash, blk);
      ma4_sum += (blk.ribbon_volume * blk.ribbon_blue);
      ma4_vol_sum += blk.ribbon_volume;
    }
    uint64_t ma4;
    if(ma4_vol_sum > 0)
      ma4 = (ma4_sum / ma4_vol_sum);
    else 
      ma4 = 0;
    
    return (ma1 + ma2 + ma3 + ma4) / 4;
}


bool get_trades_from_ogre(std::vector<exchange_trade> *trades)
{
  std::string data = make_curl_http_get(std::string(TRADE_OGRE_API) + std::string("/history/BTC-XEQ"));
    
  rapidjson::Document document;
  document.Parse(data.c_str());
  for (size_t i = 0; i < document.Size(); i++)
  {
    exchange_trade trade;
    trade.date = document[i]["date"].GetUint64();
    trade.type = document[i]["type"].GetString();
    trade.price = std::stod(document[i]["price"].GetString()); // trade ogre gives this info as a string
    trade.quantity = std::stod(document[i]["quantity"].GetString());
    trades->push_back(trade);
  }
  
  return true;
}

bool get_trades_from_tritonex(std::vector<exchange_trade> *trades)
{
  std::string data = make_curl_http_get(std::string(TRITON_EX) + std::string("/get_trades"));
    
  rapidjson::Document document;
  document.Parse(data.c_str());
  for (size_t i = 0; i < document.Size(); i++)
  {
    exchange_trade trade;
    trade.date = std::stoull(document[i]["TimeStamp"].GetString());
    trade.type = document[i]["TradeType"].GetString();
    trade.price = std::stod(document[i]["Price"].GetString()); // tritonex gives this info as a string
    trade.quantity = std::stod(document[i]["Amount"].GetString());
    trades->push_back(trade);
  }
  
  return true;
}

bool get_orders_from_ogre(std::vector<exchange_order> *orders)
{
  std::string data = make_curl_http_get(std::string(TRADE_OGRE_API) + std::string("/orders/BTC-XEQ"));
    
  rapidjson::Document document;
  document.Parse(data.c_str());

  if(document.HasMember("buy")){
    const rapidjson::Value& buyJson = document["buy"];
    size_t get_top_25_orders = buyJson.Size() - 25;
    for (rapidjson::Value::ConstMemberIterator iter = buyJson.MemberBegin() + get_top_25_orders; iter != buyJson.MemberEnd(); ++iter)
    {
      exchange_order order;
      order.price = std::stod(iter->name.GetString());
      order.quantity = std::stod(iter->value.GetString());
      orders->push_back(order);
    }
  }  

  return true;
}

double get_coinbase_pro_btc_usd()
{
  std::string data = make_curl_http_get(std::string(COINBASE_PRO) + std::string("/products/BTC-USD/ticker"));
  rapidjson::Document document;
  document.Parse(data.c_str());

  double btc_usd = 0;
  for (size_t i = 0; i < document.Size(); i++)
  {  
    btc_usd = std::stod(document["price"].GetString());
  }
  return btc_usd;
}

double get_gemini_btc_usd()
{
  std::string data = make_curl_http_get(std::string(GEMINI_API) + std::string("/trades/btcusd?limit_trades=1"));
  rapidjson::Document document;
  document.Parse(data.c_str());
  double btc_usd = 0;
  for (size_t i = 0; i < document.Size(); i++)
  {
    btc_usd = std::stod(document[0]["price"].GetString());
  }
  return btc_usd;
}

double get_bitfinex_btc_usd()
{
  std::string data = make_curl_http_get(std::string(BITFINEX_API) + std::string("/pubticker/btcusd"));
  rapidjson::Document document;
  document.Parse(data.c_str());
  double btc_usd = 0;
  for (size_t i = 0; i < document.Size(); i++)
  {
    btc_usd = std::stod(document["last_price"].GetString());
  }
  return btc_usd;
}

double get_nance_btc_usd()
{
  std::string data = make_curl_http_get(std::string(NANCE_API) + std::string("/ticker/price?symbol=BTCUSDT"));
  rapidjson::Document document;
  document.Parse(data.c_str());
  double btc_usd = 0;
  for (size_t i = 0; i < document.Size(); i++)
  {
    btc_usd = std::stod(document["price"].GetString());
  }
  return btc_usd;
}

double get_stamp_btc_usd()
{
  std::string data = make_curl_http_get(std::string(STAMP_API) + std::string("/ticker/BTCUSD"));
  rapidjson::Document document;
  document.Parse(data.c_str());
  double btc_usd = 0;
  for (size_t i = 0; i < document.Size(); i++)
  {
    btc_usd = std::stod(document["last"].GetString());
  }
  return btc_usd;
}

double get_usd_average(){
  double gemini_usd = get_gemini_btc_usd();
  double coinbase_pro_usd = get_coinbase_pro_btc_usd();
  double bitfinex_usd = get_bitfinex_btc_usd();
  double nance_usd = get_nance_btc_usd();
  double stamp_usd = get_stamp_btc_usd();

  //Sometimes coinbase pro returns 0? Need to look into this.
  if(coinbase_pro_usd == 0)
    return (gemini_usd + bitfinex_usd + nance_usd + stamp_usd) / 4;

  return (gemini_usd + coinbase_pro_usd + bitfinex_usd + nance_usd + stamp_usd) / 5;
}



uint64_t convert_btc_to_usd(double btc)
{
	double usd_average = get_usd_average();
	double usd = usd_average * btc;
	return static_cast<uint64_t>(usd * 1000); // remove "cents" decimal place and convert to integer
}

uint64_t create_ribbon_blue(std::vector<exchange_trade> trades)
{
  double filtered_mean = filter_trades_by_deviation(trades);
  return convert_btc_to_usd(filtered_mean);
}

//Volume Weighted Average
uint64_t create_ribbon_green(std::vector<exchange_trade> trades){
  double weighted_mean = trades_weighted_mean(trades);
  return convert_btc_to_usd(weighted_mean);
}

uint64_t get_volume_for_block(std::vector<exchange_trade> trades){
  double volume = 0;
  if(trades.size() == 0)
    return 0;

  for(size_t i = 0; i < trades.size();i++){
    volume += (trades[i].price * trades[i].quantity);
  }
  std::cout << volume << std::endl;

  return convert_btc_to_usd(volume);
}


//Volume Weighted Average with 2 STDEV trades removed
double filter_trades_by_deviation(std::vector<exchange_trade> trades)
{
  double weighted_mean = trades_weighted_mean(trades);
  int n = trades.size();
  double sum = 0;
  
  for (size_t i = 0; i < trades.size(); i++)
  {
    sum += pow((trades[i].price - weighted_mean), 2.0);
  }
  
  double deviation = sqrt(sum / (double)n);
  
  double max = weighted_mean + (2 * deviation);
  double min = weighted_mean - (2 * deviation);
  
  for (size_t i = 0; i < trades.size(); i++)
  {
    if (trades[i].price > max || trades[i].price < min)
      trades.erase(trades.begin() + i);
  }

  return trades_weighted_mean(trades);
}

double trades_weighted_mean(std::vector<exchange_trade> trades)
{
  double XTRI_volume_sum = 0;
  double weighted_sum = 0;
  for (size_t i = 0; i < trades.size(); i++)
  {
    XTRI_volume_sum += trades[i].quantity;
    weighted_sum += (trades[i].price * trades[i].quantity);
  }
  
  return weighted_sum / XTRI_volume_sum;
}

std::vector<exchange_trade> get_recent_trades()
{
  std::vector<service_nodes::exchange_trade> trades;
  if(!service_nodes::get_trades_from_ogre(&trades))
    MERROR("Error getting trades from Ogre");

  if(!service_nodes::get_trades_from_tritonex(&trades))
    MERROR("Error getting trades from TritonEX");

  return trades;
}


std::vector<adjusted_liquidity> get_recent_liquids(double blue)
{
  std::vector<exchange_order> orders;
  if(!get_orders_from_ogre(&orders))
    MERROR("Error getting orders from TradeOgre");
  //more exchanges below
  std::vector<adjusted_liquidity> adj_liquid = create_adjusted_liqudities(orders, blue);
  return adj_liquid;
}


std::vector<adjusted_liquidity> create_adjusted_liqudities(std::vector<exchange_order> orders, double spot){
  std::vector<adjusted_liquidity> al;

  for(size_t i = 0; i < orders.size();i++){
      adjusted_liquidity this_al;
      if(orders[i].price != spot){
        this_al.price = orders[i].price;
        double denom = (1 - std::abs(this_al.price - spot));
        this_al.liquid = (orders[i].quantity) * (1 / denom);
        al.push_back(this_al);
      }
  }

  return al;
}

double create_mac(std::vector<adjusted_liquidity> adj_liquids){
  double adj_liquid_sum = 0;
  double price_weighted = 0;

  for(size_t i = 0; i < adj_liquids.size(); i++){
    adj_liquid_sum += adj_liquids[i].liquid;
    price_weighted += (adj_liquids[i].liquid * adj_liquids[i].price);
  }
  return price_weighted * adj_liquid_sum;
}

}
