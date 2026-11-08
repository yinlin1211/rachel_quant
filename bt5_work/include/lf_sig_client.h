#pragma once
#include <string>
#include <thread>
#include <set>
#include <deque>
#include <vector>

#include "lf_strategy_define.h"

class SignalClient
{
public:
	void init(std::string endpoint, std::string topic);
	void start();
	void stop() { isRunning_ = false; }

    void set_callback(lfquant::lf_net_signal_fun fun)
    {
        fun_ = fun;
    }
private:
	void run(int socket);

private:
	bool isRunning_ = false;
    std::string endpoint_;
    std::string topic_ = "LFA";
    std::vector<int> sockets_;
    lfquant::lf_net_signal_fun fun_ = nullptr;
};
