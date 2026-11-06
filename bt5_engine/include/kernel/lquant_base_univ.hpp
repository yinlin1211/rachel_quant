#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <set>
#include <map>
#include <vector>
#include <cstdlib>
#include "xml_parser.hpp"
#include "lquant_utils.hpp"

using namespace std;

struct lquant_instrument {
  lquant_instrument():cid(-1),valid(false),tkr(""),exch(""),multiplier(0),marginrate(0) { }

  int    cid;
  bool   valid;
  string tkr;
  string exch;
  float multiplier;
  float marginrate;
};

struct lquant_index_instrument {
  lquant_index_instrument():cid(-1),tkr("") { }

  int    cid;
  string tkr;
};

class lquant_base_univ {
protected:
  lquant::XmlNode cfg;

  vector<lquant_instrument> insts;
  map<string,int>  mp;
  lquant_instrument   noInst;

  vector<lquant_index_instrument> index_insts;
  map<string,int>  index_mp;
  lquant_index_instrument   index_noInst;

protected:
  lquant_base_univ() {}

public:
  lquant_base_univ(const lquant::XmlNode &_cfg):cfg(_cfg) {}
  virtual ~lquant_base_univ() { }

  vector<string> get_tkrs() const {
    vector<string> rv;
    for(auto itr=insts.begin();itr!=insts.end();++itr) {
      if(itr->valid) {
	rv.push_back(itr->tkr);
      }
    }
    return rv;
  }

  vector<string> get_index_tkrs() const {
    vector<string> rv;
    for(auto itr=index_insts.begin();itr!=index_insts.end();++itr) {
      rv.push_back(itr->tkr);
    }
    return rv;
  }

  const lquant_instrument &get_instrument(string tkr){
    if(mp.find(tkr) == mp.end()) {
      return noInst;
    }
    return insts[mp[tkr]];
  }

  const lquant_instrument &get_instrument(int cidx){
    if(cidx >= (int)insts.size()) {
      return noInst;
    }
    return insts[cidx];
  }

  const lquant_index_instrument &get_index_instrument(string tkr){
    if(index_mp.find(tkr) == index_mp.end()) {
      return index_noInst;
    }
    return index_insts[index_mp[tkr]];
  }

  const lquant_index_instrument &get_index_instrument(int cidx){
    if(cidx >= (int)index_insts.size()) {
      return index_noInst;
    }
    return index_insts[cidx];
  }
};

class lquant_simple_univ:public lquant_base_univ {
private:
  lquant_simple_univ() { }
public:
  lquant_simple_univ(const lquant::XmlNode &cfg);
  ~lquant_simple_univ();
};


