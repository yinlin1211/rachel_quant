#pragma once

#include <atomic>
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <mutex>

#include "lquantOrder.hpp"
#include "lf_strategy_define.h"

namespace lfquant
{

using std::vector;
class HeguiTick;
struct HgConfig;

struct HeguiTick
{
    int nTime;			//最新价
    int nPrice;			//最新价
    // int64_t iTurnover;		
    int64_t iVolume;	

	void set(lf_hangqing *hq)
	{
        nTime = hq->latestTime;
        nPrice = hq->lastPrice;
		// iTurnover = hq->iTurnover;
        iVolume = hq->iVolume;
	}	
};

class lf_hegui
{
public:
	lf_hegui();

	void set_hangqing(lf_hangqing *hq) { hq_ = hq; }
	void on_hq();

	//获得合规的数量
	int get_qty(const std::string &symbol, int qty, float prc, char side, const vector<lfquant::lf_order_t> &orders, const vector<lfquant::lf_trade_t> &trades);

private:
	int get_qty_16(const std::string &symbol, int qty, float prc, char side, const HgConfig &cfg, const vector<lfquant::lf_order_t> &orders, const vector<lfquant::lf_trade_t> &trades);
	int get_qty_17(const std::string &symbol, int qty, float prc, char side, const HgConfig &cfg, const vector<lfquant::lf_order_t> &orders, const vector<lfquant::lf_trade_t> &trades);
	int get_qty_19(const std::string &symbol, int qty, float prc, char side, const HgConfig &cfg, const vector<lfquant::lf_order_t> &orders, const vector<lfquant::lf_trade_t> &trades);
	// int get_qty_20(const std::string &symbol, int qty, float prc, char side, const HgConfig &cfg, const vector<lfquant::lf_order_t> &orders, const vector<lfquant::lf_trade_t> &trades);
	int get_qty_23(const std::string &symbol, int qty, float prc, char side, const HgConfig &cfg, const vector<lfquant::lf_order_t> &orders, const vector<lfquant::lf_trade_t> &trades);
	int get_qty_25(const std::string &symbol, int qty, float prc, char side, const HgConfig &cfg, const vector<lfquant::lf_order_t> &orders, const vector<lfquant::lf_trade_t> &trades);

	double get_symbol_zhang_die_fu(int item, const std::string &symbol);
	double get_return(int startTime);
	bool is_price_qu_shi(int startTime, char side, float prc, const vector<lfquant::lf_trade_t> &trades);
	int get_volume_turnover(int startTime, double &amount, const vector<lfquant::lf_order_t> &orders, const vector<lfquant::lf_trade_t> &trades);
	int get_market_volume(int startTime);

	lf_hangqing *hq_ = nullptr;
	std::mutex ticks_mutex_; 
	std::vector<HeguiTick*>  ticks_; 

	std::atomic_int last_tick_time_;
	int openTime_ = 0;
	int closeJhTime_ = 0;
	int close1430_ = 0;
	bool isRunning_ = true;
};

}
