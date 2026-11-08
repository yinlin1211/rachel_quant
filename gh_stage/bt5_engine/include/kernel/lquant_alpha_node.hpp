#pragma once

#include "lquant_base_model.hpp"
#include "lquant_base_stats.hpp"

struct lquant_alpha_node {
  //reference to Alpha/Combo/Intraday/Stats/Cfg modules
  lquant_base_model         *model;
  lquant_base_combo         *combo;
  lquant_base_trade         *trade;
  vector<lquant_alpha_node *> children;
  vector<lquant_base_operation *> **operations;
  vector<lquant_base_stats *> statses;

  lquant::XmlNode        cfg;
  lquant::XmlNode        gcfg;

  float                   booksize;
  
  ///////////////
  bool                    bRoot;
  //lquant::XmlNode        config;
  string                  id;
  int                     delay;
  int                     mdelay; //minutes delay
  string                  universeId;
  string                  modelModuleId;
  string                  combinationId;
  string                  tradeModuleId;
  string                  traderModuleId;
  vector<string>          statsModuleIds;
  float                  *alpha;
  float                  *alpha_preop;
  float                   weight;
  
  bool                    bDump;
  bool                    bNeed;
  string                  sDumpDir;
  lquant_data_writer<float> *writer;
  lquant_data_reader<float> *reader;
  ///////////////

  lquant_alpha_node():booksize(20.e6),bRoot(false),id(""),universeId(""),bDump(false),bNeed(true) {
    model = NULL;
    combo = NULL;
  }
};


