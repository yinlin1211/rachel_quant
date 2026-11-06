#pragma once

#include <cassert>
#include <cmath>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <string>
#include <armadillo>

//sign of given value
template <class DataType>
int sign(const DataType &x)
{
  return (DataType{0} < x) - (x < DataType{0});
};

//sum values over a sequence, with `size` elements
template <class DataType>
DataType sum(const DataType *X, int size)
{
  DataType sum_val{0};
  for (auto i = 0; i < size; ++i)
    sum_val += X[i];
  return sum_val;
};

template <class DataType>
DataType nan_sum(const DataType *X, int size)
{
  DataType sum_val{0};
  for (auto i = 0; i < size; ++i)
    sum_val += (std::isnan(X[i]) ? 0 : X[i]);
  return sum_val;
};

template <class DataType>
DataType mean(const DataType *X, int size)
{
  return sum(X, size) / size;
};

template <class DataType>
DataType nan_mean(const DataType *X, int size)
{
  DataType sum{0};
  int count{0};
  for(int i = 0; i < size; ++i)
    {
      if (!std::isnan(X[i]))
        {
	  sum += X[i];
	  ++count;
        }
    }
  if (count == 0)
    return std::numeric_limits<DataType>::quiet_NaN();
  return sum / count;
};

template <class DataType>
DataType max(const DataType *X, int size)
{
  auto max_val = X[0];
  for (auto i = 1; i < size; ++i)
    max_val = (X[i] > max_val ? X[i] : max_val);
  return max_val;
};

template <class DataType>
int argmax(const DataType *X, int size)
{
  auto max_val = X[0];
  int idx = 0;
  for (auto i = 1; i < size; ++i)
    if (X[i] > max_val)
      {
	max_val = X[i];
	idx = i;
      }
  return idx;
};

template <class DataType>
DataType min(const DataType *X, int size)
{
  auto min_val = X[0];
  for (auto i = 1; i < size; ++i)
    min_val = (X[i] < min_val ? X[i] : min_val);
  return min_val;
};

template <class DataType>
int argmin(const DataType *X, int size)
{
  auto min_val = X[0];
  int idx = 0;
  for (auto i = 1; i < size; ++i)
    if (X[i] < min_val)
      {
	min_val = X[i];
	idx = i;
      }
  return idx;
};

//sequence X and Y should not have NaNs
template <class DataType>
DataType covariance(const DataType *X, const DataType *Y, int size)
{
  assert (size > 0);
  if (size == 1)
    return std::numeric_limits<DataType>::quiet_NaN();
  DataType sum_x = X[0];
  DataType sum_y = Y[0];
  DataType sum_xy = X[0] * Y[0];
  for (auto i = 1; i < size; ++i)
    {
      sum_x += X[i];
      sum_y += Y[i];
      sum_xy += X[i] * Y[i];
    }
  return sum_xy / size - sum_x * sum_y / (size * size);
};

template <class DataType>
DataType variance(const DataType *X, int size)
{
  return covariance(X, X, size);
};

template <class DataType>
DataType stddev(const DataType *X, int size)
{
  return sqrt(covariance(X, X, size));
};

template <class DataType>
DataType nan_stddev(const DataType *X, int size)
{
  DataType sum{0};
  DataType sum_sq{0};
  int count{0};
  for(int i = 0; i < size; ++i)
    {
      if (!std::isnan(X[i]))
        {
	  sum += X[i];
	  sum_sq += X[i] * X[i];
	  ++count;
        }
    }
  if (count == 0)
    return std::numeric_limits<DataType>::quiet_NaN();
  return sqrt(sum_sq / count - sum * sum / (count * count));
};

template <class DataType>
DataType skewness(const DataType *X, int size)
{
  auto mu = mean(X, size);
  auto std = stddev(X, size);
  DataType sum_cub{0};
  for(auto i = 0; i < size; ++i)
    sum_cub += pow((X[i] - mu) / std, 3);
  return sum_cub / size;
};

template <class DataType>
DataType kurtosis(const DataType *X, int size)
{
  auto mu = mean(X, size);
  auto std = stddev(X, size);
  DataType sum_q{0};
  for(auto i = 0; i < size; ++i)
    sum_q += pow((X[i] - mu) / std, 4);
  return sum_q / size - 3;
};

template <class DataType>
DataType correlation(const DataType *X, const DataType *Y, int size)
{
  return covariance(X, Y, size) / sqrt(variance(X, size) * variance(Y, size));
};

template <class DataType>
void argsort(const DataType *X, int *indices, int size)
{
  for (auto i = 0; i < size; ++i)
    indices[i] = i;
  std::sort(indices, indices + size, [&X](int i1, int i2) {return X[i1] < X[i2];});
};

