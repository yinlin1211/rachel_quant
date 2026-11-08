//=======================================================================================
/**
 *  @file    result_metadata.h
 *
 *  ver 1.0.0 2013/10/01 mysqlppc,a lightweight C++ wrapper for accessing MySQL database
 *
 *  @author Wooce (Wooce Yang,mailto:wooce@21cn.com) 2013/10/01 created.
 */
//=======================================================================================
#ifndef _MYSQLPPC_RESULTMETADATA_H
#define _MYSQLPPC_RESULTMETADATA_H

#include <string>
#include <map>
#include "exception.h"

namespace mysqlppc
{
	using std::string;
	
class ResultMetadata
	/// MySQL result metadata
{
public:
	ResultMetadata(MYSQL_STMT* stmt);
		/// Initializes the metadata.
	~ResultMetadata();

	unsigned columnsReturned() const{ return m_cols; }
		/// Returns the number of columns in resultset.	

	bool has(const string &name);

	int getInt(unsigned i);
	long long getLong(unsigned i);
	std::string getString(unsigned i);
	float getFloat(unsigned i);
	double getDouble(unsigned i);
	
	int getInt(const string &name);
	long long getLong(const string &name);
	string getString(const string &name);
	float getFloat(const string &name);
	double getDouble(const string &name);

	MYSQL_BIND* data() { return m_data; }
	
private:
	void setFieldName(unsigned i, const string &name);
	unsigned getFieldIndex(const string &name);
	
	void makeInt(unsigned i);
	void makeLong(unsigned i);
	void makeString(unsigned i, unsigned size);
	void makeString(unsigned i, unsigned size, int type);
	void makeTime(unsigned i, int type);
	void makeFloat(unsigned i);
	void makeDouble(unsigned i);
	void safe_free_str(unsigned int i);
	
	MYSQL_BIND* m_data;
	union ValType {
		int ival;
		long long llval;
		char* strval;
		float fval;
		double dval;
	};
	ValType* m_vals;
	unsigned long* m_length;
	my_bool* m_isNull;
	unsigned m_cols;
	std::map<std::string,unsigned> m_fieldNameMap;
};

}

#endif // RESULTMETADATA_H
