#pragma once

#include <string>
#include <vector>

#include "lquant_data_api.hpp"
#include "lquant_base_univ.hpp"
#include "lquant_base_trade.hpp"
#include "lquant_module_handler.hpp"
#include "xml_parser.hpp"

////////////////////////////////////////////////////////////////////////////////////////
class lquant_base_model {
protected:
  lquant::XmlNode cfg;
  int freq;
  float *alpha;
  float *alpha_preop;

public:
  virtual bool push_interval_data(long didx, long tidx) = 0;
  virtual float *get_forecasts() {
    return alpha;
  }
  virtual float *get_forecasts_preop() {
    return alpha_preop;
  }
  int get_freq() const {return freq;};

  lquant_base_model(const lquant::XmlNode &_cfg):cfg(_cfg) {
    freq = cfg.getAttrDefault("freq", 240);

    lquant_data_api *dpi  = lquant_data_api::getInstance();
    lquant_base_univ *univ = dpi->get_univ();
    vector<string> tkrs = univ->get_tkrs();

    alpha       = new float[tkrs.size()];
    alpha_preop = new float[tkrs.size()];
    const float NAN_float = numeric_limits<float>::quiet_NaN();
    for (int i = 0; i < (int)tkrs.size(); i++) {
      alpha[i]       = NAN_float;
      alpha_preop[i] = NAN_float;
    }
  }
  virtual ~lquant_base_model() {
    if(alpha) {
      delete alpha; alpha = NULL;
    }
    if(alpha_preop) {
      delete alpha_preop; alpha_preop = NULL;
    }
  }


};

class lquant_model_handler : lquant_base_module_handler {
public:
  lquant_base_model * create(string id, string path, const lquant::XmlNode &cfg) {
    void *pHandle = load_module(id, path);
    lquant_base_model * (* create)(const lquant::XmlNode &) = (lquant_base_model * (*)(const lquant::XmlNode &))dlsym(pHandle, "create");
    return create(cfg);
  }
};
////////////////////////////////////////////////////////////////////////////////////////
class lquant_base_operation {
protected:
  lquant::XmlNode cfg;

public:
  lquant_base_operation(const lquant::XmlNode &_cfg):cfg(_cfg) { }
  virtual ~lquant_base_operation() { }

  virtual bool apply(float *alpha, long didx, long tidx) = 0;



};

class lquant_operation_handler : lquant_base_module_handler {
public:
  lquant_base_operation * create(string id, string path, const lquant::XmlNode &cfg) {
    void *pHandle = load_module(id, path);
    lquant_base_operation * (* create)(const lquant::XmlNode &) = (lquant_base_operation * (*)(const lquant::XmlNode &))dlsym(pHandle, "create");
    return create(cfg);
  }
};

////////////////////////////////////////////////////////////////////////////////////////
struct lquant_alpha_node;

class lquant_base_combo {
protected:
  lquant_alpha_node *node;
  int freq;
public:
  lquant_base_combo(lquant_alpha_node *node);
  virtual ~lquant_base_combo() { }
  int get_freq() const {return freq;};
  virtual bool combine(long didx, long tidx) = 0;
  virtual float *get_forecasts() = 0;

};

class lquant_combo_handler : lquant_base_module_handler {
public:
  lquant_base_combo * create(string id, string path, lquant_alpha_node *node);
};

////////////////////////////////////////////////////////////////////////////////////////
class lquant_base_data_calcor {
protected:
  lquant::XmlNode cfg;

public:
  virtual bool push_interval_data(long didx, long tidx) = 0;

  lquant_base_data_calcor(const lquant::XmlNode &_cfg):cfg(_cfg) { }
  virtual ~lquant_base_data_calcor() { }
};
class lquant_calcor_handler : lquant_base_module_handler {
public:
  lquant_base_data_calcor * create(string id, string path, const lquant::XmlNode &cfg) {
    void *pHandle = load_module(id, path);
    lquant_base_data_calcor * (* create)(const lquant::XmlNode &) = (lquant_base_data_calcor * (*)(const lquant::XmlNode &))dlsym(pHandle, "create");
    return create(cfg);
  }
};

////////////////////////////////////////////////////////////////////////////////////////
class lquant_base_trader {
public:
  lquant_base_trader(lquant_base_port *port, lquant::XmlNode &cfg, lquant_alpha_node *alphaNode)
    :port(port),cfg(cfg),pAlphaNode(alphaNode) {
    
  }
  virtual ~lquant_base_trader() { };
  
  virtual bool push_interval_data(long didx, long tidx) = 0;
  void set_cb_orderinsert(OrderInsertCallback cb_or) {cb_OrderInsert = cb_or;}
  void set_cb_orderinfo(OrderInfoCallback cb_oi)     {cb_OrderInfo   = cb_oi;}
  void set_cb_ordercancel(OrderCancelCallback cb_oc) {cb_OrderCancel = cb_oc;}

protected:
  lquant_base_port   *port;
  lquant::XmlNode cfg;
  lquant_alpha_node  *pAlphaNode;
  
  OrderInsertCallback     cb_OrderInsert;
  OrderInfoCallback       cb_OrderInfo;
  OrderCancelCallback     cb_OrderCancel;
};

class lquant_trader_handler : lquant_base_module_handler {
public:
  lquant_base_trader * create(string id, string path, lquant_base_port *port, lquant::XmlNode &cfg, lquant_alpha_node *node) {
    void *pHandle = load_module(id, path);
    lquant_base_trader * (* create)(lquant_base_port *, lquant::XmlNode &, lquant_alpha_node *) 
      = (lquant_base_trader * (*)(lquant_base_port *, lquant::XmlNode &, lquant_alpha_node *))dlsym(pHandle, "create");
    return create(port, cfg, node);
  }
};
