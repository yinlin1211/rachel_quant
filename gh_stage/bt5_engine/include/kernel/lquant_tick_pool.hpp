#pragma once

#include <string>
#include "lquantTick.hpp"
#include "lquant_data_reader.hpp"


enum GDataType {
  TYPE_MD = 0,
  TYPE_TS = 1,
  TYPE_OD = 2,
  TYPE_MDF = 3,
  TYPE_INDEX = 4,
  TYPE_UNKOWN = 255,
};

union GDataValue {
  lquant_market_data_t *md;
  lquant_transaction_t *ts;
  lquant_tdf_order_t   *od;
  lquant_market_data_futures_t *mdf;
  lquant_index_t       *indx;
  void *UNK;
};

struct ticker_node {
  long offset[1000];
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

struct lquant_tick_pool_meta {
  long last_didx;
  int ntkr;
  long tsnodes_offset;
  long mdnodes_offset;
  long odnodes_offset;
  long indxnodes_offset;
  long mdfnodes_offset;

  long gindex_offset;
};


class lquant_tick_pool {
private:
  lquant_data_reader<char> *filedata;
  const char *data;

  lquant_tick_pool_meta *meta;
  unsigned long *gindex;
  
private:
  void reset();

private:
  int *pindex;

public:
  lquant_tick_pool();
  ~lquant_tick_pool();
  bool load(const char *fname);
  bool load_fut(const char *fname);

public:
  long get_last_didx();
  int get_fut_last_didx();
  tuple<unsigned int,unsigned int,GDataType,GDataValue> get_data(long gidx);

  lquant_market_data_futures_t *get_mdf(int cidx, int idx);
  int get_mdf_count(int cidx);

  lquant_transaction_t *get_ts(int cidx, int idx);
  int get_ts_count(int cidx);

  lquant_market_data_t *get_md(int cidx, int idx);
  int get_md_count(int cidx);

  lquant_tdf_order_t *get_od(int cidx, int idx);
  int get_od_count(int cidx);

  lquant_index_t *get_indx(int cidx, int idx);
  int get_indx_count(int cidx);

  template <typename T> 
  T* get_data(int tidx){
     return ((T*)data)+tidx;
  };
};
