#pragma once

#include <string>
#include "lquant_digital_currency.hpp"
#include "lquant_data_reader.hpp"


enum GDataCrypToType {
  CRY_TYPE_MD = 0,
  CRY_TYPE_TS = 1,
  CRY_TYPE_OD = 2,
  CRY_TYPE_MDF = 3,
  CRY_TYPE_INDEX = 4,
  CRY_TYPE_UNKOWN = 255,
};

union GDataCrypToValue {
  digital_currency_depth_t *md;
  digital_currency_trade_t *ts;
  void *UNK;
};

struct crypto_ticker_node {
  long offset[100000];
  int  last_idx;
  int  max_idx;

  inline
  long get_offset(int idx) {
    return offset[idx/CHUNK_SIZE] + (idx%CHUNK_SIZE)*sizeof(long);
  }

  inline
  long get_occupied() {
    return (last_idx/CHUNK_SIZE + 1) * sizeof(long) * CHUNK_SIZE;
  }
};

struct lquant_crypto_tick_pool_meta {
  long last_didx;
  int ntkr;
  long tsnodes_offset;
  long mdnodes_offset;

  long gindex_offset;
};


class lquant_crypto_tick_pool {
private:
  lquant_data_reader<char> *filedata;
  const char *data;

  lquant_crypto_tick_pool_meta *meta;
  unsigned long *gindex;
  
private:
  void reset();

private:
  int *pindex;

public:
  lquant_crypto_tick_pool();
  ~lquant_crypto_tick_pool();
  bool load(const char *fname);
  bool load_fut(const char *fname);

public:
  long get_last_didx();
  int get_fut_last_didx();
  tuple<unsigned int,unsigned int,GDataCrypToType,GDataCrypToValue> get_data(long gidx);


  digital_currency_trade_t *get_ts(int cidx, int idx);
  int get_ts_count(int cidx);

  digital_currency_depth_t *get_md(int cidx, int idx);
  int get_md_count(int cidx);


  template <typename T> 
  T* get_data(int tidx){
     return ((T*)data)+tidx;
  };
};
