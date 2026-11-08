#pragma once
#include <limits>
#include <tuple>

#include "lquant_osc.hpp"

/******************************************************************
 * 基本思想是：
 * 1，如果时间前移，则把上一秒钟的最大值传到CMAX对象维护
 * 2，如果时间还是当前秒，则维护当前秒的最大值
 * 3，如果时间跳跃不止1秒，则拿前值填充，并传入CMAX对象
 * 4，最终的最大值等于thisSecHigh和prevSecHigh的最大值
 *****************************************************************/
class CVariableHigh: public lquant_base_osc<tuple<int,double>,double> {
public:
  CVariableHigh(int nSec) {
    maxCalcor = new CMAX(nSec-1);
    thisSecHigh = prevSecHigh = numeric_limits<double>::min();
    nowSec = -1;
  }

  virtual ~CVariableHigh() {
    if(maxCalcor) {
      delete maxCalcor;
      maxCalcor = NULL;
    }
  }

  lquant_base_osc *on_new_value(tuple<int,double> k) {
    tie(atme, val) = k;

    if(atme > nowSec) {
      if(nowSec < 0) {
	nowSec = atme;
	thisSecHigh = val;
	prevSecHigh = val;
      }
      else {
	for(int i=nowSec;i<atme;i++) {
	  maxCalcor->on_new_value(thisSecHigh);
	}
	prevSecHigh = maxCalcor->get_osc();

	nowSec = atme;
	thisSecHigh = val;
      }
    }
    else {
      if(thisSecHigh < val) {
	thisSecHigh = val;
      }
    }
    return this;
  }

  double get_osc() {
    return max(thisSecHigh, prevSecHigh);
  }

private:
  CMAX *maxCalcor;
  double thisSecHigh, prevSecHigh;
  int nowSec;

  int atme;
  double val;
};

class CVariableLow: public lquant_base_osc<tuple<int,double>,double> {
public:
  CVariableLow(int nSec) {
    minCalcor = new CMIN(nSec-1);
    thisSecLow = prevSecLow = numeric_limits<double>::max();
    nowSec = -1;
  }

  virtual ~CVariableLow() {
    if(minCalcor) {
      delete minCalcor;
      minCalcor = NULL;
    }
  }

  lquant_base_osc *on_new_value(tuple<int,double> k) {
    tie(atme, val) = k;

    if(atme > nowSec) {
      if(nowSec < 0) {
	nowSec = atme;
	thisSecLow = val;
	prevSecLow = val;
      }
      else {
	for(int i=nowSec;i<atme;i++) {
	  minCalcor->on_new_value(thisSecLow);
	}
	prevSecLow = minCalcor->get_osc();

	nowSec = atme;
	thisSecLow = val;
      }
    }
    else {
      if(thisSecLow > val) {
	thisSecLow = val;
      }
    }
    return this;
  }

  double get_osc() {
    return min(thisSecLow, prevSecLow);
  }

private:
  CMIN *minCalcor;
  double thisSecLow, prevSecLow;
  int nowSec;

  int atme;
  double val;
};



class TsVarBufferSum : public lquant_base_osc<tuple<int, double>, double> {
public:
  TsVarBufferSum(int _mxSize, int _mxMsc)
    : maxSize(_mxSize + 1), maxMsc(_mxMsc), begin(0), end(0), count(0), sumValue(0) {
    mscCache = new int[maxSize];
    valueCache = new double[maxSize];
  };

  virtual ~TsVarBufferSum() {
    if (mscCache)
      delete[] mscCache;
    if (valueCache)
      delete[] valueCache;
  };

  lquant_base_osc * on_new_value(tuple<int, double> data) {
    int msc = 0;
    double value = 0;
    std::tie(msc, value) = data;
    mscCache[end] = msc;
    valueCache[end] = value;
    sumValue += value;
    ++count;
    auto next_pos = (end + 1) % maxSize;
    if (next_pos == begin) {
      sumValue -= valueCache[begin];
      begin = (begin + 1) % maxSize;
      --count;
    }
    end = next_pos;
    while (msc - mscCache[begin] > maxMsc && begin != end) {
      sumValue -= valueCache[begin];
      begin = (begin + 1) % maxSize;
      --count;
    }
    return this;
  };
  
