#include "lf_algo_stock.h"

void lf_algo_stock::init(const std::string& tkr, int qty, lf_timeConfig* tmconfig, lf_tradeConfig* trdconfig) {
	stkSymbol = tkr;
	stkTotalVol = std::abs(qty);
	tmConfig = tmconfig;
	trdConfig = trdconfig;

	trdConfig->totalVolumeToTrade += stkTotalVol;

	if (stoi(stkSymbol) >= 688000)
		stkSlotSize = 200;
	else
		stkSlotSize = 100;
	
	memset(&stockInfo, 0, sizeof(stockInfo));
	stockInfo.hasOrder = false;

	vector<string>::iterator iter = std::find(trdConfig->scoreSymbol.begin(), trdConfig->scoreSymbol.end(), stkSymbol);
	if (iter == trdConfig->scoreSymbol.end()) {
		LOG_INFO("ERROR unknown score tkr: {}", stkSymbol);
	} else {
		int scoreIdx = iter - trdConfig->scoreSymbol.begin();
		stkBuyThres = trdConfig->baseThreshold_buy[scoreIdx];
		stkSellThres = trdConfig->baseThreshold_sell[scoreIdx];
	}

	memset(&order_, 0, sizeof(order_));
	memset(&signal, 0, sizeof(signal));
	stockInfo.stkState = ORD_NONE;  // 状态转换的核心

	LOG_INFO("stock init done:basketId:{},symbol:{},vol:{},buyThres:{},sellThres:{}", basketId, stkSymbol, stkTotalVol, stkBuyThres, stkSellThres);
}


void lf_algo_stock::process_trader_state() {
	switch (stockInfo.stkState) {
		case ORD_NONE:
			process_state_none();
			break;
		case ORD_PENDING_NEW:
			break;
		case ORD_NEW:
			process_state_new();
			break;
		case ORD_PENDING_CANCEL:
			break;
		default:
			LOG_INFO("ERROR unknown stkState:{}", stockInfo.stkState);
	}
}


