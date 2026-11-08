#pragma once

#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include "lquant_base_model.hpp"
#include "lquant_base_stats.hpp"
#include "lquant_base_trade.hpp"
#include "lquant_data_writer.hpp"
#include "lquant_data_reader.hpp"
#include "xml_parser.hpp"


using namespace std;

struct lquant_porder_t;
typedef function<void (const lquant_porder_t&)> POCallback;

struct lquant_alpha_node;

class lquant_base_port {
public:
  lquant_base_port(const lquant::XmlNode &_cfg);
  virtual ~lquant_base_port();
  void set_onporder(POCallback pf);
  virtual void on_bars(long didx, long tidx);
  bool dump();

protected:
  bool build_tree(const lquant::XmlNode &cfg, lquant_alpha_node *panode);
  bool generate_alpha_node(const lquant::XmlNode &cfg, lquant_alpha_node *panode);
  bool generate_alphas_node(const lquant::XmlNode &cfg, lquant_alpha_node *panode);
  bool generate_operations_node(const lquant::XmlNode &cfg, lquant_alpha_node *panode);

protected:
  lquant::XmlNode gcfg;
  lquant_data_api *_dpi;
  lquant_base_univ *_univ;
  vector<string> _tkrs;
  int    nbar;
  string dataDir;
  float slippage;
  POCallback pocb;

  vector<lquant_alpha_node *> alphaNodes;
  vector<lquant_base_data_calcor *> dataCalcors;
  set<string> sysIds;
  
public:
  lquant_alpha_node *alpha_root;
};

////////////////////////////////////////////////////////////////////////////////////////
class lquant_port_handler : lquant_base_module_handler {
public:
  lquant_base_port * create(string id, string path, const lquant::XmlNode &cfg) {
    void *pHandle = load_module(id, path);
    lquant_base_port * (* create)(const lquant::XmlNode &) = (lquant_base_port * (*)(const lquant::XmlNode &))dlsym(pHandle, "create");
    return create(cfg);
  }
};