  int get_size() {
    return count;
  }

  double get_osc() {
    return sumValue;
  };

private:
  int maxSize;
  int maxMsc;
  int begin;
  int end;
  int count;
  double sumValue;
  int *mscCache;
  double *valueCache;
};


class TsVarTagBufferSum : public lquant_base_osc<tuple<char, int, double>, double> {
public:
  TsVarTagBufferSum(int _mxSize, int _mxMsc, char _tag = 'B')
    : maxSize(_mxSize + 1), maxMsc(_mxMsc), CmpTag(_tag), begin(0), end(0), count(0), sumValue(0) {
    tagCache = new char[maxSize];
    mscCache = new int[maxSize];
    valueCache = new double[maxSize];
  };

  virtual ~TsVarTagBufferSum() {
    if (tagCache)
      delete[] tagCache;
    if (mscCache)
      delete[] mscCache;
    if (valueCache)
      delete[] valueCache;
  };

  lquant_base_osc * on_new_value(tuple<char, int, double> data) {
    char tag = 'B';
    int msc = 0;
    double value = 0;
    std::tie(tag, msc, value) = data;
    tagCache[end] = tag;
    mscCache[end] = msc;
    valueCache[end] = value;
    if (CmpTag == tag) {
      sumValue += value;
      ++count;
    }
    auto next_pos = (end + 1) % maxSize;
    if (next_pos == begin) {
      if (CmpTag == tagCache[begin]){
	    sumValue -= valueCache[begin];
        --count;
      }
      begin = (begin + 1) % maxSize;
    }
    end = next_pos;
    while (msc - mscCache[begin] > maxMsc && begin != end) {
      if (CmpTag == tagCache[begin]){
	    sumValue -= valueCache[begin];
        --count;
      }
      begin = (begin + 1) % maxSize;
    }
    return this;
  };

  double get_osc() {
    return sumValue;
  };
  
  int get_size() {
    return count;
  }

private:
  int maxSize;
  int maxMsc;
  char CmpTag;
  int begin;
  int end;
  int count;
  double sumValue;
  char *tagCache;
  int *mscCache;
  double *valueCache;
};


class CMaxValTme : public lquant_base_osc<tuple<double,int>,double> {
  struct node {
    node *prev, *next;
    int nidx;
    double val;
    int tme;
  };

private:
  node *pool, *head, *tail;
  lquant_cbuffer<node *> *cbuf;

  int N;
  int freeK;
  int width;

private:
  node * get_free_node() {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }
public:
  CMaxValTme(int n,int det);
  virtual ~CMaxValTme();
  double get_osc();

  lquant_base_osc * on_new_value(tuple<double,int>);

};


class CMinValTme : public lquant_base_osc<tuple<double,int>,double> {
  struct node {
    node *prev, *next;
    int nidx;
    double val;
    int tme;
  };

private:
  node *pool, *head, *tail;
  lquant_cbuffer<node *> *cbuf;

  int N;
  int freeK;
  int width;

private:
  node * get_free_node() {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }

public:
  CMinValTme(int n,int det);
  virtual ~CMinValTme();
  double get_osc() ;

  lquant_base_osc * on_new_value(tuple<double,int> );

};



inline
CMaxValTme::CMaxValTme(int n,int det):N(n) {
  pool = new node[n+3];
  for(int i=0;i<n+3;i++) {
    pool[i].prev = pool[i].next = NULL;
    pool[i].nidx = i;
  }
  head = pool + n + 1;
  tail = pool + n + 2;
  head->next = tail;
  tail->prev = head;

  cbuf  = new lquant_cbuffer<node *>(n);
  freeK = 0;
  width = det;
}

inline
CMaxValTme::~CMaxValTme(){
  delete cbuf;
  delete pool;
}

inline
double CMaxValTme::get_osc() {
  return head->next->val;
}

