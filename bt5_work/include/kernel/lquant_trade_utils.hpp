#pragma once
#include <unordered_map>
#include <vector>
#include <mutex>
#include "lquantOrder.hpp"
#include "xml_parser.hpp"
#include "lquant_log.hpp"

class lquant_trade_utils {

public:
  lquant_trade_utils(lquant::XmlNode &cfg);
  virtual ~lquant_trade_utils();
  virtual int Init() { return 0; };
  // /** take actions */
  // int insert_market_order(short source, string instrument_id, string exchange_id, int volume, LfDirectionType direction, LfOffsetFlagType offset);
  virtual int insert_limit_order(const std::string& instrument_id, const std::string &exchange_id, double price, int volume, order_direction_t direction, order_flag_t offset) { return 0; };
  // int insert_fok_order(short source, string instrument_id, string exchange_id, double price, int volume, LfDirectionType direction, LfOffsetFlagType offset);
  // int insert_fak_order(short source, string instrument_id, string exchange_id, double price, int volume, LfDirectionType direction, LfOffsetFlagType offset);
  virtual std::vector<lquant_order_t *> req_outside_order(const std::string &instrument_id);
  virtual lquant_holdings_t *req_position(const std::string &instrument_id);
  virtual lquant_account_t *req_account();
  virtual int cancel_order(int order_id) { return 0; };
  virtual std::string getLastMsg() { return lastMsg; };

  void dump(lquant_holdings_t *holding);
  void dump(lquant_order_t *order);
  void dump(lquant_account_t *acc);

public:
  std::unordered_map<long, lquant_order_t *> orderMap;  // orderid - order
  std::unordered_map<std::string, lquant_holdings_t *> positions; // 持仓
  std::string accountid;
  std::string passwd;
  std::string server;
  lquant_account_t* accountinfo;
  std::string lastMsg;
  std::mutex ordLock; // orderMap lock
  std::mutex posLock; // position lock
  // /** basic utilities */
  // /** nano time */
  // long   get_nano();
  // /** get string time with format "%Y-%m-%d %H:%M:%S" */
  // string get_time();
  // /** parse time */
  // long   parse_time(string time_str);
  // /** parse nano */
  // string parse_nano(long nano);
};

 