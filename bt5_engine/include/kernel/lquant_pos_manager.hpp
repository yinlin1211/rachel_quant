#include <map>
#include <vector>
#include "lquant_data_api.hpp"
#include "lquant_base_univ.hpp"


using namespace std;

typedef struct hldStruct {
  long endTime;
  float marginrate;
  float multiplier;
  float cost_rate;
  float settle;
  long  lqty;
  double lcost;
  double lavgprc;
  double lmargin;
  long   sqty;
  double scost;
  double savgprc;
  double smargin;

  string __str__() {
    string res = "";
    char tmp[1000];
    sprintf(tmp, "[%ld, %.5f, %.5f, %.5f, %.5f, %ld, %.5lf, %.5lf, %.5lf, %ld, %.5lf, %.5lf, %.5lf]",
          endTime, marginrate, multiplier, cost_rate, settle, lqty, lcost, lavgprc,
    lmargin, sqty, scost, savgprc, smargin);
    res = tmp;

    return res;
  }
} HoldStruct;

typedef struct trdStruct {
  long endTime;
  string tkr;
  char bsFlg;
  char ocFlg;
  long dqty;
  float prc;
  double val;
  double dval;
  double cost;
  double nav;

  string __str__() {
    string res = "";
    char tmp[1000];
    sprintf(tmp, "[%ld, %s, %c, %c, %ld, %.5f, %.5lf, %.5lf, %.5lf, %.5lf]",
            endTime, tkr.c_str(), bsFlg, ocFlg, dqty, prc, val, dval, cost, nav);
    res = tmp;

    return res;
  }
} TradeStruct;

class lquant_pos_manager {
public:
  lquant_pos_manager(float cash, float _slippage, string dirName, bool fixedNav=true);

  double getNav0();
  double getNav();
  double getAvailable();
  bool buy(long endTime, string tkr, long qty, float prc);
  bool sell(long endTime, string tkr, long qty, float prc);
  string __str__();
  bool dump();

private:
  float slippage, __cash, __nav0, __margin;
  bool fixedNav;
  map<string, HoldStruct> __hldMap;
  vector<TradeStruct> __tradeVec;
  string outDir;
  lquant_data_api *_dpi;
  lquant_base_univ *_univ;
  vector<string> _tkrs;
  vector<string> tkrsAll;
  map<string, int> tkrsAllMap;
  double val, cost, dval;
  vector<long> minTime;
  map<long, int> minTimeMap;
  vector<long> dlyTime;
  map<long, int> dlyTimeMap;
  lquant_data<float> clseData;
  lquant_data<float> clseDlyData;
  int rowAll, colAll;
  lquant_data<float> lgRtData;
  lquant_data<float> lgRtDlyData;
};
