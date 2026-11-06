#pragma once

#include "lf_algo_stock.h"
#include "lf_market_data_reader.h"
#include "shmmd_read.h"
#include "lf_quant_backtrace_base.h"
#include <boost/lockfree/queue.hpp>
using namespace lfquant;

class SWapBasketManager: public quant_strategy_base {
	public:
		SWapBasketManager(): order_update_(10000) { oseq_ = 0; }
		virtual void start(const lf_basket& basket);
		virtual void stop() { isStopped_ = true; }
		virtual void pause(bool isPause) { trdconfig->isPause = isPause; }
		virtual void on_response(const lquant_order_t& order);
		virtual void start_sim(lf_basket basket, int tradeDate, std::string tickPath, std::string scoreFile);

	private:
		void init_basket();
		void real_thread();
		void send_order(const lquant_order_t& order);
		long generate_oseq() { oseq_ += 1; return oseq_; }
		void update_orders();
		
	private:
		bool is_real;  // 实盘还是回测
		lf_basket basket_;
		bool isStopped_ = false;
		lf_timeConfig* tmconfig = nullptr;
		lf_tradeConfig* trdconfig = nullptr;
		std::vector<string> tkr_list;  // tkr.txt对应的股票列表
		std::string tickPath_;
		std::string scoreFile_;
		lf_market_data_reader tick_data_;
		ShmMdReader<trade_signal> signal_reader_;
		std::unordered_map<string, lf_algo_stock*> symbolMap_;  // 所有股票对象
		std::vector<lf_algo_stock*> algoByCidx_;  // OPT_LOOP: cidx -> 策略对象，O(1) 直取
		std::atomic_int oseq_;  // 委托序号
		boost::lockfree::queue<lquant_order_t> order_update_;  // 委托回报队列
};

extern "C" {
	quant_strategy_base* create_strategy();
}
