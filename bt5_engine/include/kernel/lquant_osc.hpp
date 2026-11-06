#pragma once

#include<cstring>
#include <limits>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cassert>



using namespace std;

/******************************************************************************
 * circular buffer
 * get(0) returns the latest element, get(-1) returns the previous, etc.
 ******************************************************************************/
template<class T>
class lquant_cbuffer {
public:
  lquant_cbuffer(int n):MAXSIZE(n),head(0) {
    cbuf = new T[n];
    assert(cbuf);
  }
  virtual ~lquant_cbuffer() {
    if(cbuf) {
      delete cbuf;
      cbuf = NULL;
    }
  }
  inline
  lquant_cbuffer *add(T x) {
    cbuf[(head++) % MAXSIZE] = x;
    if (head>=MAXSIZE*2)
      head=head-MAXSIZE;
    return this;
  }
  inline
  T&  get(int k) { // k=0:脠隆脳卯脨脗t脢卤驴脤拢卢k=-1:脠隆t-1脢卤驴脤拢卢脪脭麓脣脌脿脥脝
    //assert(k < MAXSIZE && k < head)  // todo: if -k >= MAXSIZE, error!
    return cbuf[(head-1+k)%MAXSIZE];
  }
  inline
  T& operator[](int k) { // k=0:脠隆脳卯脨脗t脢卤驴脤拢卢k=-1:脠隆t-1脢卤驴脤拢卢脪脭麓脣脌脿脥脝
    //assert(k < MAXSIZE && k < head)  // todo: if -k >= MAXSIZE, error!
    return cbuf[(head-1+k)%MAXSIZE];
  }
  inline
  int size() {
    return head>=MAXSIZE ? MAXSIZE:head;
  }
  inline
  bool full() {
    return head >= MAXSIZE;
  }
  inline
  void clear() {
    head = 0;
  }
  inline
  int capacity() {
    return MAXSIZE;
  }
  inline 
  T* get_buffer(){
    return cbuf;
  }

private:
  lquant_cbuffer() {}

protected:
  T *cbuf;
  int MAXSIZE, head;
};

/*************************************************************************************
 * base class for various oscillator calculator
 *************************************************************************************/
template<class X, class T>
class lquant_base_osc{
public:
  virtual lquant_base_osc* on_new_value(X x) = 0;
  virtual lquant_base_osc* update_value(X x){
	  return NULL;
  }
  virtual T get_osc() = 0;
};


//////////////////////////////////////////////////////////////

/*************************************************************************************
 * tsrank
 *************************************************************************************/
class InvQuantile:public lquant_base_osc<double,double> {
  typedef struct Node {
    int k;
    struct Node *next;
    struct Node *prev;
  } TNode;

private:
  double  *buffer;
  TNode   *ptrs;
  TNode   *head;
  TNode   *tail;
  int n;
  int begk, endk;
  int totcnt;
  int gecnt;
  int eqcnt;

public:
  InvQuantile(int n);
  virtual ~InvQuantile() ;

public:
  double get_osc() ;

  lquant_base_osc *on_new_value(double x) ;

};

class CHurst : public lquant_base_osc<double,double> {
private:
  lquant_cbuffer<float> *cbuf;

public:
  CHurst(int n);

  virtual ~CHurst() ;

  double get_osc() ;

  lquant_base_osc *on_new_value(double x) ;

};

//快速算法
class CMAX : public lquant_base_osc<double,double> {
  struct node {
    node *prev, *next;
    int nidx;
    double val;
    
  };

private:
  node *pool, *head, *tail;
  lquant_cbuffer<node *> *cbuf;

  int N;
  int freeK;

private:
  node * get_free_node() {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }
public:
  CMAX(int n);
  virtual ~CMAX();
  double get_osc();

  lquant_base_osc * on_new_value(double k);
  lquant_base_osc * update_value(double k);


};

//快速算法
class CMIN : public lquant_base_osc<double,double> {
  struct node {
    node *prev, *next;
    int nidx;
    double val;
    
  };

private:
  node *pool, *head, *tail;
  lquant_cbuffer<node *> *cbuf;

  int N;
  int freeK;

private:
  node * get_free_node() {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }

public:
  CMIN(int n);
  virtual ~CMIN();
  double get_osc() ;

  lquant_base_osc * on_new_value(double k);
  lquant_base_osc * update_value(double k);

};

//快速算法
class CSMA : public lquant_base_osc<double,double> {
private:
  lquant_cbuffer<double> *cbuf;
  double ma;
  int N;
public:
  CSMA(int n);
  virtual ~CSMA();
  double get_osc();
  lquant_base_osc * on_new_value(double k);
  lquant_base_osc * update_value(double k);

};

/********************************************************************
 * 低延迟移动平均: 根据输入的长度值N，转换为指数系数，计算LLT
 * 参考文献: 广发证券
 ********************************************************************/
