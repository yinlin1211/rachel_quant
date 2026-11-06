//=======================================================================================
/**
 *  @file    binder.h
 *
 *  ver 1.0.0 2013/10/01 mysqlppc,a lightweight C++ wrapper for accessing MySQL database
 *
 *  @author Wooce (Wooce Yang,mailto:wooce@21cn.com) 2013/10/01 created.
 */
//=======================================================================================
#ifndef _MYSQLPPC_BINDER_H_
#define _MYSQLPPC_BINDER_H_

#include <string>
#include "exception.h"
// typedef struct st_mysql_bind MYSQL_BIND;

namespace mysqlppc {
	using std::string;

	class Binder {
		public:
			Binder(std::size_t size);
			~Binder();

			void bind(std::size_t pos, const int& val);
			void bind(std::size_t pos, const long long& val);
			void bind(std::size_t pos, const string& val);
			void bind(std::size_t pos, const float& val);
			void bind(std::size_t pos, const double& val);
			void bind(std::size_t pos, const char val);
			void bind(std::size_t pos, const char* val);

			size_t size() const;	     /// Return count of binded parameters
			MYSQL_BIND* data() const{ return m_data; }
		private:

			MYSQL_BIND* m_data;
			union ValType {
				int ival;
				long long llval;
				char* strval;
				float fval;
				double dval;
			};

			ValType* m_vals;
			size_t m_nSize;
	};
};
#endif
