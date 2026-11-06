#pragma once

#include "lquant_order.hpp"
#include "xml_parser.hpp"
#include "lquant_utils.hpp"

class lquant_base_exch {
protected:
  virtual void on_bmo() = 0;
  virtual void on_amc() = 0;
  virtual void on_recv_porder(const lquant_porder_t&) = 0;

public:
  lquant_base_exch(lquant::XmlNode &_cfg):cfg(_cfg) { }
  virtual ~lquant_base_exch() { }
private:
  lquant::XmlNode cfg;
};

class lquant_exch_simulation : public lquant_base_exch {
public:
  lquant_exch_simulation(lquant::XmlNode &_cfg):lquant_base_exch(_cfg) {
    ;
  }
  virtual ~lquant_exch_simulation() { }
	
	
  void on_recv_porder(const lquant_porder_t& po) {

  }

  void on_bmo() {

  }

  void on_amc() {

  }
};

class lquant_exch_online : public lquant_base_exch {
public:
  lquant_exch_online(lquant::XmlNode &_cfg):lquant_base_exch(_cfg) {
    ;
  }
  virtual ~lquant_exch_online() { }
	
	
  void on_recv_porder(lquant_porder_t& po) {

  }

  void on_bmo() {

  }

  void on_amc() {

  }
};
