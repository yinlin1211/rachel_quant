#pragma once

#include <iostream>
#include <queue>
#include <map>
#include <vector>
#include <utility>
#include <string>
#include <mutex>
#include <thread>
#include <functional>

#include "connection.h"

namespace mysqlppc
{

class MysqlPool {
public:
	MysqlPool() {}
	~MysqlPool();

	void init(const char*   mysqlhost,
		unsigned int  port,
		const char*   mysqluser,
		const char*   mysqlpwd,
		const char*   databasename,
		unsigned int  MAX_CONNECT = 4);

	void init(const std::string& mysqlhost,
		unsigned int  port,
		const std::string&   mysqluser,
		const std::string&   mysqlpwd,
		const std::string&   databasename,
		unsigned int  MAX_CONNECT = 4)
	{
		init(mysqlhost.c_str(), port, mysqluser.c_str(), mysqlpwd.c_str(), databasename.c_str(), MAX_CONNECT);
	}

	Connection* getConnection();
	void close(Connection* conn);

	void setErrorLogCallback(std::function<void(const char*)> callback)
	{
		logFunction_ = callback;
	}

private:
	std::string   dbHost_;                     //mysql主机地址
	unsigned int  dbPort_ = 3306;              //mysql端口
	std::string   dbUser_;                     //mysql用户名
	std::string   dbPwd_;	                   //mysql密码
	std::string   database_;                   //要使用的mysql数据库名字
	unsigned int  MAX_CONNECT = 4;             //同时允许最大连接对象数量
	unsigned int  connect_count_ = 0;          //目前连接池的连接对象数量
	
	std::queue<Connection*> pool_;                 
	std::mutex lock_;

	std::function<void(const char*)> logFunction_ = nullptr;
};

}
