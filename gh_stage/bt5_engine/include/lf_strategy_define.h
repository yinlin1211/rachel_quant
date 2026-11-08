#pragma once
#include <functional>
#include <atomic>

namespace lfquant 
{

using std::atomic_int;
using std::atomic_llong;
struct lf_net_signal
{
    char topic[4];
    char symbol[10];
    int tme;            //HHMMSSmmmm
    int price;
    float score;
};

struct lf_hangqing_msg
{
	char topic[4];
	char symbol[10];
    unsigned int nPreClose;				//前收盘价
    unsigned int nMatch;				//最新价
    unsigned int nAskPrice[5];			//申卖价
    unsigned int nAskVol[5];			//申卖量
    unsigned int nBidPrice[5];			//申买价
    unsigned int nBidVol[5];			//申买量
    int nTime;						    //最新价时间
    int64_t iTurnover;				    //成交总金额
    int64_t iVolume;					//成交总量    
};

class lf_hegui;
struct lf_hangqing
{
    atomic_int preClose;			//前收盘价
    atomic_int lastPrice;			//最新价
    atomic_int askPrice[5];			//申卖价
    atomic_int askVolume[5];		//申卖量
    atomic_int bidPrice[5];			//申买价
    atomic_int bidVolume[5];		//申买量
    atomic_int latestTime;			//最新价时间
    atomic_llong iTurnover;		//最新价时间
    atomic_llong iVolume;			//最新价时间

    lf_hegui *hg;                   //合规控制
};

struct lf_trade_t
{
    int     tme;
    int     qty;
    float   prc;
    float   amount;
    char    side;
    bool    is_open;
};

struct lf_order_t
{
    int     tme;
    char    side;
    int     qty;
    float   prc;
    int     state;
    int     filled_qty;
    float   filled_prc;
};

typedef std::function<void(const lf_net_signal&)> lf_net_signal_fun;

}
