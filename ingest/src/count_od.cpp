// 只统计一天 Pool 里的 OD 条数（不解码记录，仅读计数），用于入库对账
#include "lquant_tick_pool.hpp"
#include "lquant_log.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
std::shared_ptr<lquantlog::logger> LQUANTSELFLog::logger;
int main(int argc,char**argv){
  lquant_tick_pool p;
  LQUANTSELFLog::logger = lquantlog::stdout_color_mt("console");
  LQUANTSELFLog::logger->set_level(lquantlog::level::warn);
  for(int a=1;a<argc;a++){
    if(!p.load(argv[a])){printf("%s LOAD_FAIL\n",argv[a]);continue;}
    long s=0; for(int c=0;c<5213;c++) s+=p.get_od_count(c);
    printf("%s %ld\n",argv[a],s); fflush(stdout);
  }
  return 0;
}
