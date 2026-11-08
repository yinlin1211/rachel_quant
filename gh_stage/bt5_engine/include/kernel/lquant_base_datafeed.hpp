#pragma once

#include "lquant_data_api.hpp"
#include "lquant_module_handler.hpp"

class lquant_base_datafeed {
public:
  lquant_base_datafeed(const lquant::XmlNode &_cfg);

  virtual ~lquant_base_datafeed() { }

  virtual bool init() {
    return true;
  }
  virtual bool on_bmo()  {
    return true;
  }
  virtual bool on_amc()  {
    return true;
  }

  virtual bool run() {
    return true;
  }
  
  void set_onbars(callback pf) {
    onbars = pf;
  }

protected:
  lquant_data_api *dpi;
  long begDate;
  long endDate;
  string runMode;
  callback onbars;
  const lquant::XmlNode cfg;
};

////////////////////////////////////////////////////////////////////////////////////////
class lquant_datafeed_handler : lquant_base_module_handler {
public:
  lquant_base_datafeed * create(string id, string path, const lquant::XmlNode &cfg) {
    void *pHandle = load_module(id, path);
    lquant_base_datafeed * (* create)(const lquant::XmlNode &) = (lquant_base_datafeed * (*)(const lquant::XmlNode &))dlsym(pHandle, "create");
    return create(cfg);
  }
};

