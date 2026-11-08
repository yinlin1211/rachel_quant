#pragma once

#include <string>  
#include <unordered_map>
#include "lquant_base_univ.hpp"
#include "lquant_data.hpp"
#include "lquant_data_group.hpp"
using namespace std;

typedef function<void (long,long)> callback;


class lquant_tick_pool;
class lquant_crypto_tick_pool;
struct lquant_data_meta {
  int dn, tn, cn, mn, index_cn;
  //int daily_nrow,daily_ncol;
  int last_didx_offset;
};

struct lquant_hold_rank{
  char   tkr[12];
  char   membername[32];
  int    type;
  float  positionsnum;
  int    rank;
  float  positionsnumc;
};

class lquant_data_api {
private:
  static lquant_data_api *pInstance;
  lquant_data_api() { }
  lquant_data_api(const lquant::XmlNode &cfg);
  virtual ~lquant_data_api();

public:
  static lquant_data_api *initializeInstance(const lquant::XmlNode &cfg) {
    if(NULL == pInstance) {
      pInstance = new lquant_data_api(cfg);
    }
    return pInstance;
  }

  static void destroy() {
    if(pInstance) {
      delete pInstance;
      pInstance = NULL;
    }
  }

  static lquant_data_api *getInstance() {
    return pInstance;
  }

private:
  string dataDir;
  vector<string> tkrs;
  vector<string> index_tkrs;
  vector<long> time_series;
  vector<long> daily_time_series;
  unordered_map<string,lquant_data<float>*> fmap;
  unordered_map<string,lquant_data<double>*> dmap;
  unordered_map<string,lquant_data<long>*> lmap;
  unordered_map<string,lquant_data<int>*> imap;
  unordered_map<string,lquant_data<bool>*> bmap;
  unordered_map<string,lquant_data<char>*> cmap;
  unordered_map<string,lquant_data<float>*> daily_fmap;
  unordered_map<string,lquant_data<double>*> daily_dmap;
  unordered_map<string,lquant_data<long>*> daily_lmap;
  unordered_map<string,lquant_data<int>*> daily_imap;
  unordered_map<string,lquant_data<bool>*> daily_bmap;
  unordered_map<string,lquant_data<char>*> daily_cmap;

  unordered_map<string,lquant_data<float>*> f_index_map;
  unordered_map<string,lquant_data<double>*> d_index_map;
  unordered_map<string,lquant_data<long>*> l_index_map;
  unordered_map<string,lquant_data<int>*> i_index_map;
  unordered_map<string,lquant_data<bool>*> b_index_map;
  unordered_map<string,lquant_data<char>*> c_index_map;
  unordered_map<string,lquant_data<float>*> daily_f_index_map;
  unordered_map<string,lquant_data<double>*> daily_d_index_map;
  unordered_map<string,lquant_data<long>*> daily_l_index_map;
  unordered_map<string,lquant_data<int>*> daily_i_index_map;
  unordered_map<string,lquant_data<bool>*> daily_b_index_map;
  unordered_map<string,lquant_data<char>*> daily_c_index_map;

  lquant_data<lquant_hold_rank>* hold_rank;
  unordered_map<string,string> datatype_map;
  unordered_map<string,int> daily_dimension_map;
  unordered_map<string,int> mins_dimension_map;
  lquant_base_univ *univ;
  int *cidx;
  int *index_cidx;
  int nrow, ncol;

  lquant_data<char>* meta_data;
  lquant_data_meta *meta;
  int *last_didx;
  int *tick_last_didx;
  int *last_didx_vec;
  
  lquant_tick_pool **pools;
  lquant_crypto_tick_pool **coinpools;
  lquant_tick_pool ***ctapools;

  lquant_data_group dgr;

private:
  void load_meta_file(string datapath);
  void load_daily_data_file(string datapath);
  void load_data_file(string datapath);
  void load_daily_index_data_file(string datapath);
  void load_index_data_file(string datapath);

public:
  template<class T>
  lquant_data<T>& get_data(string prop);

  template<class T>
  lquant_data<T>& get_daily_data(string prop);

  //todo
  template<class T>
  lquant_data<T>& get_index_data(string prop);

  template<class T>
  lquant_data<T>& get_index_daily_data(string prop);

  lquant_base_univ *get_univ() {return univ; }

  const vector<string>& get_tkrs() {return tkrs;}
  const vector<string>& get_index_tkrs() {return index_tkrs;}

  const vector<long>& get_minTimes() {return time_series;}
  const vector<long>& get_dlyTimes(){return daily_time_series;}

