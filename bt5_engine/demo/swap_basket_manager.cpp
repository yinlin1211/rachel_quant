#include "swap_basket_manager.h"
#include <chrono>
#include <cstdio>

void SWapBasketManager::start(const lf_basket& basket) {
	LOG_INFO("SWapBasketManager REAL start. algoInstanceId:{}", algoInstanceId_);
	is_real = true;
	basket_ = basket;

	init_basket();
	std::thread(&SWapBasketManager::real_thread, this).detach();

	LOG_INFO("SWapBasketManager REAL finish. algoInstanceId:{}", algoInstanceId_);
}


void SWapBasketManager::init_basket() {
	lquant::XmlNode cfg("config.xml");

	tmconfig = new lf_timeConfig();
	trdconfig = new lf_tradeConfig();
	memset(tmconfig, 0, sizeof(lf_timeConfig));
	memset(trdconfig, 0, sizeof(lf_tradeConfig));
	
	tmconfig->isReal = is_real;
	lquant::XmlNode config = cfg.getChild("Config");
	tmconfig->forceTmRange = config.getAttrDefault("forceTmRange", 300);
	tmconfig->normalOrderTimeInterval = config.getAttrDefault("normalOrderTimeInterval", 5000);
	tmconfig->forceOrderTimeInterval = config.getAttrDefault("forceOrderTimeInterval", 30000);
	tmconfig->cancelOrderTimeInterval = config.getAttrDefault("cancelOrderTimeInterval", 30000);
	tmconfig->delayTimeLimit = config.getAttrDefault("delayTimeLimit", 5000);

	trdconfig->isPause = false;
	trdconfig->commonVol = config.getAttrDefault("commonVol", 3000);
	trdconfig->amountPerTrade = config.getAttrDefault("amountPerTrade", 300000.f);
	trdconfig->volumePerTrade = config.getAttrDefault("volumePerTrade", 1000000);

	if (tmconfig->isReal) {
		lquant::XmlNode tick = cfg.getChild("Tick");
		tickPath_ = tick.getAttrDefault("path", "");
		scoreFile_ = tick.getAttrDefault("score_file", "");
	}
	tick_data_.init(tickPath_);
	signal_reader_.load(scoreFile_);
	tkr_list = tick_data_.get_tkrs();

	// 读取阈值
	std::string scoreFilePath = "scoreThreshold.csv";
	ifstream inFile(scoreFilePath, ios::in);
	if (!inFile) {
		LOG_INFO("ERROR scoreThreshold file not found");
	}
	unsigned int scoreLen = 0;
	std::string line;
	std::string field;
	while (getline(inFile, line)) {
		scoreLen++;
	}
	inFile.close();
	trdconfig->baseThreshold_buy = new float[scoreLen];
	memset(trdconfig->baseThreshold_buy, 0, sizeof(float) * scoreLen);
	trdconfig->baseThreshold_sell = new float[scoreLen];
	memset(trdconfig->baseThreshold_sell, 0, sizeof(float) * scoreLen);
	inFile.open(scoreFilePath, ios::in);
	unsigned int row_i = 0;
	while (getline(inFile, line)) {
		istringstream tempLine(line);
		getline(tempLine, field, ',');
		trdconfig->scoreSymbol.push_back(field);
		getline(tempLine, field, ',');
		trdconfig->baseThreshold_sell[row_i] = atof(field.c_str());
		getline(tempLine, field);
		trdconfig->baseThreshold_buy[row_i] = atof(field.c_str());
		row_i++;
	}
	inFile.close();

	int startTime_ = max(tmconfig->isReal ? current() : 0, basket_.startTime);
	int endTime_ = basket_.endTime;
	if (startTime_ < 93000)
		startTime_ = 93000;
	if (startTime_ >= 113000 && startTime_ < 130000)
		startTime_ = 130000;
	if (endTime_ > 113000 && endTime_ <= 130000)
		endTime_ = 113000;
	if (endTime_ > 150000)
		endTime_ = 150000;
	if (startTime_ >= endTime_) {
		LOG_ERROR("({} {}) time error", startTime_, endTime_);
		return;
	}
	LOG_INFO("basket time:basketId:{},startTime:{},endTime:{}", basket_.basketId, startTime_, endTime_);

	tmconfig->startTime = startTime_ * 1000;
	tmconfig->endTime = endTime_ * 1000;
	tmconfig->startTimeMsc = hhmmssxxx_to_ms(tmconfig->startTime);
	tmconfig->endTimeMsc = hhmmssxxx_to_ms(tmconfig->endTime);
	tmconfig->basketTmRange = tmconfig->endTimeMsc - tmconfig->startTimeMsc;
	assert(tmconfig->basketTmRange > 0);
	if (tmconfig->endTime > 130000000 && tmconfig->startTime < 113000000)
		tmconfig->basketTmRange -= 5400000;
	tmconfig->normalTimeMsc = tmconfig->endTimeMsc - tmconfig->forceTmRange * 1000;

	trdconfig->portExposureLimit = basket_.tradeExposure;
	trdconfig->currentExposure = 0;

	for (lf_stock& s: basket_.stocks) {
		if (s.volume != 0) {
			lf_algo_stock* las = new lf_algo_stock();
			las->set_basket_id(basket_.basketId);
			las->set_tick_pool(tick_data_.get_pool());
			las->set_request_callback(std::bind(&SWapBasketManager::send_order, this, std::placeholders::_1));
			las->set_oseq_callback(std::bind(&SWapBasketManager::generate_oseq, this));
			las->init(s.symbol, s.volume, tmconfig, trdconfig);
			symbolMap_[s.symbol] = las;
		}
	}

	// OPT_LOOP: 预建 cidx -> 策略对象数组，避免逐 tick 做字符串哈希查找
	algoByCidx_.assign(tkr_list.size(), nullptr);
	{
		std::unordered_map<std::string, int> tkr2cidx;
		tkr2cidx.reserve(tkr_list.size() * 2);
		for (size_t i = 0; i < tkr_list.size(); ++i)
			tkr2cidx.emplace(tkr_list[i], (int)i);
		for (auto& kv : symbolMap_) {
			auto it = tkr2cidx.find(kv.first);
			if (it != tkr2cidx.end())
				algoByCidx_[it->second] = kv.second;
		}
	}

	LOG_INFO("SWapBasketManager basket init finish");
}


