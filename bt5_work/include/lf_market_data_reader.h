#pragma once

#include <atomic>
#include <iostream>
#include <map>
#include <thread>
#include <vector>
#include <chrono>
#include "lquantTick.hpp"
#include "lquant_tick_pool.hpp"
#include "lquant_data_api.hpp"
#include "lquant_data_writer.hpp"
#include "lquant_utils.hpp"

using namespace lquant;

namespace lfquant
{

class lf_market_data_reader
{
public:
	lf_market_data_reader();
	~lf_market_data_reader();

	bool init(std::string dataPath);
	void update();
	void stop() { is_running_ = false; }

	void set_read_from_start(bool bfromStart) { bfromStart_ = bfromStart; }
	void set_day(int day) { _myday = day; }
	int get_day() { return _myday; }

	void set_callback(std::function<void(int,const char*,void*)> callback)
	{
		callback_ = callback;
	}

	void run_updata_thread();
	std::vector<string> get_tkrs() { return tkrs; }
	lquant_tick_pool* get_pool() { return &_pool; }

private:
	lquant_tick_pool _pool;
	int _myday;
	int _cur_didx;
	
	std::map<string, int> _tkr2idxMap;
    std::vector<string> tkrs;

	std::function<void(int,const char*,void*)> callback_;
	std::atomic_bool is_running_;
	bool bfromStart_ = true;
};

}
