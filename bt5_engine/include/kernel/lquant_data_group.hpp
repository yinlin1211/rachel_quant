#pragma once

#include <map>
#include <mutex>

class lquant_data_group {
  map<string, void *> dmap;
  std::mutex mtx;

public:
  lquant_data_group() { }
  ~lquant_data_group() { }

  inline
  void *get_data(string key) {
    mtx.lock();
    auto itr = dmap.find(key);
    mtx.unlock();
    return itr == dmap.end() ? NULL : itr->second;
  }

  inline
  int add_data(string key, void *data) {
    mtx.lock();
    dmap[key] = data;
    mtx.unlock();
    return 0;
  }

};
