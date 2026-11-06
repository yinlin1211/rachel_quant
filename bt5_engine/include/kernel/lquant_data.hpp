#pragma once

#include <cassert>

template<class T>
class lquant_data{
protected:
  T* data;
  long dataLen;

  long dn, tn, cn, mn; //dn * tn * cn * mn <= dataLen
  long tnxcnxmn, cnxmn, tnxcn;
  
public:
  lquant_data() {
    data = NULL;
    dataLen = 0;
    dn = tn = cn = mn = tnxcnxmn = tnxcn = cnxmn = 0;
  }

  virtual ~lquant_data() {
    
  }

  lquant_data<T> &operator=(lquant_data<T> &r) {
    data    = r.data;
    dataLen = r.dataLen;
    dn      = r.dn;
    tn      = r.tn;
    cn      = r.cn;
    mn      = r.mn;
    return *this;
  }

  const lquant_data<T> &operator=(const lquant_data<T> &r) {
    data    = r.data;
    dataLen = r.dataLen;
    dn      = r.dn;
    tn      = r.tn;
    cn      = r.cn;
    mn      = r.mn;
    return *this;
  }

  const T* get_data() const{
    return data;
  }

  virtual T* get_data_ptr() {
    return NULL;
  }

  tuple<long, long, long, long> size() { return make_tuple(dn,tn,cn,mn); }

  //2d
  inline
  T operator()(long didx,long tidx) {
    return *(data + didx * tn * cn * mn + tidx* cn * mn);
  }

  //3d
  inline
  T operator()(long didx,long tidx, long cidx) {
    return *(data + didx * tn * cn * mn + tidx * cn * mn + cidx * mn);
  }

  //4d
  inline
  T operator()(long didx,long tidx, long cidx, long midx) {
     return *(data + didx * tn * cn * mn + tidx * cn * mn + cidx * mn + midx);
  }

protected:
  void set_dim(long dn = 1, long tn = 1, long cn = 1, long mn = 1) {
    this->dn = dn;
    this->tn = tn;
    this->cn = cn;
    this->mn = mn;
    //assert(dn * tn * cn * mn <= dataLen);
    this->tnxcnxmn = tn * cn * mn;
    this->cnxmn    = cn * mn;
    this->tnxcn    = tn * cn;
  }
};
