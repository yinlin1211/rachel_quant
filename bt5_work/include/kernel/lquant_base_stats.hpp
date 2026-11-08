#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <set>
#include <map>


#include "lquant_module_handler.hpp"
#include "xml_parser.hpp"
#include "lquant_utils.hpp"

struct lquant_alpha_node;
class lquant_base_port;

class lquant_base_stats {
public:
  lquant_base_stats(lquant_base_port *port, lquant::XmlNode &cfg, lquant_alpha_node *alphaNode)
    :port(port),cfg(cfg),pAlphaNode(alphaNode) {

  }
  virtual ~lquant_base_stats() { };
  
  virtual bool calculate(long didx, long tidx) {
    return true;
  }
 
protected:
  lquant_base_port *port;
  lquant::XmlNode cfg;
  lquant_alpha_node *pAlphaNode;
};

////////////////////////////////////////////////////////////////////////////////////////
class lquant_stats_handler : lquant_base_module_handler {
public:
  lquant_base_stats * create(string id, string path, lquant_base_port *port, lquant::XmlNode &cfg, lquant_alpha_node *node) {
    void *pHandle = load_module(id, path);
    lquant_base_stats * (* create)(lquant_base_port *, lquant::XmlNode &, lquant_alpha_node *) 
      = (lquant_base_stats * (*)(lquant_base_port *, lquant::XmlNode &, lquant_alpha_node *))dlsym(pHandle, "create");
    return create(port, cfg, node);
  }
};


/********************************************************************
 * 输出三类信息：
 *  每个alpha的pnl信息
 *  每个股票的pnl信息
 *  eod pnl信息（end of day）
 ********************************************************************/
class lquant_simple_stats : public lquant_base_stats {
public:
  lquant_simple_stats(lquant_base_port *port, lquant::XmlNode &cfg, lquant_alpha_node *panode);
  virtual ~lquant_simple_stats();
  
  virtual bool calculate(long didx, long tidx);
  
private:
  string pnlDir;
  FILE *eodf;
};
