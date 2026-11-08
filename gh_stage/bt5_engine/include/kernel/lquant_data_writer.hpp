#pragma once

#include<sys/mman.h>  
#include<sys/types.h>  
#include<fcntl.h>  
#include<unistd.h>  
#include "lquant_data_api.hpp"

template<class T> 
class lquant_data_writer:public lquant_data<T> {
public:
  lquant_data_writer(string fileName, long len,long width, long cn = 1, long mn = 1);
  ~lquant_data_writer();

  virtual T* get_data_ptr() {
    return this->data;
  }

  //2d
  inline
  void pushData(T d,long didx,long tidx){
    *(this->data + didx * this->tn + tidx) = d;
  }

  //3d
  inline
  void pushData(T d,long didx,long tidx,long cidx){
    *(this->data + didx * this->tnxcn + tidx * this->cn + cidx) = d;
  }

  //4d
  inline
  void pushData(T d,long didx,long tidx,long cidx,long midx){
    *(this->data + didx * this->tnxcnxmn + tidx * this->cnxmn + cidx * this->mn + midx) = d;
  }

  inline
  void sync() {
    msync((void *)this->data, this->dataLen*sizeof(T), MS_SYNC);
  }

  inline
  void sync(long offset,int nrow) {
    msync((void *)(this->data+offset*this->tnxcnxmn*sizeof(T)), this->tnxcnxmn*nrow*sizeof(T), MS_SYNC);
  }

  int shrink(long dlen);

private:
  string ofname;
};

template<class T>
lquant_data_writer<T>::lquant_data_writer(string fileName, long len,long width, long cn, long mn):ofname(fileName){
  this->dataLen = len * width * cn * mn;
  this->set_dim(len, width, cn, mn);
  shrink(this->dataLen);
}

template<class T>
lquant_data_writer<T>::~lquant_data_writer(){
  munmap((void *)this->data, this->dataLen*sizeof(T));
}

template<class T>
int lquant_data_writer<T>::shrink(long dlen) {
  int f = open(ofname.c_str(), O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
  if(f < 0){
    char msg[1024];
    sprintf(msg, "failed to open file: {%s}", ofname.c_str());
    throwException(msg);
  }

  long fileLengthN = ((long)(dlen*sizeof(T) / 4096 + 1)) * 4096L;
  int ret = ftruncate(f, fileLengthN);
  if(ret < 0) {
    char msg[1024];
    sprintf(msg, "failed to ftruncate file: length=%ld", fileLengthN);
    throwException(msg);
  }
  this->data = (T*) mmap(0, fileLengthN, PROT_READ|PROT_WRITE, MAP_SHARED, f, 0);
  close(f);

  return 0;
}