template <class DataType, class RankType>
void ranking(DataType* X, RankType *Rank, int size, bool norm=false) 
{
  // initialize original index locations
  std::vector<int> idx(size);
  std::iota(idx.begin(), idx.end(), 0); 
  int count = 0;

  // sort indexes based on comparing values in v, assume nan is the largest
  std::sort(idx.begin(), idx.end(), [&](int i1, int i2) {if (std::isnan(X[i2])) return !std::isnan(X[i1]); else return X[i1] < X[i2];});
  for (int i=0; i < size; ++i)
    {
      if (!std::isnan(X[idx[i]]))
        {
	  Rank[idx[i]] = i + 1;
	  ++count;
        }
    }   
  if (norm)
    {
      auto tmp = 1.0 / count;
      for (int i=0; i < size; ++i)
	Rank[i] *= tmp;
    }
};

//inplace rank, the raw data will be replaced with the rank
template <typename DataType>
bool rankdata(DataType* p, size_t length, bool norm=false) 
{
  // initialize original index locations
  std::vector<int> idx(length);
  std::iota(idx.begin(), idx.end(), 0); 
  double notNans = 0.0;

  // sort indexes based on comparing values in v, assume nan is the largest
  std::sort(idx.begin(), idx.end(),
	    [&p](int i1, int i2) {if (std::isnan(p[i2])) return !std::isnan(p[i1]); else return p[i1] < p[i2];});

  for (size_t i=0; i < length; ++i)
    {
      if (!std::isnan(p[idx[i]]))
        {
	  p[idx[i]] = i + 1;
	  notNans += 1.0;
        }
    }   

  if (norm)
    {
      for (size_t i=0; i < length; ++i)
	p[i] /= notNans;
    }
  return true;
};

//market cap neutralize, inplace
template <class DataType>
void neutralize(DataType *signal, DataType *mktcap, int size)
{
  arma::Mat<DataType> X(size, 2, arma::fill::zeros);
  arma::Col<DataType> y(size);
  for(auto i = 0; i < size; ++i)
    {
      X(i, 0) = 1;
      X(i, 1) = mktcap[i];
      y(i) = signal[i];
    }
  auto beta = arma::solve(X, y);
  auto resid = y - X * beta;
  for(auto i = 0; i < size; ++i)
    signal[i] = resid[i];
};

//sector neutralize
//sector be numbers from 1 to the number of sectors
template<class DataType>
void neutralize(DataType *signal, int *sector, int size, int num_sector)
{

  arma::Mat<DataType> X(size, num_sector + 1, arma::fill::zeros);
  arma::Col<DataType> y(size);
  for(auto i = 0; i < size; ++i)
    {
      X(i, 0) = 1;
      if (sector[i] > 0 && sector[i] <= num_sector)
	X(i, sector[i]) = 1;
      y(i) = signal[i];
    }
  auto beta = arma::solve(X, y);
  auto resid = y - X * beta;
  for(auto i = 0; i < size; ++i)
    signal[i] = resid[i];

};

//market cap and sector neutralize
template <class DataType>
void neutralize(DataType *signal, DataType *mktcap, int *sector, int size, int num_sector)
{
  arma::Mat<DataType> X(size, num_sector + 2, arma::fill::zeros);
  arma::Col<DataType> y(size);
  for(auto i = 0; i < size; ++i)
    {
      X(i, 0) = 1;
      X(i, 1) = mktcap[i];
      if (sector[i] > 0 && sector[i] <= num_sector)
	X(i, sector[i] + 1) = 1;
      y(i) = signal[i];
    }
  auto beta = arma::solve(X, y);
  auto resid = y - X * beta;
  for(auto i = 0; i < size; ++i)
    signal[i] = resid[i];
};

//risk neutralize with risk factors, risk_mat, matrix of risk factors
template <class DataType>
void risk_neutral(DataType *alpha, DataType **risk_mat, int size, int num_rfactor)
{
  arma::Mat<DataType> X(size, num_rfactor, arma::fill::zeros);
  arma::Col<DataType> y(size);
  for(auto i = 0; i < size; ++i)
    {
      for(auto j = 0; j < num_rfactor; ++j)
	X(i, j) = risk_mat[i][j];
      y(i) = alpha[i];
    }
  auto beta = arma::solve(X, y);
  auto resid = y - X * beta;
  for(auto i = 0; i < size; ++i)
    alpha[i] = resid[i];
};

//centralize
template<class DataType>
void centralize(DataType *signal, int size)
{
  DataType sum{0};
  int count{0};
  for(auto i = 0; i < size; ++i)
    {
      if (!std::isnan(signal[i]))
        {
	  sum += signal[i];
	  ++count;
        }
    }
  DataType avg{0};
  if (count > 0)
    avg = sum / count;
  for(auto i = 0; i < size; ++i)
    signal[i] -= avg;
};