  bool load_coinpool(int num);
  bool load_tickpool(int num);  
  bool load_futtickpool(int num);  
  bool load_ctatickpool(int num);  
  bool load_futtickpool(int num,string path);  
  inline 
  const lquant_hold_rank* get_rank_data_ptr(){
    return hold_rank->get_data();
  }

  inline
  long get_endTime(int didx) {
    return time_series[didx];
  }

  inline
  int get_mn() {
    return meta->mn;
  }

  inline
  int get_nrow() {
    return meta->dn;
  }

  inline
  int get_ncol() {
    return meta->tn;
  }
  
  inline
  int get_last_didx() {
    return *last_didx;
  }

  inline
  int get_tick_last_didx() {
    return *tick_last_didx;
  }

  inline
  int *get_last_didx_vec() {
    return last_didx_vec;
  }

  inline
  const int *get_cidx() {
    return cidx;
  }

  inline
  const int *get_index_cidx() {
    return index_cidx;
  }

  lquant_tick_pool *get_pool(int didx) const {
    return pools[didx];
  }

  lquant_crypto_tick_pool *get_coin_pool(int didx) const {
    return coinpools[didx];
  }

  lquant_tick_pool **get_cta_pool(int didx) const {
    return ctapools[didx];
  }

  void uninstall_pool(int didx);
  void uninstall_coinpool(int didx);
  void uninstall_cta_pool(int didx);

  lquant_data_group &get_data_group() {
    return dgr;
  }

};

/////////////////////////////////////////////////////////////////////////////////
template<class T>
lquant_data<T>& lquant_data_api::get_data(string prop) {
  if(std::is_same<T, float>::value) {
    return *(lquant_data<T>*)fmap[prop];
  }else if(std::is_same<T, double>::value){
    return *(lquant_data<T>*)dmap[prop];
  }else if(std::is_same<T, long>::value){
    return *(lquant_data<T>*)lmap[prop];
  }else if(std::is_same<T, int>::value){
    return *(lquant_data<T>*)imap[prop];
  }else if(std::is_same<T, bool>::value){
    return *(lquant_data<T>*)bmap[prop];
  }else if(std::is_same<T, char>::value){
    return *(lquant_data<T>*)cmap[prop];
  }
}

template<class T>
lquant_data<T>& lquant_data_api::get_daily_data(string prop) {
  if(std::is_same<T, float>::value) {
    return *(lquant_data<T>*)daily_fmap[prop];
  }else if(std::is_same<T, double>::value){
    return *(lquant_data<T>*)daily_dmap[prop];
  }else if(std::is_same<T, long>::value){
    return *(lquant_data<T>*)daily_lmap[prop];
  }else if(std::is_same<T, int>::value){
    return *(lquant_data<T>*)daily_imap[prop];
  }else if(std::is_same<T, bool>::value){
    return *(lquant_data<T>*)daily_bmap[prop];
  }else{
    return *(lquant_data<T>*)daily_cmap[prop];
  }
}

template<class T>
lquant_data<T>& lquant_data_api::get_index_data(string prop) {
  if(std::is_same<T, float>::value) {
    return *(lquant_data<T>*)f_index_map[prop];
  }else if(std::is_same<T, double>::value){
    return *(lquant_data<T>*)d_index_map[prop];
  }else if(std::is_same<T, long>::value){
    return *(lquant_data<T>*)l_index_map[prop];
  }else if(std::is_same<T, int>::value){
    return *(lquant_data<T>*)i_index_map[prop];
  }else if(std::is_same<T, bool>::value){
    return *(lquant_data<T>*)b_index_map[prop];
  }else if(std::is_same<T, char>::value){
    return *(lquant_data<T>*)c_index_map[prop];
  }
}

template<class T>
lquant_data<T>& lquant_data_api::get_index_daily_data(string prop) {
  if(std::is_same<T, float>::value) {
    return *(lquant_data<T>*)daily_f_index_map[prop];
  }else if(std::is_same<T, double>::value){
    return *(lquant_data<T>*)daily_d_index_map[prop];
  }else if(std::is_same<T, long>::value){
    return *(lquant_data<T>*)daily_l_index_map[prop];
  }else if(std::is_same<T, int>::value){
    return *(lquant_data<T>*)daily_i_index_map[prop];
  }else if(std::is_same<T, bool>::value){
    return *(lquant_data<T>*)daily_b_index_map[prop];
  }else{
    return *(lquant_data<T>*)daily_c_index_map[prop];
  }
}

