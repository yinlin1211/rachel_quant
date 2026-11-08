#pragma once

#include <cstddef>

#define lquant_TICK_SYMBOL_LEN         16
#define lquant_TICK_EXCH_LEN            8
#define lquant_ORD_ALGO_LEN            64
#define lquant_ORD_ORDID_LEN           64
//#define MAX_LSIZE                   256
#define MAX_LSIZE                   8
#define lquant_ACCOUNTID_LEN           16


#define lquant_BASKETID_LEN           64
#define lquant_PARAMS_LEN           64


enum market_state_t {
    MKT_PREOPEN,
    MKT_OPEN,
    MKT_CLOSED,
    MKT_HALTED,
    MKT_VOLATILE,
    MKT_STATE_MAX
};

enum qes_order_type_t {
    lquant_ORDER_TYPE_NA			    = '?',
    lquant_ORDER_TYPE_ORDER_ENTER	    = 'O',	// request
    lquant_ORDER_TYPE_ORDER_ACK		= 'A',  // 报单已收到回报， 确认交易所收到
    lquant_ORDER_TYPE_ENTER_REJ		= 'J',	// 收到响应或者错误， 拒绝
    lquant_ORDER_TYPE_EXECUTION		= 'E',  // 报单已执行， 成交
    lquant_ORDER_TYPE_CANCEL_REQUEST   = 'X',	// 撤单request
    lquant_ORDER_TYPE_CANCEL_ACK		= 'C',  // 撤单确认
    lquant_ORDER_TYPE_EXCHANGE_TRADE_CANCEL_CORRECTION='U',	//(tag 20=1 or 2)
    lquant_ORDER_TYPE_LOGIN			= 'L',	// request
    lquant_ORDER_TYPE_REQUEST_HIST	    = 'H',	// 请求历史记录

    lquant_ORDER_TYPE_MAX		        = 'Z' + 1
};

enum order_direction_t
{
    lquant_ORDER_DIRECTION_LONG = 'B',
    lquant_ORDER_DIRECTION_SHORT = 'S'
};
enum order_flag_t
{
    lquant_ORDER_FLAG_OPEN = 'O',
    lquant_ORDER_FLAG_CLOSE = 'C',
    lquant_ORDER_FLAG_CLOSE_TODAY = 'D',
    lquant_ORDER_FLAG_CLOSE_YESTODAY = 'T'
};
enum order_state_t {
    ORD_NONE = 0, 
    ORD_PENDING_NEW = 1, 
    ORD_NEW = 2, 
    ORD_PENDING_CANCEL = 3, 
    ORD_REJECTED = 4,
    ORD_FILLED = 5,
    ORD_CANCELED = 6,
    ORD_STATE_MAX = 100,
};


#pragma pack(push, 1)

struct lquant_order_t {
    char symbol[lquant_TICK_SYMBOL_LEN]; //合约代码
    char algo[lquant_ORD_ALGO_LEN];	  // 算法名称
    char exch[lquant_TICK_EXCH_LEN];	 // 交易所
    char orderid[lquant_ORD_ORDID_LEN];   //父订单在外部系统的编号

	char baskid[lquant_BASKETID_LEN];   //委托订单的篮子编号
	char accid[lquant_ACCOUNTID_LEN];   //委托订单的账户
	char params[lquant_PARAMS_LEN];   //其他扩展信息

    unsigned int idnum;	 // 合约当天唯一编号
    unsigned int pid;    // 父订单编号 traderid
    unsigned int oseq;   //login_teid 登录时为:teclient::teid  报单时为报单编号 orderSequence
    unsigned int tseq;   // 每个teid（配置文件里的Teid）的回报数量 traderSequence
    unsigned int tm;     // 委托时间 （HHMMSSmmm)

    int qty;	   // 总量
    float prc;		// 价
    char type;    // open/close/resend hist
    char side;   // 'B'=buy, 'S'=sell long, 'T'=sell short
    char ocflag; // 'O'=open, 'C'=close, 'D'=平今 'Y'=平昨, '\0'=default
    char state;  //

    float ask;
    int   asksiz;
    float bid;
    int   bidsiz;
};


struct lquant_position_t {
    char           symbol[lquant_TICK_SYMBOL_LEN]; // ticker symbol
    char           broker[lquant_TICK_EXCH_LEN];   // execution broker
    char           locate_broker[lquant_TICK_EXCH_LEN]; 
    char           algo[lquant_ORD_ALGO_LEN];      // description and parameters of broker execution algorithms
    char           exch[lquant_TICK_EXCH_LEN];     // destination exchange
    char           orderid[lquant_ORD_ORDID_LEN];  //父订单在外部系统的编号

    unsigned int   idnum;                        // qes stock id
    unsigned int   sid;                         // id uniquely identify trading strategy
    unsigned int   seq;                         // day unique reference number  

    int            dqty;                        // desired shares, long > 0, short < 0
    float          dprc;                        // desired price

    int            bqty;                        // bought shares / long 
    int            sqty;                        // sold shares  / short 
    float          cost;                        // buy costs 买付出
    float          prds;                        // sell proceeds 卖收益
    char           cond;                        // 'T'=short sell, 'O'=open for future, 'C'=close for future, 'D'=close for future

};

struct lquant_ladder_t {
    int          lid;			       // ladder id 0=demand, 1=supply
    int          n;                    // ladder size
    lquant_order_t  order[MAX_LSIZE];
    unsigned int orderid[MAX_LSIZE];
};

struct lquant_trader_t {
    lquant_position_t position;

    int basketid;       // same basketid collects
    int traderid;		// index of persistence store, unique identify this trader, 每个持仓配置或者每个报单命令产生的一个traderID， 自增
    unsigned tseq;		// highest tseq seen by this trader

    lquant_ladder_t dmd;
    lquant_ladder_t spl;

    float oslim;			// order size limit
    float tsize;			// tick size
    market_state_t mstate;
};

// 持仓数据
struct lquant_holdings_t {
    char symbol[lquant_TICK_SYMBOL_LEN]; // ticker symbol
    unsigned int cidx;                // qes stock id
    char exch[lquant_TICK_EXCH_LEN];    // destination exchange

    unsigned int qty_long; // bought shares / long
    unsigned int qty_short; // sold shares  / short

    unsigned int qty_long_yd;   // bought shares / long
    unsigned int qty_short_yd; // sold shares  / short

    float avgprc_long;          // avg price 
    float avgprc_short;
};


// 资金数据
struct lquant_account_t {
    char accountid[lquant_ACCOUNTID_LEN];

    double capital;     // 权益
    double available;  // 可用资金
    double profit;      // 收益
};

struct lquant_signal_info
{
    char              symbol[lquant_TICK_SYMBOL_LEN];
    unsigned int      volume;
    order_direction_t direction;
    int               pid;
    double            prc;
};

#pragma pack(pop)



