#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cstring>

//证券类型定义
#define ID_BT_INDEX                 0x01   //交易所指数
#define ID_BT_SHARES_A              0x10   //A股
#define ID_BT_ZXB                   0x11   //中小板
#define ID_BT_CYB                   0x12   //创业板
#define ID_BT_FUND_ETF              0x23   //ETF
#define ID_BT_BOND_CORP             0x31   //企业债券
#define ID_BT_FUTURES_IDX           0x70   //指数期货

//数据类型定义
#define ID_TDFTELE_LOGIN			1		//登录(Login)
#define ID_TDFTELE_LOGINANSWER		2		//登录应答
#define ID_TDFTELE_LOGOUT			3		//登出(Logout)
#define ID_TDFTELE_CLOSE			4		//服务器关闭(Server Close)
#define ID_TDFTELE_COCDETABLE		6		//证劵索引表(Security Directory)
#define ID_TDFTELE_REQDATA			7		//请求行情数据
#define ID_TDFTELE_MARKETCLOSE		8		//闭市信息	
#define ID_TDFTELE_TRADINGMESSAGE	9		//交易信息
#define ID_TDFTELE_TRANSACTION		1101	//成交(Transaction)
#define ID_TDFTELE_ORDERQUEUE		1102	//委托队列(Queue)
#define ID_TDFTELE_ORDER			1103	//逐笔委托(Order)
#define ID_TDFTELE_ORDERQUEUE_FAST	1104	//委托队列(FAST Queue)
#define ID_TDFTELE_TRANSACTIONEX    1105    //逐笔成交扩展 
#define ID_TDFTELE_MARKETDATA		1012	//行情数据(Market Data)
#define ID_TDFTELE_MARKETDATA_FUTURES	1016	//期货行情数据(Futures Market Data)	1016
#define ID_TDFTELE_INDEXDATA		1113	//指数(Index)
#define ID_TDFTELE_MARKETOVERVIEW	1115	//市场概况(Market Overview)
#define ID_TDFTELE_ETFLISTFILE		1116	//ETF清单文件.


#define ID_TDFTELE_ADDCODE			2001	//添加代码.
#define ID_TDFTELE_SUBSCRIPTION		2002	//订阅行情.
#define ID_TDFTELE_PLAYSPEED		2003	//数据回放速度设置.
#define ID_TDFTELE_REQETFLIST		2004	//请求ETF清单.

#pragma pack(push,1)

#ifdef __linux__
typedef long long __int64;
#endif

struct TDFDefine_MsgHead
{
    unsigned short  	sFlags;		//16字节 标识符,升级到 TDF_VERSION_NX_START_6001 .
    unsigned short  	sDataType;	//16字节 数据类型	          
    int					nDataLen;	//32字节 数据长度
    int					nTime;		//32字节 时间戳（精确到毫秒HHMMSSmmmm）
    int     			nOrder;		//32字节 流水号
};

struct TDFDefine_Login
{
    char chUserName[16];
    char chPassword[32];
    char chID[8];
    char chMD5[32];
};

struct TDFDefine_LoginAnswer
{
    char chInfo[64];			//信息
    int  nAnswer;				//1:登陆成功
    int  nMarkets;
    char chMarketFlag[32][4];	//市场标志
    int  nDynDate[32];			//动态数据日期
};

struct TDFDefine_RequestCodeTable
{
    char chMarketFlag[4];		//市场标志(SZ;SH;HK;CF)
    int  nDate;					//数据日期，-1:表示动态数据，
};

#define ID_HDFDATAFLAGS_RETRANSALTE			0x00000001	//数据从开始传送
#define ID_HDFDATAFLAGS_NOTRANSACTION		0x00000100	//不传送逐笔成交数据
#define ID_HDFDATAFLAGS_NOABQUEUE			0x00000200	//不传送委托队列数据
#define ID_HDFDATAFLAGS_NOINDEX				0x00000400	//不传送指数数据
#define ID_HDFDATAFLAGS_NOMARKETOVERVIEW	0x00000800	//不传送 Market OverView 数据
#define ID_HDFDATAFLAGS_NOORDER				0x00001000	//不传送逐笔委托数据（SZ-Level2）
#define ID_HDFDATAFLAGS_COMPRESSED			0x00010000	//启用数据压缩
#define ID_HDFDATAFLAGS_ABQUEUE_FAST		0x00020000	//以FAST方式提供委托队列数据
#define ID_HDFDATAFLAGS_TRANSACTIONEX       0x00080000  //逐笔成交按扩展方式传送（含委托信息）
#define ID_HDFDATAFLAGS_DBF1                0x08000000  //交易所DBF传送专用协议


