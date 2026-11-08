// 多线程导出 OD，RowBinary 直出到 stdout（管道给 clickhouse-client）
#include "lquant_tick_pool.hpp"
#include "lquant_log.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <vector>
#include <omp.h>
std::shared_ptr<lquantlog::logger> LQUANTSELFLog::logger;
struct __attribute__((packed)) Row {
  uint32_t date; uint16_t cidx; uint32_t time; uint32_t idx;
  char chType; char chBS; uint32_t price; uint32_t volume;
};
static_assert(sizeof(Row)==24, "row must be 24 bytes");
int main(int argc,char**argv){
  if(argc<3){fprintf(stderr,"usage: dump_mt <file> <date> [threads]\n");return 2;}
  int threads = argc>3?atoi(argv[3]):8;
  uint32_t date = (uint32_t)atoi(argv[2]);
  lquant_tick_pool p;
  LQUANTSELFLog::logger = lquantlog::stdout_color_mt("console");
  LQUANTSELFLog::logger->set_level(lquantlog::level::warn);
  if(!p.load(argv[1])){fprintf(stderr,"load failed\n");return 1;}
  const int N = 5213;
#pragma omp parallel num_threads(threads)
  {
    std::vector<char> buf; buf.reserve(1<<20);
#pragma omp for schedule(dynamic, 8)
    for(int c=0;c<N;c++){
      int no=p.get_od_count(c);
      for(int i=0;i<no;i++){
        lquant_tdf_order_t*o=p.get_od(c,i);
        Row r; r.date=date; r.cidx=(uint16_t)c; r.time=(uint32_t)o->nTime; r.idx=(uint32_t)o->nIndex;
        r.chType=(char)(o->chType?o->chType:'0'); r.chBS=(char)(o->chBSFlag?o->chBSFlag:'X');
        r.price=(uint32_t)o->nPrice; r.volume=(uint32_t)o->nVolume;
        const char* pp=(const char*)&r;
        buf.insert(buf.end(), pp, pp+sizeof(Row));
        if(buf.size()>=(1<<20)-64){
#pragma omp critical
          { fwrite(buf.data(),1,buf.size(),stdout); }
          buf.clear();
        }
      }
    }
    if(!buf.empty()){
#pragma omp critical
      { fwrite(buf.data(),1,buf.size(),stdout); }
    }
  }
  fflush(stdout);
  return 0;
}
