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



bool MysqlConn::prepare(const string& sql)
{
    if (m_stmt) closeStmt();
    m_stmt = mysql_stmt_init(m_conn);
    if (!m_stmt) return false;
    if (mysql_stmt_prepare(m_stmt, sql.c_str(), sql.length())) return false;
    return true;
}

bool MysqlConn::bindParam(MYSQL_BIND* bind)
{
    if (!m_stmt) return false;
    if (mysql_stmt_bind_param(m_stmt, bind)) return false;
    return true;
}

bool MysqlConn::execute()
{
    if (!m_stmt) return false;
    if (mysql_stmt_execute(m_stmt)) return false;
    return true;
}

bool MysqlConn::storeResult()
{
    if (!m_stmt) return false;
    if (mysql_stmt_store_result(m_stmt)) return false;
    return true;
}

bool MysqlConn::fetch()
{
    if (!m_stmt) return false;
    return mysql_stmt_fetch(m_stmt) == 0;
}


bool MysqlConn::fetchInt(int& out)
{
    if (!m_stmt) return false;
    MYSQL_BIND resultBind[1] = {0};
    resultBind[0].buffer_type = MYSQL_TYPE_LONG;
    resultBind[0].buffer = &out;
    resultBind[0].buffer_length = sizeof(out);
    resultBind[0].is_null = 0;
    resultBind[0].length = 0;
    mysql_stmt_bind_result(m_stmt, resultBind);
    return mysql_stmt_fetch(m_stmt) == 0;
}


string MysqlConn::getString(int index)
{
    // 这里只做简单实现，实际应根据字段类型处理
    char buf[1024] = {0};
    unsigned long len = 0;
    MYSQL_BIND bind = {0};
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = buf;
    bind.buffer_length = sizeof(buf);
    bind.length = &len;
    bind.is_null = 0;
    mysql_stmt_bind_result(m_stmt, &bind);
    if (mysql_stmt_fetch(m_stmt) == 0)
        return string(buf, len);
    return string();
}

void MysqlConn::closeStmt()
{
    if (m_stmt) {
        mysql_stmt_close(m_stmt);
        m_stmt = nullptr;
    }
}
