//
// Created by fb on 2025/6/12.
//

#ifndef JSONPARSE_H
#define JSONPARSE_H

#include <string>
#include <json/json.h>
#include <memory>

struct DBInfo
{
    std::string user;
    std::string password;
    std::string ip;
    unsigned short port;
    std::string dbname;
};


class JsonParse
{
public:
    enum DBType{Mysql, Redis};

    JsonParse(std::string fileName = "../config/config.json");

    //获取数据
    std::shared_ptr<DBInfo> getDBInfo(DBType type);

private:
    Json::Value m_root;

};



#endif //JSONPARSE_H
