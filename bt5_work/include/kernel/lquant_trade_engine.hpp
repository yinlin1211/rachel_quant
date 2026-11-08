#pragma once

#include "lquant_base_port.hpp"
#include "lquant_base_exch.hpp"
#include "lquant_base_datafeed.hpp"
#include "lquant_module_handler.hpp"
#include "xml_parser.hpp"
#include "lquant_utils.hpp"




class lquant_trade_engine {
public:
  lquant_trade_engine(const lquant::XmlNode& _cfg);
  ~lquant_trade_engine();

  bool init();
  bool on_bmo();
  bool on_amc();
  bool run();
  
  void on_bars(long didx, long tidx);

protected:
  void on_recv_porder(const lquant_porder_t &po) {
    if(toSim) {
      send_po_to_sim_exch(po);
    }
    else {
      send_po_to_online_exch(po);
    }
  }

private:
  void send_po_to_sim_exch(const lquant_porder_t & po) {
    exch->on_recv_porder(po);
  }
	
  void send_po_to_online_exch(const lquant_porder_t &po) {
		
  }

  bool load_datafeed();
  bool load_port();

private:
  lquant::XmlNode cfg;
  string runMode;
  bool toSim;
  bool running;
  int  startDateIdx;
  int  endDateIdx;
  int startDate;
  int endDate;
  
  lquant_base_port *port;
  lquant_exch_simulation *exch;
  lquant_base_datafeed *feed;
};



