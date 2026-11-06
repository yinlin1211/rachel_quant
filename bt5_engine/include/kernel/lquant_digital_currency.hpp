#pragma once

struct digital_currency_trade_t {
  short id;
  long  time;
  long  localtime;
  float qty;
  float prc;
  char  direction;
};


struct digital_currency_depth_t {
	short id;
	long  time;
	long  localtime;
	float latest;
    float opi;
	float nAskPrice[20];
	float nAskVol[20];
	float nBidPrice[20];
	float nBidVol[20];
};

