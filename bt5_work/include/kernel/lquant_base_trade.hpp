#pragma once

#include <vector>


#include "lquant_data_api.hpp"
#include "lquant_data_reader.hpp"
#include "lquant_data_writer.hpp"
#include "xml_parser.hpp"
#include "lquant_utils.hpp"
#include "lquantOrder.hpp"
#include "lquant_module_handler.hpp"

struct lquant_alpha_node;
class lquant_base_port;
struct lquant_order_t;
class lquant_base_trader;
typedef function<lquant_order_t* (int)>      OrderInfoCallback;
typedef function<int (lquant_signal_info&)>  OrderInsertCallback;
typedef function<int (int)>               OrderCancelCallback;

/*******************************************
 * 处理多只证券的买卖交易，由行情、订单驱动
 * 输入：行情、预测信号
 * 输出：挂单、撤单
 *******************************************/
class lquant_base_trade {
public:
  lquant_base_trade(lquant_base_port *port, lquant::XmlNode &cfg, lquant_alpha_node *alphaNode);
  virtual ~lquant_base_trade();

  virtual bool push_interval_data(long didx, long tidx);
  virtual bool dump();
  virtual int             req_order_insert(lquant_signal_info& signal) {return -1;}
  virtual lquant_order_t*    req_order_info(int orderId) {return nullptr;}
  virtual lquant_holdings_t* req_holding_info(string instrumentId) {return nullptr;}

protected:
  lquant_base_port   *port;
  lquant::XmlNode cfg;
  lquant_alpha_node  *pAlphaNode;
};

class lquant_simple_trade : public lquant_base_trade {
public:
  lquant_simple_trade(lquant_base_port *port, lquant::XmlNode &cfg, lquant_alpha_node *alphaNode);
  virtual ~lquant_simple_trade();
  virtual bool push_interval_data(long didx, long tidx);

protected:
  bool on_bmo();
  bool on_amc();

  void sendExchOrder(lquant_order_t &order);
  bool onRecvExchOrder(lquant_order_t *order);

  virtual bool loop_processing();

protected:
  //lquant_trader *getTraderByIdnum(int idnum);

protected:
  bool running;

  lquant_data_api  *_dpi;
  lquant_base_univ *_univ;
  vector<string> _tkrs;

  //lquant_trader   **traders;

  int curOrdIdx;

  lquant_data_reader<lquant_order_t> *replyData;
  lquant_data_writer<lquant_order_t> *orderData;
};

////////////////////////////////////////////////////////////////////////////////////////
class lquant_trade_handler : lquant_base_module_handler {
public:
  lquant_base_trade * create(string id, string path, lquant_base_port *port, lquant::XmlNode &cfg, lquant_alpha_node *node) {
    void *pHandle = load_module(id, path);
    lquant_base_trade * (* create)(lquant_base_port *, lquant::XmlNode &, lquant_alpha_node *) 
      = (lquant_base_trade * (*)(lquant_base_port *, lquant::XmlNode &, lquant_alpha_node *))dlsym(pHandle, "create");
    return create(port, cfg, node);
  }
};


