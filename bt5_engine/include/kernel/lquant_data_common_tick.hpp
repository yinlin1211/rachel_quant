#pragma once

#include "lquant_tick_pool.hpp"

struct wts_change_list {
  int chgIdx[1024*10];
  int n;
};

struct latest_tick {
  lquant_tick_pool *pool;
  long didxNow;
  long tidxNow;
  GDataType type;
  GDataValue val;
  int cidx, idx;
  int dte, tme;
};