inline
lquant_base_osc<tuple<double,int>,double> * CMaxValTme::on_new_value(tuple<double,int> data){
    
  int tme = 0;
  double k = 0;
  std::tie(k,tme) = data;
  
  node *nnd = get_free_node();
  nnd->val = k;
  nnd->tme = tme;
  nnd->prev = tail->prev;
  nnd->next = tail;
  tail->prev->next = nnd;
  tail->prev = nnd;

  for(node *nd=nnd->prev;nd && nd!=head ;nd=nd->prev) {
    if(nd->val<nnd->val ){
     nd->prev->next = nnd;
      nnd->prev = nd->prev;
    }else if (nd->tme+width < nnd->tme){
      nd->prev->next = nd->next;
      nd->next->prev = nd->prev;
    }else {
        
    }

  }

  if(cbuf->full()) {
    node *first = cbuf->get(-(N-1)); 
    node *top = head->next;
    if(top != tail && top == first) {
      head->next = top->next;
      top->next->prev = head;
    }
  }
  cbuf->add(nnd);

  return this;
}

inline
CMinValTme::CMinValTme(int n,int det):N(n) {
  pool = new node[n+3];
  for(int i=0;i<n+3;i++) {
    pool[i].prev = pool[i].next = NULL;
    pool[i].nidx = i;
  }
  head = pool + n + 1;
  tail = pool + n + 2;
  head->next = tail;
  tail->prev = head;

  cbuf  = new lquant_cbuffer<node *>(n);
  freeK = 0;
  width = det;
}

inline
CMinValTme::~CMinValTme(){
  delete cbuf;
  delete pool;
}

inline
double CMinValTme::get_osc() {
  return head->next->val;
}

inline
lquant_base_osc<tuple<double,int>,double> * CMinValTme::on_new_value(tuple<double,int> data){
  int tme = 0;
  double k = 0;
  std::tie(k,tme) = data;
  
  node *nnd = get_free_node();
  nnd->val = k;
  nnd->tme = tme;
  nnd->prev = tail->prev;
  nnd->next = tail;
  tail->prev->next = nnd;
  tail->prev = nnd;

  for(node *nd=nnd->prev;nd && nd!=head ;nd=nd->prev) {
    if(nd->val>nnd->val){
      nd->prev->next = nnd;
      nnd->prev = nd->prev;
    }else if(nd->tme+width < nnd->tme){
      nd->prev->next = nd->next;
      nd->next->prev = nd->prev;
    }else{
        
    }
  }

  if(cbuf->full()) {
    node *first = cbuf->get(-(N-1)); 
    node *top = head->next;
    if(top != tail && top == first) {
    head->next = top->next;
    top->next->prev = head;
    }
  }
  cbuf->add(nnd);

  return this;
}

class FixValue : public lquant_base_osc<tuple<double,int>,double> {
  struct node {
    node *prev, *next;
    int nidx;
    double val;
    int tme;
  };

private:
  node *pool, *head, *tail;
  lquant_cbuffer<node *> *cbuf;

  int N;
  int freeK;
  int width;
  double price;

private:
  node * get_free_node() {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }

public:
  FixValue(int n,int det);
  virtual ~FixValue();
  double get_osc() ;

  lquant_base_osc * on_new_value(tuple<double,int> data);

};



inline
FixValue::FixValue(int n,int det):N(n) {
  pool = new node[n+3];
  for(int i=0;i<n+3;i++) {
    pool[i].prev = pool[i].next = NULL;
    pool[i].nidx = i;
  }
  head = pool + n + 1;
  tail = pool + n + 2;
  head->next = tail;
  tail->prev = head;

  cbuf  = new lquant_cbuffer<node *>(n);
  freeK = 0;
  width = det;
  price = 0;
}

inline
FixValue::~FixValue(){
  delete cbuf;
  delete pool;
}

inline
double FixValue::get_osc() {
  return price;
}

