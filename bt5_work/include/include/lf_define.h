#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace lfquant
{
// tradeside (交易方向)
// 数值    含义
// 1    买入
// 2    卖出
struct lf_instruct
{
    char inst_type;     // O / C
    char client_id[32]; // 用户自定义ID
    char acct_type[8];  // 账户类型 0 集中交易  F0 深证快速交易  SHF0 上海快速交易
    char acct_id[16];   // 账户ID
    char order_no[64];  // 撤单需要
    char symbol[32];    // 
    char tradeside[8];  // 1 买入  2 卖出  
    int ord_qty;        //委托数量
    float ord_price;    //委托价格
    char ord_type[8];   // 委托类型 0 限价  U 市价 
    char ord_param[32];     
    char basket_id[64];     //篮子id
    char trade_channel[64]; //交易通道，算法指令不用
    char teid[16];      //算法teid
};

// order_status (订单状态)
// 数值    含义
// 0    已报
// 1    部分成交
// 2    全部成交
// 3    部分撤单
// 4    全部撤单
// 5    交易所拒单
// 6    柜台未接受
struct lf_order_update
{
    char client_id[32];
    char ord_no[64];
    char ord_status[4];
    char acct_type[8];
    char acct_id[16];
    char cats_acct[16];
    char symbol[16];
    char tradeside[8];
    char ord_qty[9];
    char ord_px[8];
    char ord_type[8];
    char ord_param[32];
    char corr_type[32];  // 下单来源
    char corr_id[32];
    char filled_qty[9];
    char avg_px[8];     //成交均价
    char cxl_qty[9];   //撤单数量
    char ord_time[24]; // yyyy-MM-dd HH:mm:ss
    char err_msg[96];
    char write_time[32]; // 
    char basket_id[64];     //篮子id
};

struct lf_trade {
	char acct_type[8];	//账户类型	参见数据字典章节
	char acct_id[16];	//账户
	char symbol[16];	//标的代码
	char tradeside[8];	//交易方向	参见数据字典章节
	char ord_no[64];	//订单编码
	char trd_no[64];	//成交编码
	char filled_qty[16];	//成交数量
	char filled_prc[16];	//成交均价
	char trd_type[8];	//交易类型
    char trd_time[32];	//成交时间格式yyyy-MM-dd HH:mm:ss.fff
    char client_id[32];
    char basket_id[64]; //篮子id
    char param[32];     
};

struct lf_stock
{
    std::string symbol;     //股票ID，600888.SH
    int volume;             //交易数量，1000买入1000股，-1000卖出1000股
    std::string info;       //附加信息，委托时原样传递即可，现有股卡信息
    std::string infoEx;       //附加信息，api用来传递instanceId
};

struct lf_basket
{
    std::string basketId;       //篮子ID
    std::string strategyId;     //算法策略ID
    std::string strategyTeId;   //算法策略TeID
    std::string accountId;      //交易账号ID
    int startTime;              //篮子开始时间,hhmmss
    int endTime;                //篮子结束时间,hhmmss
    double tradeExposure;       //交易最大敞口
    double cancelRatio;         //最大撤单比率
    bool isTradeLimitPrice;     //涨跌停板，是否允许委托，true涨跌停不交易
    std::vector<lf_stock> stocks;     //交易篮子
    std::unordered_map<std::string, int> stockQtys;     //交易篮子股票总数量
    std::string batchNumber;   //客户自定义批号
    
    std::string catsapi_strid;          //中信api策略id
    // std::string catsapi_instanceId;     //中信api实例Id
    std::string catsapi_basketId;       //中信api篮子Id
    std::string params;        //参数列表map，key1:value1,key2:value2

    int get_symbol_num(const char *symbol)
    {
        auto it = stockQtys.find(symbol);
        if(it == stockQtys.end())
            return 0;
        return it->second;
    }
};

struct lf_basket_operation
{
    std::string basketId;        // 篮子ID
    std::string operationType;   // 操作类型 撤销篮子(cancel) 暂停篮子(pause) 恢复篮子(recover)
};

inline int format_instruct(char *buf, int sz, const lf_instruct &d) 
{
    // char buf[512] = {0};
    return snprintf(buf, sz, "instructs:{inst_type:%c,client_id:%s,acct_type:%s,acct_id:%s,order_no:%s,"
                "symbol:%s,tradeside:%s,ord_qty:%d,ord_price:%.2f,ord_type:%s,ord_param:%s,"
                "basket_id:%s,trade_channel:%s,teid:%s,}", 
                d.inst_type, d.client_id, d.acct_type, d.acct_id, d.order_no, d.symbol, d.tradeside, 
                d.ord_qty, d.ord_price, d.ord_type, d.ord_param, d.basket_id, d.trade_channel, d.teid);
}
    
inline int format_order(char *buf, int sz, const lf_order_update &d) 
{
    return snprintf(buf, sz, "order:{client_id:%s,ord_no:%s,ord_status:%s,ord_type:%s,acct_type:%s,acct_id:%s,"
        "symbol:%s,tradeside:%s,ord_qty:%s,ord_px:%s,ord_param:%s,filled_qty:%s,avg_px:%s,cxl_qty:%s,"
        "ord_time:%s,err_msg:%s,basket_id:%s,}",
        d.client_id, d.ord_no, d.ord_status, d.ord_type, d.acct_type, d.acct_id, d.symbol, d.tradeside, d.ord_qty, d.ord_px,
        d.ord_param, d.filled_qty, d.avg_px, d.cxl_qty, d.ord_time, d.err_msg, d.basket_id);
}
    
inline int format_trade(char *buf, int sz, const lf_trade &d) 
{
    return snprintf(buf, sz, "trade:{client_id:%s,ord_no:%s,trd_no:%s,trd_type:%s,acct_type:%s,acct_id:%s,"
        "symbol:%s,tradeside:%s,filled_qty:%s,filled_prc:%s,trd_time:%s,basket_id:%s,param:%s,}",
        d.client_id, d.ord_no, d.trd_no, d.trd_type, d.acct_type, d.acct_id, d.symbol, d.tradeside, 
        d.filled_qty, d.filled_prc, d.trd_time, d.basket_id, d.param);
}

typedef lf_stock quant_basket_stock;
typedef lf_basket quant_basket;

typedef std::function<void(const lf_instruct&)> lf_instruct_fun;
typedef std::function<void(const lf_order_update&)> lf_order_update_fun;
typedef std::function<void(const lf_trade&)> lf_trade_fun;

}
