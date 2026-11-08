#pragma once

#include <string>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include <map>
#include <vector>

#include "lf_define.h"
#include "CJsonObject.hpp"

namespace lfquant
{

inline void localtime_sr(struct tm* t, time_t * tt)
{
#ifdef WIN32
	localtime_s(t, tt);
#else
	localtime_r(tt, t);
#endif	
}

int getDay();
int getTime();
int64_t getMsTime();
int64_t getLocalMsTime();
int64_t getUsTime();
int64_t getLocalUsTime();
int getTimeFromStamp(int64_t timeStamp);
inline std::string getDayStr() { return std::to_string(getDay()); }
inline std::string getTimeStr() { return std::to_string(getTime()); }

void replace(std::string &s, const char *src, const char *dst);
void split(const std::string &str, const std::string& delims, std::vector<std::string> &out);
inline std::vector<std::string> split(const std::string &str, const std::string& delims)
{
	std::vector<std::string> out;
	split(str, delims, out);
	return out;
}

inline int tme2seconds(int t) { return (t / 10000 * 3600) + ((t % 10000)/100 * 60) + (t % 100); }
inline int seconds2tme(int t) { return (t / 3600) * 10000 + ((t % 3600)/60 * 100) + (t % 60); }

int calc_trade_seconds(int t1, int t2);

void SystemErrorHandler(int signum);
void init_system_error_handle();

inline void sleep(int seconds)
{
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

inline void msleep(int millseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(millseconds));
}

std::string trim(std::string &str);
inline std::string get_trim(const std::string &str)
{
	std::string s = str;
	return trim(s);
}
std::string getValue(const std::string &data, const std::string &key);
std::string getValueEx(const std::string &data, const std::string &key);
std::string getValue(const std::map<std::string,std::string> &params, const std::string &key, const std::string &defaultValue);
int getValue(const std::map<std::string,std::string> &params, const std::string &key, int defaultValue);
double getValue(const std::map<std::string,std::string> &params, const std::string &key, double defaultValue);

unsigned int SDBMHash(const char *str);

std::string getLocalIp();

inline int hhmmss_to_s(int hhmmss) {
	return hhmmss/10000 * 3600 + (hhmmss/100 % 100) * 60 + hhmmss % 100;
}

inline int s_to_hhmmss(int s) {
	return s/3600 * 10000 + s % 3600 / 60 * 100 + s % 60;
}

inline int hhmmss_to_ms(unsigned hmsint) {
	int ms;
	unsigned hh;
	unsigned mm;
	unsigned ss;

	ss = hmsint % 100;
	mm = (hmsint - ss) / 100 % 100;
	hh = hmsint / 10000;

	ms = (hh * 3600 + mm * 60 + ss) * 1000;

	return (ms);
}

inline int ms_to_hhmmss(unsigned ms) {
	int sec;
	int ss;
	int mm;
	int hh;
	int hhmmss;

	if((unsigned int)(-1)==ms) {
		hhmmss=-1;
	}
	else {
		sec = ms / 1000;
		ss = sec % 60;
		mm = sec / 60;
		mm = mm % 60;
		hh = sec / 3600;

		hhmmss=hh*10000 + mm*100 + ss;
	}
	return(hhmmss);
}

inline int hhmmssxxx_to_ms(int hhmmssxxx) {
	return hhmmss_to_s(hhmmssxxx/1000)*1000 + hhmmssxxx % 1000;
}

inline int ms_to_hhmmssxxx(int ms) {
	return s_to_hhmmss(ms/1000)*1000 + ms % 1000;
}

inline int get_min_volume(const std::string &symbol)
{
    if(symbol.size() >= 2 && symbol[0] == '6' && symbol[1] == '8')
        return 200;
    return 100;
}

void generate_clientid(char *buf, int &cid);
void generate_prefix_clientid(char *buf, const std::string &prefix, int &cid);
int mkpath(std::string s, mode_t mode/*=0755*/);

void get_basket_from_json(const neb::CJsonObject& data, lf_basket &basket);
neb::CJsonObject get_basket_json(const lf_basket &basket);

void get_instruct(lf_instruct &ins, const std::string &data);
void get_order_update(lf_order_update &ord, const std::string &data);
void get_trade(lf_trade &trd, const std::string &data);

std::string encrypt(const std::string &plainText);
std::string encrypt(const std::string &plainText, const std::string &key, const std::string &iv);
std::string decrypt(const std::string &hexText);
std::string decrypt(const std::string &hexText, const std::string &key, const std::string &iv);

std::map<std::string, std::string> parse_basket_params(const std::string &str);
std::string get_map_value(const std::map<std::string, std::string> &m, const std::string &key);

typedef enum {
	GBK,
	UTF8,
	UNKOWN
} LF_CODING;

bool isUtf8(unsigned char* data, int len);
bool isGBK(unsigned char* data, int len);
LF_CODING GetCoding(unsigned char* data, int len);
int GbkToUtf8(char *str_str, size_t src_len, char *dst_str, size_t dst_len);
int Utf8ToGbk(char *src_str, size_t src_len, char *dst_str, size_t dst_len);

std::string get_gbk_string(const char *str, int len);
inline std::string get_gbk_string(const std::string &str) { return get_gbk_string(str.c_str(), str.size()); }
std::string get_utf8_string(const char *str, int len);
inline std::string get_utf8_string(const std::string &str) { return get_utf8_string(str.c_str(), str.size()); }

bool is_single_process(const std::string &ProcName);

class lf_redis;
lf_redis* create_redis(const std::string &ip, int port, const std::string &pwd, bool isCluster);
// std::string get_device_id(const char *devname);

}