//normalize
template<class DataType>
void normalize(DataType *signal, int size)
{
  DataType sum{0};
  DataType sum_sq{0};
  int count{0};
  for(auto i = 0; i < size; ++i)
    {
      if (!std::isnan(signal[i]))
        {
	  sum += signal[i];
	  sum_sq += signal[i] * signal[i];
	  ++count;
        }
    }
  DataType std{1};
  if (count > 0)
    std = sqrt(sum_sq / count - sum * sum / (count * count));
  for(auto i = 0; i < size; ++i)
    signal[i] /= std;
};

//zscore
template <class DataType>
void z_score(DataType *X, int size)
{
  auto mu = nan_mean(X, size);
  auto std = nan_stddev(X, size);
  for(int i = 0; i < size; ++i)
    X[i] = (X[i] - mu) / std;
};

struct sGrp{
  int cnts;
  double sum_;
};

template <typename DataType>
void grp_neutral(DataType* alpha, const long* grp, size_t length)
{
  std::unordered_map<int, sGrp> grpMap;

  // map
  for (int i=0; i < length; ++i)
    {   
      if (!std::isnan(alpha[i]))
        {   
	  grpMap[grp[i]].sum_ += alpha[i];
	  ++grpMap[grp[i]].cnts;
        }   
    }   
  // demean
  for (int i=0; i < length; ++i)
    alpha[i] -= (grpMap[grp[i]].sum_ / grpMap[grp[i]].cnts);
};

template <typename K, typename V>
V getWithDef(const  std::map<K,V> & m, const K & key, const V & defval ) {
  typename std::map<K,V>::const_iterator it = m.find( key );
  if ( it == m.end() ) {
    return defval;
  }
  else {
    return it->second;
  }
};

template <class DataType>
void fill_nan(DataType *signal, int size, DataType fill)
{
  for(auto i = 0; i < size; ++i)
    if (std::isnan(signal[i]))
      signal[i] = fill;
};

enum weight_type {Equal, Linear, Exponential};

template<class DataType>
void init_weights(DataType *wts, int size, weight_type wtype)
{

  switch(wtype)
    {
    case weight_type::Exponential:
      {
	auto decay = (size - 1.0) / (size + 1);
	for(auto i = 0; i < size; ++i)
	  wts[i] = (1 - decay) * pow(decay, size-1-i) / (1 - pow(decay, size));
	break;
      }
    case weight_type::Linear:
      {
	for(auto i = 0; i < size; ++i)
	  wts[i] = 2.0 * (i + 1) / (size * (size + 1));
	break;
      }
    default:
      {
	for(auto i = 0; i < size; ++i)
	  wts[i] = 1.0 / size;
      }
    }   
};

template <class DataType>
DataType decay_(const DataType *X, int size, weight_type wtp)
{
  DataType *weights = new DataType[size];
  init_weights(weights, size, wtp);
  DataType ret{0};
  for(int i = 0; i < size; ++i)
    ret += X[i] * weights[i];
  delete[] weights;
  return ret;
};

template <class DataType>
DataType decay_linear(const DataType *X, int size)
{
  return decay_(X, size, weight_type::Linear);
};

template <class DataType>
DataType decay_exp(const DataType *X, int size)
{
  return decay_(X, size, weight_type::Exponential);
};

template <class DataType>
class linear_1d
{
public:
  linear_1d() : fitted(false), degree(0), slope(0), intercept(0), rsquared(0), rsquared_adj(0), resvol(0), residual(nullptr) {} ;
  ~linear_1d() { if (residual !=nullptr) delete[] residual; };

  inline bool is_fitted() const { return fitted; };
  inline DataType get_slope() const { return slope; };  
  inline DataType get_intercept() const { return intercept; }
  inline DataType get_rsquared() const { return rsquared; }
  inline DataType get_rsquared_adj() const { return rsquared_adj; }
  inline DataType get_resvol () const { return resvol; }
  inline DataType *get_residual() const { return residual; }
  void fit(DataType *X, DataType *Y, int size)
  {
    slope = covariance(X, Y, size) / variance(X, size);
    intercept = mean(Y, size) - slope * mean(X, size);
    if (residual == nullptr)
      residual = new DataType[size];
    else if (degree != size)
      {
	delete[] residual;
	residual = new DataType[size];
      }
    degree = size;
    for (auto i = 0; i < size; ++i)
      residual[i] = Y[i] - slope * X[i] - intercept;
    auto SST = variance(Y, size);
    auto SSE = variance(residual, size);
    rsquared = 1 - SSE / SST;
    if (size == 2)
      rsquared_adj = rsquared;
    else
      rsquared_adj = 1 - SSE * (size - 1) / (SST * (size - 2));
    resvol = sqrt(SSE);
    fitted = true;
  };
  void predict(DataType *X, DataType *P, int size)
  {
    if (!fitted) 
      throw std::bad_exception();
    for (auto i = 0; i < size; ++i)
      P[i] = slope * X[i] + intercept;
  };
private:
  bool fitted;
  int degree;
  DataType slope;
  DataType intercept;
  DataType rsquared;
  DataType rsquared_adj;
  DataType resvol;
  DataType *residual;
};
