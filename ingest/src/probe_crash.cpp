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
  if(!p.load(argv[1])){printf("load failed\n");return 1;}
  unsigned long long acc=0;
  for(int c=0;c<5213;c++){
    int no=p.get_od_count(c);
    fprintf(stderr,"c=%d no=%d\n",c,no);
    for(int i=0;i<no;i++){
      lquant_tdf_order_t*o=p.get_od(c,i);
      acc += (unsigned)o->nTime + (unsigned)o->nPrice + (unsigned)o->nVolume + (unsigned)o->nIndex;
    }
  }
  printf("done acc=%llu\n",acc);
  return 0;
}