class CLLT : public lquant_base_osc<double,double> {
private:
  double alpha;
  lquant_cbuffer<double> price;
  lquant_cbuffer<double> trend;

public:
  //EMA与SMA可比，参数的换算关系: alpha=2/(N+1)
  CLLT(int N):alpha(2.0/(N+1)),price(3),trend(3) {

  }
  
  virtual ~CLLT() { }

  double get_osc() {
    return trend[trend.size()-1];
  }

  lquant_base_osc * on_new_value(double val){
    price.add(val);
    trend.add(val);
    doCalc();   
    return this;
  }

  lquant_base_osc * update_value(double val){
    price[0] = val;
    doCalc();
    return this;
  }

  inline
  void doCalc() {
    if(price.full()) {
      trend[0] = (alpha-alpha*alpha/4)*price[0] \
	+ 0.5*alpha*alpha*price[-1]    \
	- (alpha-0.75*alpha*alpha)*price[-2]    \
	+ 2*(1-alpha)*trend[-1]    \
	- (1-alpha)*(1-alpha)*trend[-2];
    }
    else {
      int n = price.size();
      if(n == 1) {
        ;
      }
      else if(n == 2) {
        trend[0] = (price[0] + price[-1])/2;
      }
    }
  }

};


/********************************************************************
 * 指数移动平均: 根据输入的长度值N，转换为指数系数，计算EMA
 * 参考文献: 
 ********************************************************************/
class CEMA : public lquant_base_osc<double,double> {
private:
  double alpha;
  lquant_cbuffer<double> price;
  lquant_cbuffer<double> trend;

public:
  //EMA与SMA可比，参数的换算关系: alpha=2/(N+1)
  CEMA(int N):alpha(2.0/(N+1)),price(2),trend(2) {

  }
  virtual ~CEMA() { }

  double get_osc() {
    return trend[0];
  }

  lquant_base_osc * on_new_value(double x){
    price.add(x);
    trend.add(x);
    doCalc();
    return this;
  }

  lquant_base_osc * update_value(double x){
    price[0] = x;
    doCalc();
    return this;
  }

  inline
  void doCalc() {
    int n = price.size();
    if(n <= 1) {
      ;
    }
    else {
      trend[0] = price[0] * alpha + trend[-1] * (1-alpha);
    }
  }

};

class CBIAS : public lquant_base_osc<double,double> {
private:
  double *cbuf;
  int MAXSIZE;
  int head;

public:
  CBIAS(int n) ;
  virtual ~CBIAS();
  double get_osc() ;
  lquant_base_osc * on_new_value(double k);
  lquant_base_osc * update_value(double k);

};

//naive implementation
class CVARIANCE : public lquant_base_osc<double,double> {
private:
  CSMA *xmean, *x2mean;

public:
  CVARIANCE(int n) ;
  virtual ~CVARIANCE();
  double get_osc() ;

  lquant_base_osc * on_new_value(double k);
  lquant_base_osc * update_value(double k);

 
};

//naive implementation
class CSTDEV : public lquant_base_osc<double,double> {
private:
  CVARIANCE *xvar;

public:
  CSTDEV(int n) ;
  virtual ~CSTDEV();
  double get_osc() ;

  lquant_base_osc * on_new_value(double k);
  lquant_base_osc * update_value(double k);

 
};

class CWMA : public lquant_base_osc<double,double> {
private:
  lquant_cbuffer<double> *cbuf;
  int n;
  double w, sum, tot, wn;
public:
  CWMA(int n,double weight) ;
  virtual ~CWMA();
  double get_osc();

  lquant_base_osc * on_new_value(double k);

 
};


class CArgMax : public lquant_base_osc<double,std::tuple<double,int> > {
  struct node {
    node *prev, *next;
    int nidx;
    double val;
    int cbidx;

   
  };

private:
  node *pool, *head, *tail;
  int N;
  int freeK;

  node **cbuf;
  int begk,endk;

private:
  node * get_free_node()  {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }

public:
  CArgMax(int n);
  virtual ~CArgMax();
  std::tuple<double,int>  get_osc();
  lquant_base_osc* on_new_value(double val);
  
};

class CArgMin : public lquant_base_osc<double,std::tuple<double,int> > {
  struct node {
    node *prev, *next;
    int nidx;
    double val;
    int cbidx;

  };

private:
  node *pool, *head, *tail;
  int N;
  int freeK;

  node **cbuf;
  int begk,endk;


private:
  node * get_free_node() {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }

public:
  CArgMin(int n);
  virtual ~CArgMin();
  
  std::tuple<double,int>  get_osc();
  lquant_base_osc* on_new_value(double val);

};

