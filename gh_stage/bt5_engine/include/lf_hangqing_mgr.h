#pragma once

#include <atomic>
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <mutex>

#include "lf_strategy_define.h"

namespace lfquant
{

class lf_hangqing_mgr
{
public:
	static lf_hangqing_mgr& getInstance()
	{
		static lf_hangqing_mgr inst_;
		return inst_;
	}
	
	void init(std::string endpoint);
	void init_tkrs(std::string tkrPath);
	void start();
	void stop() { isRunning_ = false; }

	lf_hangqing* get_hangqing(const std::string &symbol) { return get_hangqing(symbol.c_str()); }
	lf_hangqing* get_hangqing(const char *symbol);

private:
	lf_hangqing_mgr();
	void run(int socket);

	lf_hangqing* try_get_hangqing(const std::string &symbol) { return try_get_hangqing(symbol.c_str()); }
	lf_hangqing* try_get_hangqing(const char *symbol);

	std::mutex marketMap_mutex_;
	std::unordered_map<std::string, lf_hangqing*> marketMap_;

	bool isRunning_ = true;
    std::string endpoint_;
    std::vector<int> sockets_;
	// std::vector<std::string> tkrs_;
};

}