struct TDFDefine_RequestMarketData{
    char chMarketFlag[4];		
    int  nFlags;              //
};


struct TDFDefine_SecurityCodeHead{
    int  nSource;	//交易所 0:深圳 2:上海
    int  nDate;		//数据日期
    int  nCount;	//数据总数(-1:未授权)
    int  nFlags;	//(保留)
};

struct TDFDefine_SecurityCode{
    int  gid;	
    int  nType;	
    char chSecurityCode[8];
    char chSymbol[16];		
};

//1.1.2 逐笔成交(Transaction)
struct TDFDefine_TransactionHead
{
    int 	gid;			//本日编号
    int     nItems;			//数据个数
};

struct TDFDefine_Transaction{
    int 	nTime;		//成交时间(HHMMSSmmmm)
    int 	nIndex;		//成交编号
    int		nPrice;		//成交价格
    int 	nVolume;	//成交数量
    int		nTurnover;	//成交金额
};

//逐笔成交扩展(TransactionEx)
struct TDFDefine_TransactionExHead
{
    int gid;    //本日编号
    int nItems; //数据个数
};
struct TDFDefine_TransactionEx
{
    int nTime;           //成交时间(HHMMSSmmm)
    int nIndex;          //成交编号
    int nPrice;          //成交价格
    int nVolume;         //成交数量
    int nTurnover;       //成交金额
    char chBSFlag;       //买卖方向
    char chOrderKind;    //成交类型
    char chFunctionCode; //成交代码
    char chResv;         //保留
    int nAskOrder;       //叫卖方委托序号
    int nBidOrder;       //叫买方委托序号
};

//1.1.2 逐笔委托(Order)
struct TDFDefine_OrderHead
{
    int 	gid;			//本日编号
    int     nItems;			//数据个数
};

struct TDFDefine_Order
{
    int 	nTime;		//委托时间(HHMMSSmmmm)
    int 	nIndex;		//委托编号
    int		nPrice;		//委托价格
    int 	nVolume;	//委托数量
    char    chType;		//委托类别
    char    chBSFlag;	//委托买卖类型('B','S','C')
    char    chResv[2];	//保留	
};

//1.1.3 订单队列(Queue)
struct TDFDefine_OrderQueueHead
{
    int     nItems;			//数据个数
};

struct TDFDefine_OrderQueue{
    int 	gid;			//本日编号
    int 	nTime;			//订单编号(HHMMSSmmmm)
    int     nSide;			//买卖方向('B':Bid 'S':Ask)
    int		nPrice;			//成交价格
    int 	nOrders;		//订单数量
    int 	nABItems;		//明细个数
    int 	nABVolume[200];	//订单明细
};

//1.1.3.1 订单队列(上海FAST Queue)
struct TDFDefine_OrderQueue_FAST
{
    int  gid;			//本日编号
    int  nTime;				//订单时间（HHMMSS）
    int  nSide;				//买卖方向（'B':Bid 'A':Ask）
     int  nImageStatus;		//操作状态（1-Full Image 2-Update）
    int  nNoPriceLevel;		//操作的价位数
}; 

struct TDFDefine_OrderQueue_FAST_Operate
{
    int		nPriceOperate;	// 1.add , 2 Update ,3 Delete  0. absent 
    int     nPrice;			//价格
    int		nNumOrders;		//委托笔数
    int		nOqLevel;		//当前价位上要操作的订单数
};

struct TDFDefine_OrderQueue_FAST_OperateItem
{
    int nOperate;	//操作方式: 1 Add ,2 Update,3 Delete
    int nEntryID;   //操作ID:  Oreder Queue Operator Entry ID
    int nQty;		//订单数量
};

//1.1.4 行情数据(Market Data)
struct TDFDefine_MarketDataHead
{
    int     nItems;			//数据个数
};

typedef TDFDefine_MarketDataHead TDFDefine_MarketDataFuturesHead;

