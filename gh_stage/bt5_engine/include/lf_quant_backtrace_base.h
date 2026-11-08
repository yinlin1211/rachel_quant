#pragma once

#include <string>
#include <functional>
#include <list>

// #include "quant_log.h"
#include "lquantOrder.hpp"

// using namespace lquant;

// namespace lquant {
class lquant_tick_pool;

// };

namespace lfquant 
{

class Statistics;
class lf_basket;

typedef std::function<void(const lquant_order_t&)> at_response_fun;
// typedef std::function<void(const std::string&, int, double)> at_transaction_fun;
// typedef quant_backtrace_base* (*algo_backtrace_create_fun)();

class quant_backtrace_base
{
public:
    quant_backtrace_base()
    {}
    virtual ~quant_backtrace_base()
    {}

    virtual bool init(const lf_basket &basket) = 0;    //对象初始化
    virtual void on_request(const lquant_order_t &order)   //处理指令回报
    {
        // printf("quant_backtrace_base::on_request\n");
        orders_.push_back(new lquant_order_t(order));

        lquant_order_t response(order);
        response.state = ORD_NEW;
        snprintf(response.orderid, sizeof(response.orderid) - 1, "%d", order.oseq);
        response.type = 'A';
        if(on_response_)
            on_response_(response);     
    }
    virtual bool on_request_ex(int id, const lquant_order_t &order, const std::string&)
    {
        on_request(order);
        return true;
    }    
    virtual void on_tick(lquant_tick_pool* pool, int didx, const std::string &symbol) = 0;      //处理行情
    virtual int calc() = 0;

    void set_response_callback(at_response_fun fun)               //设置委托指令回调函数
    {
        on_response_ = fun;
    }

    // void set_transaction_callback(at_transaction_fun fun)               //设置委托指令回调函数
    // {
    //     on_transaction = fun;
    // }

protected:
    at_response_fun on_response_;                       //委托回报回调句柄
    // at_transaction_fun on_transaction;
    std::list<lquant_order_t*> orders_;
    Statistics *sts_ = nullptr;
};

}

// extern "C"
// {
//     lfquant::quant_backtrace_base *create_strategy();
// }