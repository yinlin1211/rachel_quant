#pragma once

#include "lf_quant_strategy_base.h"
#include "lf_util.h"
#include "lquant_log.hpp"
#include "lquant_tick_pool.hpp"
using namespace lfquant;

struct trade_signal {
	long didx;	// 日期索引
	long tidx;	// 行索引
	long cidx;	// 股票索引
	float score;  // 分数
};

struct lf_stock_info_t {
	int volTraded;	// 已完成的总数量
	int volTraded_B;	// 已完成的买入数量
	int volTraded_S;	// 已完成的卖出数量
	double amtTraded;  // 已完成的金额
	int* posToTrade;  // 各段结束时需要完成量的数组
	float* posTimeRate;  // 各段结束时的时间比例
	bool hasOrder;  // 是否存在委托
	char stkState;  // 当前策略状态

	bool hasPrice;  // 是否已有正常价格
	int latestTime;  // 最新行情时间ms
	int askPrice1;  // 卖一价
	int askVolume1;	 // 卖一挂单量
	int bidPrice1;	// 买一价
	int bidVolume1;	 // 买一挂单量
	int lastPrice;	// 最新价

	int odFilledQty;  // 当前委托的已完成量
	int lastOpenReqTime;  // 上一次发出委托的时间ms
	int cancel_counts;  // 当前委托取消次数
	int last_cancel_time;  // 当前委托上次提交撤单时间ms
	float latestTimeRate;  // 最新时间比例
	
	float score;  // 信号分数
	int signalTime;  // 信号时间ms
};

struct lf_timeConfig {
	// 策略中全部只读不写
	bool isReal;  // true实时，false回测
	int startTime;  // 篮子起始时间hhmmssxxx
	int endTime;  // 篮子结束时间hhmmssxxx
	int startTimeMsc;  // 篮子起始时间ms
	int endTimeMsc;  // 篮子结束时间ms
	int basketTmRange;  // 篮子持续时间ms
	int normalTimeMsc;  // 正常交易时间ms
	int forceTmRange;  // 强制扫单时间s
	int normalOrderTimeInterval;  // 正常交易最小下单间隔ms
	int forceOrderTimeInterval;  // 扫单最小下单间隔ms
	int cancelOrderTimeInterval;  // 撤单时间间隔限制ms
	int delayTimeLimit;  // 行情和信号时间间隔限制ms
};

struct lf_tradeConfig {
	// 策略中只读不写
	int commonVol;  // 每只股票单笔常规委托量
	float amountPerTrade;  // 每只股票单笔最大委托金额
	int volumePerTrade;  // 每只股票单笔最大委托量

	// 策略中只写一次
	int totalVolumeToTrade;  // 篮子目标总成交量
	float portExposureLimit;  // 篮子强制敞口限制

	// 策略中动态读写
	bool isPause;  // 暂停发单
	int currentVolumeTraded;  // 篮子当前已完成的数量
	float currentExposure;  // 篮子当前实际敞口

	// 策略相关
	std::vector<string> scoreSymbol;
	float* baseThreshold_buy;
	float* baseThreshold_sell;
};

typedef std::function<void(const lquant_order_t&)> lf_ats_request_fun;
typedef std::function<int()> lf_ats_oseq_fun;

class lf_algo_stock {
	public:
		lf_algo_stock() {};
		virtual ~lf_algo_stock() {};

		void init(const std::string& tkr, int qty, lf_timeConfig* tmconfig, lf_tradeConfig* trdconfig);
		void process_trader_state();
		void on_tick(int type, void* p);
		void on_signal(trade_signal* sig);
		void onRecvExchOrder(const lquant_order_t& report);

		void set_basket_id(const std::string& id) { basketId = id; }
		void set_tick_pool(lquant_tick_pool* pool) { pool_ = pool; }
		void set_request_callback(lf_ats_request_fun fun) { on_request_ = fun; }
		void set_oseq_callback(lf_ats_oseq_fun fun) { on_oseq_ = fun; }

	private:
		void process_state_none();
		void process_state_new();

		void allocateSliceVol();
		bool is_can_trade();
		void make_order(char side, float prc, int qty);
		void send_order(const lquant_order_t& order);
		float prep_od_price(char BSflag);
		int prep_od_volume(float price, char BSflag, float exposure, int allRemain);
		void cancel_order();
		void clean_order();

		void recv_report_state_pending_new(const lquant_order_t& report);
		void recv_report_state_new(const lquant_order_t& report);
		void recv_report_state_pending_cancel(const lquant_order_t& report);
		void apply_cancel(const lquant_order_t& report);
		void apply_reject(const lquant_order_t& report);
		void apply_trade_trans(const lquant_order_t& report);

	private:
		std::string stkSymbol;  // 股票代码
		int stkTotalVol;  // 目标总成交量
		int stkSlotSize;  // 最小交易股数
		int stkNumSlice;  // 分段数
		int stkTypClose;  // 典型价，10.00的股票stkTypClose是100000
		float stkBuyThres;  // 买的信号阈值
		float stkSellThres;  // 卖的信号阈值

		lf_timeConfig* tmConfig = nullptr;
		lf_tradeConfig* trdConfig = nullptr;
		lf_stock_info_t stockInfo;
		lquant_order_t order_;
		trade_signal signal;
		lquant_tick_pool* pool_ = nullptr;
		lf_ats_request_fun on_request_ = nullptr;  // 委托指令回调句柄
		lf_ats_oseq_fun on_oseq_ = nullptr;  // oseq生成句柄
		std::string strtName;
		std::string basketId;
};
