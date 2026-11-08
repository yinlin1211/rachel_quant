#pragma once

#include <atomic>
#include <iostream>
#include <map>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

#include "lquantTick.hpp"
#include "lquant_tick_pool.hpp"
#include "lquant_data_api.hpp"
#include "lquant_data_writer.hpp"
#include "lquant_utils.hpp"
#include "lf_market_data_reader.h"

using namespace lquant;

namespace lfquant
{

enum lf_price_type
{
	LFPT_LastPrice,
	LFPT_Bid1,
	LFPT_Ask1,
};

struct lf_market_data
{
	std::atomic_int lastPrice;
	std::atomic_int bid1;
	std::atomic_int ask1;
};

class lf_market_mgr
{
public:
	static lf_market_mgr& getInstance()
	{
		static lf_market_mgr inst_;
		return inst_;
	}

	~lf_market_mgr();

	bool init();
	double get_price(const char *tkr, lf_price_type type);

private:
	lf_market_mgr();
	void on_tick(int type, const char* tkr, void* p);
 	lf_market_data_reader tick_data_;

	std::unordered_map<string, lf_market_data*> marketMap_;
};

}
