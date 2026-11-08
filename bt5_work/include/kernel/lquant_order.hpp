#pragma once

#include <string>

#pragma pack(push, 1)

struct lquant_porder_t {
  lquant_porder_t()
    :tkr(""),side('?'),ocflag('?'),dqty(0),dprc(0),eqty(0),eval(0),dte(0),tme(0),op('O'),strtname(""),teid(0),id(0) { 
  }

  std::string tkr;
  char side;
  char ocflag;
  int dqty;
  int dprc;
  int eqty;
  float eval;
  int dte;
  int tme;
  char op;
  std::string strtname;
  int teid;
  int id;
};

#pragma pack(pop)
