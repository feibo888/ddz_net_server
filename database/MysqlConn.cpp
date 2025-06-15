#include "MysqlConn.h"

MysqlConn::MysqlConn()
{
	m_conn = mysql_init(nullptr);
	mysql_set_character_set(m_conn, "utf8");

}

MysqlConn::~MysqlConn()
{
	if (m_conn)
	{
		mysql_close(m_conn);
	}
	freeRes();
}

bool MysqlConn::connect(string user, string passwd, string dbName, string ip, unsigned short port)
{
	MYSQL* ptr = mysql_real_connect(m_conn, ip.c_str(), user.c_str(), passwd.c_str(), dbName.c_str(), port, nullptr, 0);

	return ptr != nullptr;
}

bool MysqlConn::update(string sql)
{
	if (mysql_query(m_conn, sql.c_str()))
	{
		return false;
	}
	return true;
}

bool MysqlConn::query(string sql)
{
	freeRes();
	if (mysql_query(m_conn, sql.c_str()))
	{
		return false;
	}
	m_res = mysql_store_result(m_conn);
	return true;
}

bool MysqlConn::next()
{
	if (m_res)
	{
		m_row = mysql_fetch_row(m_res);
		if (m_row)
		{
			return true;
		}
	}
	return false;
}

string MysqlConn::value(int index)
{
	int colCount = mysql_num_fields(m_res);
	if (index >= colCount || index < 0)
	{
		return string();
	}
	char* val = m_row[index];
	unsigned long length = mysql_fetch_lengths(m_res)[index];

	return string(val, length);
}

void MysqlConn::transaction()
{
	mysql_autocommit(m_conn, false);
}

void MysqlConn::commit()
{
	mysql_commit(m_conn);
	mysql_autocommit(m_conn, true);
}

void MysqlConn::rollback()
{
	mysql_rollback(m_conn);
	mysql_autocommit(m_conn, true);
}

void MysqlConn::freeRes()
{
	if (m_res)
	{
		mysql_free_result(m_res);
		m_res = nullptr;
	}
}
