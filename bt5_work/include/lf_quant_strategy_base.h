#pragma once

#include "lquantOrder.hpp"
#include "lf_define.h"

namespace lfquant 
{
class quant_strategy_base;
class quant_backtrace_base;
typedef std::function<bool(int, const lquant_order_t&, const std::string&)> at_request_fun;
typedef quant_strategy_base* (*algo_strategy_create_fun)();
int main_impl(algo_strategy_create_fun stg, int argc, char** argv);

class quant_strategy_base {
    public:
        quant_strategy_base() {}
        virtual ~quant_strategy_base() {}
        void set_name(const std::string& name) { name_ = name; }
        const std::string& get_name() const { return name_; }
        void set_stg_id(int id) { algoInstanceId_ = id; }
        int get_stg_id() { return algoInstanceId_; }
        void set_request_callback(at_request_fun fun) { on_request_ = fun; }  // 设置委托指令回调函数

        virtual void start(const lf_basket& basket) = 0;  // 开始篮子
        virtual void stop() = 0;  // 停止篮子
        virtual void pause(bool isPause) = 0;  // 暂停篮子
        virtual void on_response(const lquant_order_t& order) = 0;  // 处理委托回报

        virtual void start_sim(lf_basket basket, int tradeDate, std::string tickPath, std::string scoreFile) {}  // 篮子回测
        virtual void set_backtrace(quant_backtrace_base* p) { pbt_ = p; }  // 设置回测撮合模块
        virtual void pingcang(const std::string& symbol) {}  // 平仓
        virtual void stop_symbol(const std::string& symbol) {}  // 停止股票

    protected:
        std::string name_;  // 策略名
        int algoInstanceId_ = 0;  // 策略唯一ID
        at_request_fun on_request_ = nullptr;  // 委托指令回调句柄
        quant_backtrace_base* pbt_ = nullptr;  // 撮合模块
};
}

extern "C" {
    lfquant::quant_strategy_base* create_strategy();
}