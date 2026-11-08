#pragma once

#include <string>
#include <vector>
// #include <mutex>
#include "lf_redis.h"

struct redisClusterContext;
struct redisReply;

namespace lfquant
{

struct lf_instruct;
struct lf_order_update;
struct lf_trade;

class lf_cluster_redis : public lf_redis
{
public:
    lf_cluster_redis(const char *hostname, const char *pwd);
    virtual ~lf_cluster_redis();

    virtual void connect();
    virtual void ping();
    virtual std::string get(const char * key);
    virtual std::string get(const std::string &key) { return get(key.c_str()); }
    virtual int set(const char *key, const char *data);
    virtual int pushData(const char *key, const char* data, bool isLog=true);
    virtual int pushData(const std::string &key, const std::string &data) { return pushData(key.c_str(), data.c_str()); }
    virtual std::string popData(const char * key);
    virtual std::string bpopData(const char *key, int waitTime);
    virtual int pushData(std::vector<std::string> &keys, const char* data);
    virtual int pushData(const char *key, const std::vector<const char*> &data, bool isLog=true);
    virtual std::vector<std::string> lrange(const char * key, int start, int end);
    virtual bool ltrim(const char * key, int start, int end);
    virtual int llen(const char * key);
    virtual int delKey(const char* key);

    virtual int pushData(const char *key, const lf_instruct *data, bool isLog = true);
    virtual int pushData(const char *key, const std::vector<lf_instruct*> &data, bool isLog = true);
    virtual int pushData(const char *key, const lf_order_update *data, bool isLog = true);
    virtual int pushData(std::vector<std::string> &keys, const lf_order_update *data);
    virtual int pushData(const char *key, const lf_trade *data, bool isLog = true);

private:
    redisClusterContext *cc_ = nullptr;
};


}