inline
lquant_base_osc<tuple<double,int>,double> * FixValue::on_new_value(tuple<double,int> data){
    
  int tme = 0;
  double k = 0;
  std::tie(k,tme) = data;
  
  node *nnd = get_free_node();
  nnd->val = k;
  nnd->tme = tme;
  nnd->prev = tail->prev;
  nnd->next = tail;
  tail->prev->next = nnd;
  tail->prev = nnd;

  if(cbuf->full()) {
    node *first = cbuf->get(-(N-1)); 
    node *top = head->next;
    if(top != tail && top == first) {
    head->next = top->next;
    top->next->prev = head;
    }
  }
  cbuf->add(nnd);
  
  price = 0;
  for(node *nd=head->next;nd && nd!=tail ;nd=nd->next) {
    if (nd->tme+width <= nnd->tme){
      price = nd->val;
      head->next = nd;
      nd->prev=head;
    }else {
        break;
    }
  }
  return this;
}

class TsVarTagSum : public lquant_base_osc<tuple<char,double,int>,tuple<long,int,long,int>> {
  struct node {
    node *prev, *next;
    int nidx;
    double val;
    int tme;
    char BSFlag;
  };
  
private:
  node *pool, *Bhead, *Btail, *Shead, *Stail;
  lquant_cbuffer<node *> *cbuf;

  int N;
  int freeK;
  int width;
  
  long sumB,sumS;
  int countB,countS;

private:
  node * get_free_node() {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }
public:
  TsVarTagSum(int n,int det);
  virtual ~TsVarTagSum();
  tuple<long,int,long,int> get_osc();

  lquant_base_osc * on_new_value(tuple<char,double,int>);

};


inline
TsVarTagSum::TsVarTagSum(int n,int det):N(n) {
  pool = new node[n+5];
  for(int i=0;i<n+5;i++) {
    pool[i].prev = pool[i].next = NULL;
    pool[i].nidx = i;
  }
  Bhead = pool + n + 1;
  Btail = pool + n + 2;
  Shead = pool + n + 3;
  Stail = pool + n + 4;
  Bhead->next = Btail;
  Btail->prev = Bhead;
  Shead->next = Stail;
  Stail->prev = Shead;
  cbuf  = new lquant_cbuffer<node *>(n);
  freeK = 0;
  width = det;
  sumB=sumS=0;
  countB=countS=0;
}

inline
TsVarTagSum::~TsVarTagSum(){
  delete cbuf;
  delete pool;
}

inline
tuple<long,int,long,int> TsVarTagSum::get_osc() {
  return make_tuple(sumB,countB,sumS,countS);
}

inline
lquant_base_osc<tuple<char,double,int>,tuple<long,int,long,int>> * TsVarTagSum::on_new_value(tuple<char,double,int> data){
  char flag = 'B';
  int tme = 0;
  double k = 0;
  std::tie(flag,k,tme) = data;
  node *nnd = get_free_node();
  nnd->val = k;
  nnd->tme = tme;
  nnd->BSFlag = flag;
  if ('B'==flag){
    nnd->prev = Btail->prev;
    nnd->next = Btail;
    Btail->prev->next = nnd;
    Btail->prev = nnd;
    sumB += k;
    countB++;
  }else if('S'==flag){
    nnd->prev = Stail->prev;
    nnd->next = Stail;
    Stail->prev->next = nnd;
    Stail->prev = nnd;
    sumS +=k;
    countS++;
  }else{
    ;
  }
  
  if(cbuf->full()) {
    node *first = cbuf->get(-(N-1)); 
    node *top = Bhead->next;
    if(top != Btail && top == first) {
      sumB -=top->val;
      countB--;
      Bhead->next = top->next;
      top->next->prev = Bhead;
    }
    top = Shead->next;
    if(top != Stail && top == first) {
      sumS -=top->val;
      countS--;
      Shead->next = top->next;
      top->next->prev = Shead;
    }
  }
  for(node *nd=Bhead->next;nd && nd!=Btail ;nd=nd->next) {
    if (nd->tme+width < nnd->tme){
      sumB -=nd->val;
      countB--;
      
      Bhead->next = nd->next;
      nd->next->prev=Bhead;
    }else {
        break;
    }
  }
  
  for(node *nd=Shead->next;nd && nd!=Stail ;nd=nd->next) {
    if (nd->tme+width < nnd->tme){
      sumS -=nd->val;
      countS--;
      
      Shead->next = nd->next;
      nd->next->prev=Shead;
    }else {
        break;
    }
  }

  cbuf->add(nnd);
  
  return this;
}