struct TDFDefine_MarketData
{
    int		gid;						//本日编号
    int		nTime;						//时间(HHMMSSmmmm)
    int		nStatus;					//状态
    unsigned int nPreClose;				//前收盘价
    unsigned int nOpen;					//开盘价
    unsigned int nHigh;					//最高价
    unsigned int nLow;					//最低价
    unsigned int nMatch;				//最新价
    unsigned int nAskPrice[10];			//申卖价
    unsigned int nAskVol[10];			//申卖量
    unsigned int nBidPrice[10];			//申买价
    unsigned int nBidVol[10];			//申买量
    unsigned int nNumTrades;			//成交笔数
    __int64	iVolume;					//成交总量
    __int64	iTurnover;					//成交总金额
    __int64	nTotalBidVol;				//委托买入总量
    __int64	nTotalAskVol;				//委托卖出总量
    unsigned int nWeightedAvgBidPrice;	//加权平均委买价格
    unsigned int nWeightedAvgAskPrice;  //加权平均委卖价格
    int		nIOPV;						//IOPV净值估值
    int		nYieldToMaturity;			//到期收益率
    unsigned int nHighLimited;			//涨停价
    unsigned int nLowLimited;			//跌停价
    char	chPrefix[4];				//证券信息前缀
};

//1.1.5 指数(Index)
struct TDFDefine_IndexDataHead
{
    int     nItems;				//数据个数
};

struct TDFDefine_IndexData
{
    int		gid;			//本日编号
    int     nTime;			//时间(HHMMSSmmmm)
    int		nOpenIndex;		//今开盘指数
    int 	nHighIndex;		//最高指数
    int 	nLowIndex;		//最低指数
    int 	nLastIndex;		//最新指数
    __int64	iTotalVolume;	//参与计算相应指数的交易数量
    __int64	iTurnover;		//参与计算相应指数的成交金额
    int		nPreCloseIndex;	//前盘指数
};


//1.1.6 市场概况(Market Overview)
struct TDFDefine_MarketOverview
{
    int		nSource;
    int		nOrigTime;			//时间(HHMMSSmmmm)
    int		nOrigDate;			//日期
    char	chEndOfDayMarket;	//结束标志
};


//f)	闭市(Close)
struct TDFDefine_MarketClose
{
    int		nSource;
    int		nTime;				//时间(HHMMSSmmmm)
    char	chInfo[64];			//闭市信息
};

//g)	交易信息通知 (Trading Message)
struct TDFDefine_TradingMessage
{
    int		gid;				//本日编号
    char	chSecurityId[8];	//证券代码
    char	chInfo[128];		//信息
    int		nTime;				//时间(HHMMSSmmmm)
};


//期货Markets Data.
struct TDFDefine_MarketData_Futures
{
    int gid;							//本日编号
    int	nTime;							//时间(HHMMSSmmmm)	
    int		nStatus;					//状态
    __int64 iPreOpenInterest;			//昨持仓
    unsigned int nPreClose;				//昨收盘价
    unsigned int nPreSettlePrice;		//昨结算
    unsigned int nOpen;					//开盘价	
    unsigned int nHigh;					//最高价
    unsigned int nLow;					//最低价
    unsigned int nMatch;				//最新价
    __int64	iVolume;					//成交总量
    __int64	iTurnover;					//成交总金额
    __int64 iOpenInterest;				//持仓总量
    unsigned int nClose;				//今收盘
    unsigned int nSettlePrice;			//今结算
    unsigned int nHighLimited;			//涨停价
    unsigned int nLowLimited;			//跌停价
    int		nPreDelta;			        //昨虚实度
    int     nCurrDelta;                 //今虚实度
    unsigned int nAskPrice[5];			//申卖价
    unsigned int nAskVol[5];			//申卖量
    unsigned int nBidPrice[5];			//申买价
    unsigned int nBidVol[5];			//申买量
};

#pragma pack(pop)

/////////////////////////////////////////////////////////////////////////////////////////////////
using namespace std;

typedef TDFDefine_MsgHead           lquant_msg_head_t;
typedef TDFDefine_Login             lquant_login_t;
typedef TDFDefine_LoginAnswer       lquant_login_answer_t;
typedef TDFDefine_RequestCodeTable  lquant_ctrequ_t;
typedef TDFDefine_RequestMarketData lquant_mdrequ_t;
typedef TDFDefine_SecurityCodeHead  lquant_sc_head_t;
typedef TDFDefine_SecurityCode      lquant_sc_t;
//typedef TDFDefine_TransactionHead   lquant_transaction_head_t;
//typedef TDFDefine_Transaction       lquant_transaction_t;
typedef TDFDefine_TransactionExHead   lquant_transaction_head_t;
typedef TDFDefine_TransactionEx       lquant_transaction_t;
typedef TDFDefine_MarketData        lquant_market_data_t;
typedef TDFDefine_MarketData_Futures lquant_market_data_futures_t;
typedef TDFDefine_OrderQueue        lquant_order_queue_t;
typedef TDFDefine_OrderHead         lquant_tdf_order_head_t;
typedef TDFDefine_Order             lquant_tdf_order_t;
typedef TDFDefine_IndexDataHead     lquant_index_head_t;
typedef TDFDefine_IndexData         lquant_index_t;

