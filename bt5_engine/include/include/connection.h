//=======================================================================================
/**
 *  @file    connection.h
 *
 *  ver 1.0.0 2013/10/01 mysqlppc,a lightweight C++ wrapper for accessing MySQL database
 *
 *  @author Wooce (Wooce Yang,mailto:wooce@21cn.com) 2013/10/01 created.
 */
//=======================================================================================
#ifndef _MYSQLPPC_CONNECTION_H_
#define _MYSQLPPC_CONNECTION_H_

#include <string>

#include "statement.h"

namespace mysqlppc {
	using std::string;
	class Connection {
		public:
			static Connection* get_connction(const string& host, unsigned port,
					const string& uname, const string& passwd, 
					const string& db);
			// static Connection* get_connction(const string& sock,
			// 		const string& uname, const string& passwd, 
			// 		const string& db);

			~Connection();
			operator bool ();

			class Statement* prepare(const string& sql);
			void charset(const std::string& cs);

			bool ping();
			void setAutoCommit(bool isAutoCommit);
			void startTransaction();
			void rollback();
			void commit();
			int execute_query(const char *sql);

		private:
			Connection()
			{
				
			}
			Connection(const Connection& other);
			const Connection& operator=(const Connection& other);
			void set_reconnect();

			MYSQL* m_conn = nullptr;
	};
}

#endif
