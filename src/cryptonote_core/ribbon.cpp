#include <vector>
#include <curl/curl.h>
#include <iostream>
#include <math.h>

#include "rapidjson/document.h"
#include "blockchain.h"
#include "ribbon.h"

namespace service_nodes {

cryptonote::Blockchain* m_blockchain_storage;

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

std::vector<exchange_trade> get_trades_from_ogre()
{
  std::string data = make_curl_http_get(std::string(TRADE_OGRE_API) + std::string("/history/BTC-XTRI"));
    
  rapidjson::Document document;
  document.Parse(data.c_str());
  std::vector<exchange_trade> trades;
  for (size_t i = 0; i < document.Size(); i++)
  {
    exchange_trade trade;
    trade.date = document[i]["date"].GetUint64();
    trade.type = document[i]["type"].GetString();
    trade.price = std::stod(document[i]["price"].GetString()); // trade ogre gives this info as a string
    trade.quantity = std::stod(document[i]["quantity"].GetString());
    trades.push_back(trade);
  }
  
  return trades;
}

std::vector<exchange_trade> get_trades_from_tritonex(std::vector<exchange_trade> trades)
{
  std::string data = make_curl_http_get(std::string(TRITON_EX) + std::string("/get_trades"));
    
  rapidjson::Document document;
  document.Parse(data.c_str());
  for (size_t i = 0; i < document.Size(); i++)
  {
    exchange_trade trade;
    trade.date = document[i]["TimeStamp"].GetString();
    trade.type = document[i]["TradeType"].GetString();
    trade.price = std::stod(document[i]["Price"].GetString()); // tritonex gives this info as a string
    trade.quantity = std::stod(document[i]["Amount"].GetString());
    trades.push_back(trade);
  }
  
  return trades;
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

std::vector<exchange_trade> trades_during_latest_1_block(std::vector<exchange_trade> trades, cryptonote::Blockchain* chain)
{
  uint64_t top_block_height = chain->get_current_blockchain_height() - 1;
  crypto::hash top_block_hash = chain->get_block_id_by_height(top_block_height);
  cryptonote::block top_blk;
  chain->get_block_by_hash(top_block_hash, top_blk);
  uint64_t top_block_timestamp = top_blk.timestamp;

 uint64_t start_block_height = chain->get_current_blockchain_height() - 2;
  crypto::hash start_block_hash = chain->get_block_id_by_height(start_block_height);
  cryptonote::block start_blk;
  chain->get_block_by_hash(start_block_hash, start_blk);
  uint64_t start_block_timestamp = start_blk.timestamp;

  std::vector<exchange_trade> result;
  for (size_t i = 0; i < trades.size(); i++)
  {
    if (trades[i].date >= start_block_timestamp && trades[i].date <= top_block_timestamp){
      result.push_back(trades[i]);
    }
  }
  return result;
}


double get_usd_average(double gemini_usd, double coinbase_pro_usd){
  return (gemini_usd + coinbase_pro_usd) / 2;
}

double price_over_x_blocks(int blocks){
  double ribbon_blue_sum = 0;
  uint64_t top_block_height = m_blockchain_storage->get_current_blockchain_height() - 1;

  for(size_t i = 1; i > blocks - blocks;i++){
    uint64_t this_top_block_height = m_blockchain_storage->get_current_blockchain_height() - i;
    crypto::hash this_block_hash = m_blockchain_storage->get_block_id_by_height(this_top_block_height);
    cryptonote::block this_blk;
    m_blockchain_storage->get_block_by_hash(this_block_hash, this_blk);
    std::string::size_type size;
    double blk_rb = 0;
    ribbon_blue_sum += blk_rb;
  }

  return ribbon_blue_sum / blocks;
}

double create_ribbon_red(){
  double ma_960 = price_over_x_blocks(960);
  double ma_480 = price_over_x_blocks(480);
  double ma_240 = price_over_x_blocks(240);
  double ma_120 = price_over_x_blocks(120);

  return (ma_960 + ma_480 + ma_240 + ma_120) / 4;
}

double create_ribbon_blue(std::vector<exchange_trade> trades)
{
  return filter_trades_by_deviation(trades);
}

//Volume Weighted Average
double create_ribbon_green(std::vector<exchange_trade> trades){
  return trades_weighted_mean(trades);
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

}