//自定义的一个全局tick结构
enum lquant_update_type_t {
    UPD_UNKNOWN = 0,
    UPD_TRANSACTION,
    UPD_MARKETDATA,
    UPD_ORDERQUEUE,
    UPD_MARKETDATA_FUTURES,
    UPD_INDEX
};

//定义数据的来源
enum lquant_quote_src_t {
    DTYPE_UNK,
    DTYPE_SHL1,
    DTYPE_SHL2,
    DTYPE_SZL1,
    DTYPE_SZL2,
    DTYPE_CFL1,
    DTYPE_CFL2,
    DTYPE_CTP,
};

#pragma pack(push, 1)

// struct lquant_tick_t {
//     lquant_update_type_t updType;      //更新类型
//     lquant_quote_src_t   srcType;      //数据来源
//     int               gid;          //datafeed中的编号
//     int               idnum;        //内部维护的编号
//     int               updTime;      //自凌晨开始的毫秒数
//     int	              updOrder;		//更新的序号，依次递增

//     union {
//         lquant_market_data_t         md;
//         lquant_market_data_futures_t mdf;
//         lquant_transaction_t         ts;
//         lquant_order_queue_t         qu;
//     };
// };

#pragma pack(pop)

////////////////////////////////////////////////////////////////////////////////////////////////////
struct SecuCode {
    int   gid;          //证券的当日编号，来自数据源；末尾表示交易所：10，cf 1，sh 2，sz
    int   idnum;        //证券的唯一编号，来自历史数据；0,未知
    char  tkr[16];
    char  tkrName[38];
    int   type;         //证券类型：股票、基金、指数等
};

typedef map<int, int>    GidMap;   //gid to secucode map
typedef map<int, int>    IdnumMap; //idnum to secucode map
typedef map<string, int> TkrMap;   //tkr to secucode map

class lquantSecuTable {
public:
    lquantSecuTable():dte(0) { }

    bool addSecu(int gid, int idnum, const char *tkr, const char *tkrName, int type) {
        if(gmap.find(gid) != gmap.end()) return false;

        scodes.push_back(SecuCode());
        int idx = (int)scodes.size() - 1;
        SecuCode &scode = scodes[scodes.size()-1];
        scode.gid = gid;
        scode.idnum = idnum;
        strncpy(scode.tkr, tkr, 16);
        switch(gid%100) {
        case 10:
          strncpy(scode.tkr + strlen(tkr), ".SP", strlen(tkr));
            break;
        case 1:
            strncpy(scode.tkr + 6, ".SH", 4);
            break;
        case 0:
            strncpy(scode.tkr + 6, ".SZ", 4);
            break;
        default:
            break;
        }
        strncpy(scode.tkrName, tkrName, 38);
        scode.type = type;

        gmap[gid] = idx;
        if(idnum > 0)  nmap[idnum] = idx;
        tmap[scode.tkr] = idx;
        return true;
    }

    const SecuCode *getSCByGid(int gid) {
        GidMap::iterator itr = gmap.find(gid);
        return itr == gmap.end() ? NULL : &scodes[0] + itr->second;
    }

    const SecuCode *getSCByIdnum(int idnum) {
        IdnumMap::iterator itr = nmap.find(idnum);
        return itr == nmap.end() ? NULL : &scodes[0] + itr->second;
    }

    const SecuCode *getSCByTkr(string tkr) {
        TkrMap::iterator itr = tmap.find(tkr);
        return itr == tmap.end() ? NULL : &scodes[0] + itr->second;
    }

    const SecuCode *getSCList(const SecuCode *&scList, int &n) {
        n = scodes.size();
        scList = &scodes[0];
        return scList;
    }

    int getDte() const { return dte; }
    int setDte(int dte) {
        return this->dte = dte;
    }

private:
    vector<SecuCode> scodes;
    GidMap   gmap;
    IdnumMap nmap;
    TkrMap   tmap;
    int      dte;
};