void lf_algo_stock::process_state_none() {
	if (!is_can_trade())
		return;

	int otype = -1;
	char side = '?';
	float prc = 0;
	int qty = 0;
	float exposure = trdConfig->currentExposure;
	int expore_qty = stockInfo.volTraded_B - stockInfo.volTraded_S;
	float score = stockInfo.score;

	if (abs(expore_qty) >= stkSlotSize) {  // 存在敞口，走平仓逻辑
		if (stockInfo.latestTime >= tmConfig->normalTimeMsc) {  // 超过了正常交易时间，强制平仓
			if (expore_qty <= stkSlotSize * -1) {  // 强制买入
				otype = 0;
				side = 'B';
				prc = max(stockInfo.askPrice1, stockInfo.lastPrice) / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
			else if (expore_qty >= stkSlotSize) {  // 强制卖出
				otype = 0;
				side = 'S';
				prc = min(stockInfo.bidPrice1, stockInfo.lastPrice) / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
		}
		else if (stockInfo.askPrice1 <= 0 || stockInfo.bidPrice1 <= 0) {  // 涨跌停，强制平仓
			if (expore_qty <= stkSlotSize * -1 && stockInfo.bidPrice1 <= 0) {  // 跌停买入
				otype = 1;
				side = 'B';
				prc = stockInfo.askPrice1 / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
			else if (expore_qty >= stkSlotSize && stockInfo.askPrice1 <= 0) {  // 涨停卖出
				otype = 1;
				side = 'S';
				prc = stockInfo.bidPrice1 / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
		}
		else if (stockInfo.latestTime - stockInfo.lastOpenReqTime >= 600000) {  // 持仓超过10分钟，条件平仓
			if (expore_qty <= stkSlotSize * -1 && score > 0) {  // 买入
				otype = 2;
				side = 'B';
				prc = stockInfo.askPrice1 / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
			else if (expore_qty >= stkSlotSize && score < 0) {  // 卖出
				otype = 2;
				side = 'S';
				prc = stockInfo.bidPrice1 / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
		}
		else if (stockInfo.latestTime - stockInfo.lastOpenReqTime >= 180000) {  // 持仓超过3分钟，条件平仓
			if (expore_qty <= stkSlotSize * -1 && score > stkBuyThres * 0.9) {  // 买入
				otype = 3;
				side = 'B';
				prc = stockInfo.askPrice1 / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
			else if (expore_qty >= stkSlotSize && score < stkSellThres * 0.9) {  // 卖出
				otype = 3;
				side = 'S';
				prc = stockInfo.bidPrice1 / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
		}
		else if (stockInfo.latestTime - stockInfo.lastOpenReqTime >= 120000) {  // 持仓超过2分钟，条件平仓
			if (expore_qty <= stkSlotSize * -1 && score > stkBuyThres) {  // 买入
				otype = 4;
				side = 'B';
				prc = stockInfo.askPrice1 / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
			else if (expore_qty >= stkSlotSize && score < stkSellThres) {  // 卖出
				otype = 4;
				side = 'S';
				prc = stockInfo.bidPrice1 / 10000.0;
				qty = abs(expore_qty) / stkSlotSize * stkSlotSize;
			}
		}
	}
	else {  // 未有敞口，走开仓逻辑
		int allRemain = stkTotalVol - stockInfo.volTraded;
		if (allRemain >= stkSlotSize * 2) {
			if (score > stkBuyThres) {
				otype = 10;
				side = 'B';
				prc = prep_od_price(side);
				qty = prep_od_volume(prc, side, exposure, allRemain);
			}
			else if (score < stkSellThres) {
				otype = 10;
				side = 'S';
				prc = prep_od_price(side);
				qty = prep_od_volume(prc, side, exposure, allRemain);
			}
		}
	}

	if (side == '?' || isnormal(prc) == 0 || qty < stkSlotSize)
		return;

	make_order(side, prc, qty);
	send_order(order_);

	LOG_INFO("send new order:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{},otype:{},orderTime:{},tickTime:{},tickPrc:{},signalTime:{},score:{},askPrc1:{},askVol1:{},bidPrc1:{},bidVol1:{}",
		basketId, stkSymbol, order_.side, order_.qty, order_.prc, order_.oseq, otype, order_.tm, ms_to_hhmmssxxx(stockInfo.latestTime), stockInfo.lastPrice, ms_to_hhmmssxxx(stockInfo.signalTime), stockInfo.score,
		stockInfo.askPrice1, stockInfo.askVolume1, stockInfo.bidPrice1, stockInfo.bidVolume1);
}


bool lf_algo_stock::is_can_trade() {
	// 未到计划开始时间
	if (stockInfo.latestTime < tmConfig->startTimeMsc)
		return false;

	// 超过计划结束时间
	if (stockInfo.latestTime > tmConfig->endTimeMsc)
		return false;

	// 暂停发单
	if (trdConfig->isPause)
		return false;

	// 未有价格
	if (!stockInfo.hasPrice)
		return false;

	// 已有外挂单
	if (stockInfo.hasOrder)
		return false;

	// 已完成
	if (stkTotalVol - stockInfo.volTraded <= 0)
		return false;

	// 小于下单间隔, 正常下单时间
	if (stockInfo.latestTime < tmConfig->normalTimeMsc && stockInfo.latestTime - stockInfo.lastOpenReqTime < tmConfig->normalOrderTimeInterval)
		return false;

	// 小于下单间隔, 补单时间
	if (stockInfo.latestTime >= tmConfig->normalTimeMsc && stockInfo.latestTime - stockInfo.lastOpenReqTime < tmConfig->forceOrderTimeInterval)
		return false;

	// 信号分值错误
	if (abs(signal.score) > 20)
		return false;

	// tickpool文件错误
	if (pool_ == nullptr) {
		LOG_INFO("ERROR cannot find pool");
		return false;
	}

	tuple<int, int, GDataType, GDataValue> ret = pool_->get_data(signal.tidx);
	int cidx, idx;
	GDataType type;
	GDataValue val;
	tie(cidx, idx, type, val) = ret;
	int time = 0;
	if (type == TYPE_MD) {
		auto md = val.md;
		time = md->nTime;
	}
	else if (type == TYPE_TS) {
		auto ts = val.ts;
		time = ts->nTime;
	}
	else if (type == TYPE_OD) {
		auto od = val.od;
		time = od->nTime;
	}

	// 行情和信号时间间隔相差太多
	if (tmConfig->isReal) {
		int timeDelay = stockInfo.latestTime - hhmmssxxx_to_ms(time);
		if (timeDelay > tmConfig->delayTimeLimit) {
			return false;
		}
	}

	stockInfo.score = signal.score;
	stockInfo.signalTime = hhmmssxxx_to_ms(time);

	return true;
}


void lf_algo_stock::make_order(char side, float prc, int qty) {
	memcpy(order_.symbol, stkSymbol.c_str(), sizeof(order_.symbol));
	strncpy(order_.algo, strtName.c_str(), sizeof(order_.algo));
	order_.idnum = -1;
	order_.pid = -1;
	order_.oseq = on_oseq_();

	order_.tm = tmConfig->isReal ? current() * 1000 : ms_to_hhmmssxxx(stockInfo.latestTime);
	order_.type = lquant_ORDER_TYPE_ORDER_ENTER;
	order_.ocflag = (side == 'B' ? 'O' : 'C');
	order_.qty = qty;
	order_.prc = prc;
	order_.side = side;

	stockInfo.hasOrder = true;
	stockInfo.odFilledQty = 0;
	stockInfo.lastOpenReqTime = stockInfo.latestTime;
	stockInfo.cancel_counts = 0;
	stockInfo.last_cancel_time = 0;
	if (side == 'B') {
		trdConfig->currentExposure += order_.qty * order_.prc;
	} else {
		trdConfig->currentExposure -= order_.qty * order_.prc;
	}
	stockInfo.stkState = ORD_PENDING_NEW;
}


void lf_algo_stock::send_order(const lquant_order_t& order) {
	on_request_(order);
}


float lf_algo_stock::prep_od_price(char BSflag) {
	if (BSflag == 'B') {
		return stockInfo.askPrice1 / 10000.0;
	} else {
		return stockInfo.bidPrice1 / 10000.0;
	}
}


int lf_algo_stock::prep_od_volume(float price, char BSflag, float exposure, int allRemain) {
	if (price <= 0) {
		return 0;
	}

	float amtLimit = trdConfig->amountPerTrade;
	if (BSflag == 'B') {
		if (exposure >= trdConfig->portExposureLimit)
			return 0;
		amtLimit = min(amtLimit, trdConfig->portExposureLimit - exposure);
	}

	int volume = amtLimit / price;
	volume = min(volume, allRemain / 2);
	if (stkTypClose > 100000) {
		volume = min(volume, trdConfig->commonVol);
	}

	if (BSflag == 'B') {
		volume = min(volume, stockInfo.askVolume1);
	} else {
		volume = min(volume, stockInfo.bidVolume1);
	}
	volume = min(volume, trdConfig->volumePerTrade);

	if (stoi(stkSymbol) < 688000) {
		volume = volume / stkSlotSize * stkSlotSize;  // 转为整数倍
	}
	return volume;
}


void lf_algo_stock::process_state_new() {
	int serverTime = stockInfo.latestTime;
	if (serverTime - hhmmssxxx_to_ms(order_.tm) > tmConfig->cancelOrderTimeInterval && stockInfo.cancel_counts < 3 && serverTime - stockInfo.last_cancel_time >= 3000) {
		cancel_order();
		LOG_INFO("request cancel:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{},tickTime:{},tickPrc:{}",
			basketId, stkSymbol, order_.side, order_.qty, order_.prc, order_.oseq, ms_to_hhmmssxxx(stockInfo.latestTime), stockInfo.lastPrice);
	}
}


void lf_algo_stock::cancel_order() {
	stockInfo.last_cancel_time = stockInfo.latestTime;
	stockInfo.cancel_counts += 1;
	stockInfo.stkState = ORD_PENDING_CANCEL;

	lquant_order_t order(order_);
	order.type = 'X';
	send_order(order);
}


void lf_algo_stock::clean_order() {
	memset(&order_, 0, sizeof(lquant_order_t));
	stockInfo.stkState = ORD_NONE;
	stockInfo.hasOrder = false;
}


void lf_algo_stock::on_tick(int type, void* p) {
	if (!p)
		return;

	if (type == TYPE_MD) {
		auto md = (lquant_market_data_t*)p;
		stockInfo.latestTime = hhmmssxxx_to_ms(md->nTime);
		stockInfo.askPrice1 = md->nAskPrice[0];
		stockInfo.askVolume1 = md->nAskVol[0];
		stockInfo.bidPrice1 = md->nBidPrice[0];
		stockInfo.bidVolume1 = md->nBidVol[0];
	}
	else if (type == TYPE_TS) {
		auto ts = (lquant_transaction_t*)p;
		stockInfo.latestTime = hhmmssxxx_to_ms(ts->nTime);
		if (ts->nPrice > 0) {
			stockInfo.lastPrice = ts->nPrice;
			if (!stockInfo.hasPrice) {  // 只在开盘时做一次
				stkTypClose = stockInfo.lastPrice;
				float price = stkTypClose / 10000.0;
				stockInfo.hasPrice = true;
			}
		}
	}
	else if (type == TYPE_OD) {
		auto od = (lquant_tdf_order_t*)p;
		stockInfo.latestTime = hhmmssxxx_to_ms(od->nTime);
	}

	int currentMsc = stockInfo.latestTime - tmConfig->startTimeMsc;
	if (tmConfig->startTimeMsc < 41400000 && stockInfo.latestTime >= 46800000)
		currentMsc -= 5400000;
	stockInfo.latestTimeRate = 1.0 * currentMsc / tmConfig->basketTmRange;
}


void lf_algo_stock::on_signal(trade_signal* sig) {
	memcpy(&signal, sig, sizeof(trade_signal));
}


void lf_algo_stock::onRecvExchOrder(const lquant_order_t& report) {
	switch(stockInfo.stkState) {
		case ORD_NONE:
			LOG_INFO("ERROR state_none unexpected:basketId:{},symbol:{},type:{},tseq:{}", basketId, stkSymbol, report.type, report.tseq);
			break;
		case ORD_PENDING_NEW:
			recv_report_state_pending_new(report);
			break;
		case ORD_NEW:
			recv_report_state_new(report);
			break;
		case ORD_PENDING_CANCEL:
			recv_report_state_pending_cancel(report);
			break;
		default:
			LOG_INFO("ERROR unknown stkState:basketId:{},symbol:{},state:{}", basketId, stkSymbol, stockInfo.stkState);
	}
}


void lf_algo_stock::recv_report_state_pending_new(const lquant_order_t& report) {
	switch (report.type) {
		case 'A':
			stockInfo.stkState = ORD_NEW;
			LOG_INFO("order received:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{},reportTime:{},tickTime:{},tickPrc:{}",
				basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq, report.tm, ms_to_hhmmssxxx(stockInfo.latestTime), stockInfo.lastPrice);
			break;
		case 'C':
			LOG_INFO("ERROR state_pending_new C");
			apply_cancel(report);
			break;
		case 'J':
			apply_reject(report);
			break;
		case 'E':
			LOG_INFO("ERROR state_pending_new E");
			apply_trade_trans(report);
			break;
		case 'U':
			LOG_INFO("ERROR state_pending_new U:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{}", basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq);
			break;
		default:
			LOG_INFO("ERROR state_pending_new unexpected:basketId:{},symbol:{},type:{},tseq:{}", basketId, stkSymbol, report.type, report.tseq);
	}
}


void lf_algo_stock::apply_cancel(const lquant_order_t& report) {
	if (report.side == 'B') {
		trdConfig->currentExposure -= report.qty * report.prc;
	}
	else if (report.side == 'S') {
		trdConfig->currentExposure += report.qty * report.prc;
	}
	else {
		LOG_INFO("ERROR unexpected report side:basketId:{},symbol:{},side:{}", basketId, stkSymbol, report.side);
	}
	LOG_INFO("order canceled:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{},reportTime:{},tickTime:{}",
		basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq, report.tm, ms_to_hhmmssxxx(stockInfo.latestTime));
	clean_order();
}


void lf_algo_stock::apply_reject(const lquant_order_t& report) {
	if (report.side == 'B') {
		trdConfig->currentExposure -= report.qty * report.prc;
	}
	else if (report.side == 'S') {
		trdConfig->currentExposure += report.qty * report.prc;
	}
	else {
		LOG_INFO("ERROR unexpected report side:basketId:{},symbol:{},side:{}", basketId, stkSymbol, report.side);
	}
	LOG_INFO("order rejected:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{},reportTime:{},tickTime:{}",
		basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq, report.tm, ms_to_hhmmssxxx(stockInfo.latestTime));
	clean_order();
}


void lf_algo_stock::apply_trade_trans(const lquant_order_t& report) {
	trdConfig->currentVolumeTraded += report.qty;
	stockInfo.volTraded += report.qty;
	if (report.side == 'B') {
		stockInfo.volTraded_B += report.qty;
	} else if(report.side == 'S') {
		stockInfo.volTraded_S += report.qty;
	}
	stockInfo.amtTraded += report.qty * report.prc;

	LOG_INFO("stock transaction:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{},reportTime:{},tickTime:{},timeRate:{},stkProg:{},bskProg:{}",
		basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq, report.tm, ms_to_hhmmssxxx(stockInfo.latestTime), stockInfo.latestTimeRate,
		1.0 * stockInfo.volTraded / stkTotalVol, 1.0 * trdConfig->currentVolumeTraded / trdConfig->totalVolumeToTrade);

	stockInfo.odFilledQty += report.qty;
	if (order_.qty - stockInfo.odFilledQty <= 0) {
		LOG_INFO("order filled:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{},reportTime:{},tickTime:{}", basketId, stkSymbol, order_.side, order_.qty, order_.prc, order_.oseq, report.tm, ms_to_hhmmssxxx(stockInfo.latestTime));
		clean_order();
	}
}


void lf_algo_stock::recv_report_state_new(const lquant_order_t& report) {
	switch (report.type) {
		case 'A':
			LOG_INFO("ERROR state_new A:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{}", basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq);
			break;
		case 'C':
			LOG_INFO("ERROR state_new C");
			apply_cancel(report);
			break;
		case 'J':
			LOG_INFO("ERROR state_new J");
			apply_reject(report);
			break;
		case 'E':
			apply_trade_trans(report);
			break;
		case 'U':
			LOG_INFO("ERROR state_new U:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{}", basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq);
			break;
		default:
			LOG_INFO("ERROR state_new unexpected:basketId:{},symbol:{},type:{},tseq:{}", basketId, stkSymbol, report.type, report.tseq);
	}
}


void lf_algo_stock::recv_report_state_pending_cancel(const lquant_order_t& report) {
	switch (report.type) {
		case 'A':
			LOG_INFO("ERROR pending_cancel A:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{}", basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq);
			break;
		case 'C':
			apply_cancel(report);
			break;
		case 'J':
			stockInfo.stkState = ORD_NEW;
			LOG_INFO("order cancel rejected:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{},reportTime:{},tickTime:{}", 
				basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq, report.tm, ms_to_hhmmssxxx(stockInfo.latestTime));
			break;
		case 'E':
			apply_trade_trans(report);
			break;
		case 'U':
			LOG_INFO("ERROR pending_cancel U:basketId:{},symbol:{},side:{},qty:{},prc:{},oseq:{}", basketId, stkSymbol, report.side, report.qty, report.prc, report.oseq);
			break;
		default:
			LOG_INFO("ERROR pending_cancel unexpected:basketId:{},symbol:{},type:{},tseq:{}", basketId, stkSymbol, report.type, report.tseq);
	}
}
