#include "lquantTick.hpp"
#include "xml_parser.hpp"

class lquant_base_HHL2TickCalcor {
public:
  virtual int process_code_table(lquant_msg_head_t *head, char *data){return 0;}; //
  virtual int process_market_data(lquant_market_data_t * md){return 0;};
  virtual int process_market_data_L1(lquant_msg_head_t *head, char *data, lquant_quote_src_t src){return 0;};
  virtual int process_market_data_futures(lquant_msg_head_t *head, char *data, lquant_quote_src_t src) {return 0;};
  virtual int process_transaction(lquant_msg_head_t *head, char *data, lquant_quote_src_t src) {return 0;};
  virtual bool save() {return true;};

  lquant_base_HHL2TickCalcor(lquant::XmlNode &cfg):cfg(cfg){ };
protected:
  lquant::XmlNode cfg;
};
