#pragma once
#include <string>
#include <mysql.h>

using namespace std;

class MysqlConn
{
public:
	//初始化数据库连接
	MysqlConn();

	//释放数据库连接
	~MysqlConn();

	//连接数据库
	bool connect(string user, string passwd, string dbName, string ip, unsigned short port = 3306);

	//更新数据库 insert, update, delete
	bool update(string sql);

	//查询数据库
	bool query(string sql);

	//遍历查询得到的结果集
	bool next();

	//得到结果集中的字段值
	string value(int index);

	//事务操作
	void transaction();

	//提交事务
	void commit();

	//事务回滚
	void rollback();


	// 新增：参数化查询接口
    bool prepare(const string& sql);
    bool bindParam(MYSQL_BIND* bind);
    bool execute();
    bool storeResult();
    bool fetch();
	bool fetchInt(int& out);
    string getString(int index);
    void closeStmt();

private:
	void freeRes();

	MYSQL* m_conn = nullptr;
	MYSQL_RES* m_res = nullptr;
	MYSQL_ROW m_row = nullptr;

	// 新增
    MYSQL_STMT* m_stmt = nullptr;
};

