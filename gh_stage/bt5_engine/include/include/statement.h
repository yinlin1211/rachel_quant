//=======================================================================================
/**
 *  @file    statement.h
 *
 *  ver 1.0.0 2013/10/01 mysqlppc,a lightweight C++ wrapper for accessing MySQL database
 *
 *  @author Wooce (Wooce Yang,mailto:wooce@21cn.com) 2013/10/01 created.
 */
//=======================================================================================
#ifndef _MYSQLPPC_STATEMENT_H_
#define _MYSQLPPC_STATEMENT_H_

#include <string>

#include "exception.h"
#include "binder.h"

namespace mysqlppc {
	using std::string;
	class Statement {
		public:
			enum State
			{
				STMT_INITED,
				STMT_COMPILED,
				STMT_EXECUTED
			};

			Statement(MYSQL* conn, const string& sql);
			~Statement();

			void execute();	/// Executes the statement.
			long long lastId();
			int affectedRows();

			class ResultSet* getResultSet();

			template<typename T>
				void set(unsigned i, const T& val)
				{
					if (m_binder == NULL)
						throw(MysqlException(ERRORS, "no param to bind"));
					m_binder->bind(i, val);
				}
			void setInt(unsigned i, int val);
			void setFloat(unsigned i, float val);
			void setDouble(unsigned i, double val);
			void setLong(unsigned i, long long val);
			void setString(unsigned i, const string& val);
		private:
			MYSQL* m_conn;
			MYSQL_STMT* m_stmt;
			int m_state;
			class Binder* m_binder;
	};
}
#endif
