
#ifndef LQUANTSELFLOG_H_
#define LQUANTSELFLOG_H_
#include<iostream>
#include <unordered_map>
#include <cstring>
#include <functional>
#include "lquantlog/lquantlog.h"
#include "xml_parser.hpp"


struct LQUANTSELFLog {
    static std::shared_ptr<lquantlog::logger> logger;
    static void init(lquant::XmlNode cfg){
      LQUANTSELFLog::logger = lquantlog::daily_logger_mt("file_logger", cfg.getAttrDefault("log_file", "tradelog.txt"), 8, 0);
      lquantlog::set_async_mode(4096, 0);
      std::unordered_map<std::string, lquantlog::level::level_enum> log_levels = {{"warn", lquantlog::level::warn}, {"info", lquantlog::level::info}, {"debug", lquantlog::level::debug}};
      LQUANTSELFLog::logger->set_level(log_levels[cfg.getAttrDefault("log_level", "debug")]);
      LQUANTSELFLog::logger->flush_on(lquantlog::level::info);
      lquantlog::set_pattern("[%H:%M:%S.%f] %v");
    }
    //static init() {
    /*    if(!print) {
                //      globalData->m_logger = lquantlog::daily_logger_mt("file_logger", _global_node.getAttrDefault("log_file", "tradelog.txt"), 8, 0);
                LQUANTSELFLog::logger = lquantlog::daily_logger_mt("file_logger", _global_node.getAttrDefault("log_file", "tradelog.txt"), 8, 0);
            } else {
        //        globalData->m_logger = lquantlog::stdout_color_mt("console");
                LQUANTSELFLog::logger = lquantlog::stdout_color_mt("console");
            }
            std::unordered_map<std::string, lquantlog::level::level_enum> log_levels = {{"warn", lquantlog::level::warn}, {"info", lquantlog::level::info}, {"debug", lquantlog::level::debug}};
        //    globalData->m_logger->set_level(log_levels[_global_node.getAttrDefault("log_level", "debug")]);
        //    globalData->m_logger->flush_on(lquantlog::level::info);
            LQUANTSELFLog::logger->set_level(log_levels[_global_node.getAttrDefault("log_level", "debug")]);
            LQUANTSELFLog::logger->flush_on(lquantlog::level::info);
            lquantlog::set_pattern("%Y%m%d[%H:%M:%S.%f] %v");*/
    // }
    // 使用 在main.cpp 中初始化spdlog设置
    // std::shared_ptr<lquantlog::logger> LQUANTSELFLog::logger;
    // LQUANTSELFLog::logger = lquantlog::daily_logger_mt("file_logger", _global_node.getAttrDefault("log_file", "tradelog.txt"), 8, 0);
    // lquantlog::set_async_mode(4096, 0);
    // std::unordered_map<std::string, lquantlog::level::level_enum> log_levels = {{"warn", lquantlog::level::warn}, {"info", lquantlog::level::info}, {"debug", lquantlog::level::debug}};
    // LQUANTSELFLog::logger->set_level(log_levels[_global_node.getAttrDefault("log_level", "debug")]);
    // LQUANTSELFLog::logger->flush_on(lquantlog::level::info);
    // lquantlog::set_pattern("[%H:%M:%S.%f] %v");
};
#define LOG_INFO(X, ...) \
    LQUANTSELFLog::logger->info(X, ##__VA_ARGS__);
#define LOG_DEBUG(X, ...) \
    LQUANTSELFLog::logger->debug(X, ##__VA_ARGS__);
#define LOG_WARN(X, ...) \
    LQUANTSELFLog::logger->warn(X, ##__VA_ARGS__);
#define LOG_ERROR(X, ...) \
    LQUANTSELFLog::logger->error(X, ##__VA_ARGS__);
#define LOG_FLUSH() \
    LQUANTSELFLog::logger->flush();

#include <chrono>
// 函数耗时统计  us
class FuncCall {
    public:
      FuncCall(const char * pfuncName) {
          funcName = pfuncName;
        //   start = std::chrono::steady_clock::now();
          LOG_INFO("{} enter", funcName);
      }
      ~FuncCall() {
        //   auto end = std::chrono::steady_clock::now();
        //   NLOG_INFO << funcName << " use_tme:" << std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count() * 1000 * 1000;
        //   LOG_INFO("{} use_tme:{}", funcName, std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count() * 1000 * 1000);
          LOG_INFO("{} leave", funcName);
      }

    private:
      std::string funcName;
    //   std::chrono::steady_clock::time_point start;
};

#define FUNC_CALL FuncCall cc(__FUNCTION__);
#endif