void SWapBasketManager::real_thread() {
	trade_signal* signal_p = signal_reader_.getDb();  // 信号起始指针
	int signal_pre_idx = 0;  // 信号上次读数的索引
	int signal_curr_idx = 0;  // 信号当前读数的索引
	lquant_tick_pool* pool = tick_data_.get_pool();  // 行情文件
	long tick_pre_idx = 0;  // 行情上次读数的索引
	long tick_curr_idx = 0;  // 行情当前读数的索引
	int nStocks = static_cast<int>(tkr_list.size());  // tkr股票数
	int cidx, idx;
	GDataType type;
	GDataValue val;

	while (true) {
		if (isStopped_ || hhmmss_to_s(current()) > hhmmss_to_s(tmconfig->endTime / 1000) + 30)
			break;

		// 更新信号
		signal_curr_idx = signal_reader_.getIndex();
		while (signal_pre_idx < signal_curr_idx) {
			signal_pre_idx++;
			trade_signal& s = *(signal_p + signal_pre_idx);
			if (s.cidx >= 0 && s.cidx < nStocks) {  // cidx有效
				auto it = symbolMap_.find(tkr_list[s.cidx]);
				if (it != symbolMap_.end()) {  // cidx在篮子里
					auto* las = it->second;
					las->on_signal(&s);
				}
			}
		}

		// 更新报文
		update_orders();
		
		// 更新行情
		tick_curr_idx = pool->get_last_didx();
		while (tick_pre_idx < tick_curr_idx) {
			tick_pre_idx++;
			tuple<int, int, GDataType, GDataValue> ret = pool->get_data(tick_pre_idx);
			tie(cidx, idx, type, val) = ret;

			if (cidx >= 0 && cidx < nStocks) {
				auto it = symbolMap_.find(tkr_list[cidx]);
				if (it != symbolMap_.end()) {
					void* obj = nullptr;
					if (type == TYPE_MD) {
						obj = pool->get_md(cidx, idx);
					}
					else if (type == TYPE_TS) {
						obj = pool->get_ts(cidx, idx);
					}
					else if (type == TYPE_OD) {
						obj = pool->get_od(cidx, idx);
					}

					auto* las = it->second;
					las->on_tick(type, obj);
					las->process_trader_state();
				}
			}
		}

		usleep(1);
	}
}


void SWapBasketManager::update_orders() {
	for(int i = 0; i < 50; i++) {  // 读固定数量的委托回报
		if(order_update_.empty())
			break;

		lquant_order_t order;
		order_update_.pop(order);
		auto it = symbolMap_.find(order.symbol);
		if(it != symbolMap_.end())
			it->second->onRecvExchOrder(order);
		else
			LOG_ERROR("unknown report algoInstanceId:{} symbol:{} oseq:{}", algoInstanceId_, order.symbol, order.oseq);
	}
}