class CArgMaxMin : public lquant_base_osc<double,std::tuple<double,int,double,int> > {
  struct node {
    node *maxprev, *maxnext;
    node *minprev, *minnext;
    int nidx;
    double val;
    int cbidx;

  };

private:
  node *pool, *maxhead, *maxtail,*minhead, *mintail;
  int N;
  int freeK;

  node **cbuf;
  int begk,endk;


private:
  node * get_free_node() {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }

public:
  CArgMaxMin(int n);
  virtual ~CArgMaxMin();
  
  std::tuple<double,int,double,int>  get_osc();
  lquant_base_osc* on_new_value(double val);

};

class CArgMinMax : public lquant_base_osc<double,std::tuple<double,int,double,int> > {
  struct node {
    node *maxprev, *maxnext;
    node *minprev, *minnext;
    int nidx;
    double val;
    int cbidx;
 
  };

private:
  node *pool, *maxhead, *maxtail,*minhead, *mintail;
  int N;
  int freeK;

  node **cbuf;
  int begk,endk;


private:
  node * get_free_node() {
    node *nd =  pool + freeK;
    freeK++;
    freeK = (freeK) %(N+1);
    return nd;
  }

public:
  CArgMinMax(int n);
  virtual ~CArgMinMax();
  
  std::tuple<double,int,double,int>  get_osc();
  lquant_base_osc* on_new_value(double val);

};


class CCovariance : public lquant_base_osc<tuple<double,double>,double> {
public:
  CCovariance(int n);
  
  virtual ~CCovariance();
  
  double get_osc() ;

  lquant_base_osc * on_new_value(tuple<double,double> k);
  lquant_base_osc * update_value(tuple<double,double> k);
  
private:
  CSMA     *smaX, *smaY, *smaXY;
  double    Exy, Ex, Ey;

};

class CCorrelation : public lquant_base_osc<tuple<double,double>,double> {
public:
  CCorrelation(int n);
  
  virtual ~CCorrelation();
  
  double get_osc() ;

  lquant_base_osc * on_new_value(tuple<double,double> k);
  lquant_base_osc * update_value(tuple<double,double> k);
  
private:
  CVARIANCE *varX, *varY;
  CCovariance *covaXY;
  double    cvxy, vx, vy;

};

class CBeta : public lquant_base_osc<tuple<double,double>,double> {
public:
  CBeta(int n);
  
  virtual ~CBeta();
  
  double get_osc() ;

  lquant_base_osc * on_new_value(tuple<double,double> k);
  lquant_base_osc * update_value(tuple<double,double> k);
  
private:
  CVARIANCE *varX;
  CCovariance *covaXY;
  double    cvxy, vx;

};


class CSkewness : public lquant_base_osc<double,double> {
private:
  CSMA *X, *X2, *X3;
  int N;

public:
  CSkewness(int n);

  virtual ~CSkewness() ;

  double get_osc() ;

  lquant_base_osc * on_new_value(double k);

  lquant_base_osc * update_value(double k);

};

class CKurtosis : public lquant_base_osc<double,double> {
private:
  CSMA *X, *X2, *X3, *X4; 
  int N;

public:
  CKurtosis(int n);

  virtual ~CKurtosis();

  double get_osc();

  lquant_base_osc * on_new_value(double k);
  lquant_base_osc * update_value(double k);

};

class CDeltaD : public lquant_base_osc<double,double> {
private:
  lquant_cbuffer<double> *cbuf;
  int N;

public:
  CDeltaD(int n):N(n+1) {
    cbuf  = new lquant_cbuffer<double>(N);
  }
  virtual ~CDeltaD(){
    delete cbuf;
  }

  double get_osc() {
    if(cbuf->full())
      return cbuf->get(0) - cbuf->get(-N+1);
    else {
      int d = cbuf->size();
      return cbuf->get(0) - cbuf->get(-d+1);
    }
  }

  lquant_base_osc * on_new_value(double k){
    cbuf->add(k);

    return this;
  }

  lquant_base_osc * update_value(double k){
    cbuf->get(0) = k;
    return this;
  }

};

class CDecayLinear : public lquant_base_osc<double,double> {
private:
  lquant_cbuffer<double> *cbuf;
  int N;
  double norm, esum, wsum;

public:
  CDecayLinear(int n):N(n), norm(0), esum(0), wsum(0) {
    cbuf  = new lquant_cbuffer<double>(N);
  }
  virtual ~CDecayLinear() {
    delete cbuf;
  }

  double get_osc() {
    return wsum / norm;
  }

  lquant_base_osc * on_new_value(double k){
    if(cbuf->full()) {
      wsum += N * k - esum;
      esum += k - cbuf->get(1-N);
    } else {
      auto size = cbuf->size();
      esum += k;
      wsum += (size + 1) * k;
      norm += (size + 1);
    }
    cbuf->add(k);
    return this;
  }

  lquant_base_osc * update_value(double k){
    cbuf->get(0) = k;
    return this;
  }

};
