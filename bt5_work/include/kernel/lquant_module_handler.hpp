#pragma once

#include <string>
#include <dlfcn.h>
#include "lquant_utils.hpp"

class lquant_base_module_handler {
protected:
  void *load_module(std::string id, std::string path) {
    void * pHandle  = dlopen(path.c_str(), RTLD_LAZY);
    if(!pHandle) {
      printf("dlopen - %s\n", dlerror());
      throwException(std::string("load ") + path + " failed!");
    }
    return pHandle;
  }
};

