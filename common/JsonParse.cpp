//
// Created by fb on 2025/6/12.
//

#include "JsonParse.h"

#include <assert.h>
#include <fstream>
#include <iostream>

JsonParse::JsonParse(std::string fileName)
{
    std::ifstream ifs(fileName);
    assert(ifs.is_open());

    Json::Reader rd;
    rd.parse(ifs, m_root);
    assert(m_root.isObject());
}

std::shared_ptr<DBInfo> JsonParse::getDBInfo(DBType type)
{
    std::string dbname = type == DBType::Mysql ? "mysql" : "redis";
    Json::Value node = m_root[dbname];

    DBInfo* info = new DBInfo();


    info->ip = node["ip"].asString();
    info->port = node["port"].asInt();

    if (type == DBType::Mysql)
    {
        info->dbname = node["dbname"].asString();
        info->user = node["user"].asString();
        info->password = node["password"].asString();
    }
    return std::shared_ptr<DBInfo>(info);
}