void SWapBasketManager::send_order(const lquant_order_t& order) {
	if (tmconfig->isReal) {
		on_request_(algoInstanceId_, order, "");
	} else {
		pbt_->on_request(order);
	}
}


void SWapBasketManager::on_response(const lquant_order_t& order) {
	order_update_.push(order);
}


void SWapBasketManager::start_sim(lf_basket basket, int tradeDate, std::string tickPath, std::string scoreFile) {
	LOG_INFO("SWapBasketManager SIM start. algoInstanceId:{}", algoInstanceId_);
	is_real = false;
	basket_ = basket;
	tick_data_.set_day(tradeDate);
	tickPath_ = tickPath;
	scoreFile_ = scoreFile;

	pbt_->set_response_callback(std::bind(&SWapBasketManager::on_response, this, std::placeholders::_1));
	init_basket();
	int startTime = basket_.startTime;
	int endTime = basket_.endTime;
	
	lquant_tick_pool* pool = tick_data_.get_pool();
	long tp_last_tidx = pool->get_last_didx();
	unsigned int cidx, idx;
	GDataType type;
	GDataValue val;
	
	// 寻找回测开始的tidx
	int start_i = 0;
	int end_i = tp_last_tidx;
	while (start_i <= end_i) {
		int mid_i = (start_i + end_i) / 2;
		tuple<int, int, GDataType, GDataValue> ret = pool->get_data(mid_i);
		tie(cidx, idx, type, val) = ret;

		int nTime_i = 0;
		if (type == TYPE_MD) {
			auto tp = pool->get_md(cidx, idx);
			nTime_i = tp->nTime;
		}
		else if (type == TYPE_TS) {
			auto tp = pool->get_ts(cidx, idx);
			nTime_i = tp->nTime;
		}
		else if (type == TYPE_OD) {
			auto tp = pool->get_od(cidx, idx);
			nTime_i = tp->nTime;
		}

		if (nTime_i == 0) {
			start_i += 1;
		}
		else {
			if (nTime_i / 1000 <= startTime - 200)  // 收敛至篮子开始前2分钟
				start_i = mid_i + 1;
			else
				end_i = mid_i - 1;
		}
	}
	int curr_tidx = start_i;

	trade_signal* p = signal_reader_.getDb();
	int sgn_last_idx = signal_reader_.getIndex();
	int sgn_i = 0;
	int newest_time = 0;
#ifdef OPT_LOOP
	// 二分找到 endTime 对应的最后一条 tick，省掉逐条时间判断
	long end_tidx = tp_last_tidx;
	{
		long lo = curr_tidx, hi = tp_last_tidx;
		while (lo <= hi) {
			long mid = (lo + hi) / 2;
			auto ret = pool->get_data(mid);
			unsigned int c2, i2; GDataType t2; GDataValue v2;
			tie(c2, i2, t2, v2) = ret;
			int nt = 0;
			if (t2 == TYPE_MD) nt = pool->get_md(c2, i2)->nTime;
			else if (t2 == TYPE_TS) nt = pool->get_ts(c2, i2)->nTime;
			else if (t2 == TYPE_OD) nt = pool->get_od(c2, i2)->nTime;
			if (nt == 0) { lo = mid + 1; }
			else if (nt / 1000 <= endTime) { lo = mid + 1; }
			else { hi = mid - 1; }
		}
		end_tidx = lo - 1;
		if (end_tidx > tp_last_tidx) end_tidx = tp_last_tidx;
	}

	auto t0 = std::chrono::steady_clock::now();
	long n_ticks = 0, n_basket = 0;
	for (; curr_tidx <= end_tidx; curr_tidx++) {
		n_ticks++;
		// 更新信号
		while (sgn_i < sgn_last_idx) {
			trade_signal& s = p[sgn_i];
			if (s.tidx > curr_tidx)
				break;

			if (s.tidx == curr_tidx) {
				if (s.cidx >= 0 && s.cidx < (long)tkr_list.size()) {
					auto it = symbolMap_.find(tkr_list[s.cidx]);
					if (it != symbolMap_.end()) {
						auto* las = it->second;
						las->on_signal(&s);
					}
				}
			}

			sgn_i++;
		}

		auto ret = pool->get_data(curr_tidx);
		tie(cidx, idx, type, val) = ret;

		// 非篮子股票直接跳过：不再 get_md/ts/od，不再做字符串哈希
		lf_algo_stock* las = (cidx < algoByCidx_.size()) ? algoByCidx_[cidx] : nullptr;
		if (!las)
			continue;
		n_basket++;

		void* obj = nullptr;
		int nt = 0;
		if (type == TYPE_MD) {
			auto tp = pool->get_md(cidx, idx);
			nt = tp->nTime;
			obj = tp;
		}
		else if (type == TYPE_TS) {
			auto tp = pool->get_ts(cidx, idx);
			nt = tp->nTime;
			obj = tp;
		}
		else if (type == TYPE_OD) {
			auto tp = pool->get_od(cidx, idx);
			nt = tp->nTime;
			obj = tp;
		}

		if (nt == 0 || nt / 1000 < startTime || nt / 1000 > endTime)
			continue;

		if (!order_update_.empty())
			update_orders();
		las->on_tick(type, obj);
		las->process_trader_state();
		pbt_->on_tick(pool, curr_tidx, tkr_list[cidx]);
	}
	auto t1 = std::chrono::steady_clock::now();
	double sec = std::chrono::duration<double>(t1 - t0).count();
	LOG_INFO("[BENCH] mode=opt ticks:{} basket:{} loop_sec:{} per_tick_us:{}",
	         n_ticks, n_basket, sec, n_ticks > 0 ? sec * 1e6 / n_ticks : 0.0);
	fprintf(stderr, "[BENCH] mode=opt ticks=%ld basket=%ld loop_sec=%.3f per_tick_us=%.3f\n",
	        n_ticks, n_basket, sec, n_ticks > 0 ? sec * 1e6 / n_ticks : 0.0);
#else
	auto t0 = std::chrono::steady_clock::now();
	long n_ticks = 0, n_basket = 0;
	for (; curr_tidx <= tp_last_tidx; curr_tidx++) {
		n_ticks++;
		// 更新信号
		while (sgn_i < sgn_last_idx) {
			trade_signal& s = p[sgn_i];
			if (s.tidx > curr_tidx)
				break;

			if (s.tidx == curr_tidx) {
				if (s.cidx >= 0 && s.cidx < tkr_list.size()) {  // cidx有效
					auto it = symbolMap_.find(tkr_list[s.cidx]);
					if (it != symbolMap_.end()) {  // cidx在篮子里
						auto* las = it->second;
						las->on_signal(&s);
					}
				}
			}

			sgn_i++;
		}

		// 更新行情
		tuple<int, int, GDataType, GDataValue> ret = pool->get_data(curr_tidx);
		tie(cidx, idx, type, val) = ret;

		void* obj = nullptr;
		if (type == TYPE_MD) {
			auto tp = pool->get_md(cidx, idx);
			newest_time = tp->nTime;
			obj = tp;
		}
		else if (type == TYPE_TS) {
			auto tp = pool->get_ts(cidx, idx);
			newest_time = tp->nTime;
			obj = tp;
		}
		else if (type == TYPE_OD) {
			auto tp = pool->get_od(cidx, idx);
			newest_time = tp->nTime;
			obj = tp;
		}

		if (newest_time / 1000 < startTime)
			continue;
		if (newest_time / 1000 > endTime)
			break;

		// 执行本次流程
		auto it = symbolMap_.find(tkr_list[cidx]);
		lf_algo_stock* las = it == symbolMap_.end() ? nullptr : it->second;
		if (las) {  // 本次cidx是篮子里的股票
			n_basket++;
			update_orders();
			las->on_tick(type, obj);
			las->process_trader_state();
			pbt_->on_tick(pool, curr_tidx, tkr_list[cidx]);
		}
	}
	auto t1 = std::chrono::steady_clock::now();
	double sec = std::chrono::duration<double>(t1 - t0).count();
	LOG_INFO("[BENCH] mode=base ticks:{} basket:{} loop_sec:{} per_tick_us:{}",
	         n_ticks, n_basket, sec, n_ticks > 0 ? sec * 1e6 / n_ticks : 0.0);
	fprintf(stderr, "[BENCH] mode=base ticks=%ld basket=%ld loop_sec=%.3f per_tick_us=%.3f\n",
	        n_ticks, n_basket, sec, n_ticks > 0 ? sec * 1e6 / n_ticks : 0.0);
#endif

	LOG_INFO("SWapBasketManager SIM finish. algoInstanceId:{}", algoInstanceId_);
}


quant_strategy_base* create_strategy() {
	LOG_INFO("start create quant_strategy so SWapBasketManager");
	return new SWapBasketManager();
}
