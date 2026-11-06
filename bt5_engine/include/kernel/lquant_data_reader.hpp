#pragma once

#include<sys/mman.h>  
#include<sys/types.h>  
#include<fcntl.h>  
#include<unistd.h>  
#include "lquant_data_api.hpp"

static const int CHUNK_SIZE = 4096;
static const int GLOBAL_INDEX_SIZE = 1024 * 1024 * 512;

template<class T>
class lquant_data_reader : public lquant_data<T> {
public:
  lquant_data_reader(const char *fileName,long len,long width, long cn = 1, long mn = 1);
  ~lquant_data_reader();
};

template<class T>
lquant_data_reader<T>::lquant_data_reader(const char *fileName,long len,long width, long cn, long mn){
  int f = open(fileName, O_RDONLY, S_IRUSR);
  if(f < 0){
    cout<<"failed to open file: {}"<< fileName<<endl;
    return; 
  }

  this->dataLen = width * len * cn * mn;
  this->data = (T*) mmap(0, this->dataLen*sizeof(T), PROT_READ, MAP_SHARED, f, 0);
  this->set_dim(len, width, cn, mn);
}

template<class T>
lquant_data_reader<T>::~lquant_data_reader(){
  munmap((void *)this->data, this->dataLen*sizeof(T));
}
