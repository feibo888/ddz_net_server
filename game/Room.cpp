//
// Created by fb on 2025/6/12.
//

#include "Room.h"

#include <JsonParse.h>


Room::~Room()
{
    if (m_redis)
    {
        delete m_redis;
    }
}

bool Room::initEnvironment()
{
    JsonParse json;
    auto info = json.getDBInfo(JsonParse::Redis);

    //uri — URI, e. g. 'tcp:// 127.0.0.1', 'tcp:// 127.0.0.1:6379', or 'unix:// path/ to/ socket'. Full URI
    std::string connStr = "tcp://" + info->ip + ":" + std::to_string(info->port);
    m_redis = new sw::redis::Redis(connStr);

    //测试
    if (m_redis->ping() == "PONG")
    {
        return true;
    }
    return false;
}

void Room::clear()
{
    // flushdb
    m_redis->flushdb();
}

void Room::saveRsaSecKey(std::string field, std::string value)
{
    m_redis->hset("RSA", field, value);
}

std::string Room::getRsaSecKey(std::string field)
{
    auto value = m_redis->hget("RSA", field);
    if (value.has_value())
    {
        return value.value();
    }
    return std::string();
}


std::string Room::joinRoom(std::string userName)
{
    std::optional<std::string> room;
    do
    {
        //scard 先找两人间
        if (m_redis->scard(TwoPlayers) > 0)
        {
            //srandmember
            room = m_redis->srandmember(TwoPlayers);
            break;
        }
        if (m_redis->scard(OnePlayer) > 0)
        {
            //srandmember
            room = m_redis->srandmember(OnePlayer);
            break;
        }
        //添加新的房间
        room = getRoomName();
    }
    while (0);

    //加入到某个房间
    joinRoom(userName, room.value());

    return room.value();
}

bool Room::joinRoom(std::string userName, std::string roomName)
{
    //zcard,用于返回有序集合中的成员个数。
    if (m_redis->zcard(roomName) >= 3)
    {
        return false;
    }
    //检查房间是否存在
    if (!m_redis->exists(roomName))
    {
        //sadd将一个或多个成员元素加入到集合中，已经存在于集合的成员元素将被忽略。
        m_redis->sadd(OnePlayer, roomName);
    }
    else if (m_redis->sismember(OnePlayer, roomName))
    {
        //smove, 将一个集合中的数据移动到另一个集合
        m_redis->smove(OnePlayer, TwoPlayers, roomName);
    }
    else if (m_redis->sismember(TwoPlayers, roomName))
    {
        m_redis->smove(TwoPlayers, ThreePlayers, roomName);
    }
    else
    {
        return false;
    }

    //将玩家添加到房间，使用的结构是sortedset
    m_redis->zadd(roomName, userName, 0);
    //将玩家存储起来， hash-> 通过玩家找到玩家的房间
    m_redis->hset("Players", userName, roomName);

    return true;

}

std::string Room::getRoomName()
{
    //创建随机设备对象
    std::random_device rd;
    //创建随机数生成对象
    std::mt19937 gen(rd());
    //创建随机数分布对象， 均匀分布
    std::uniform_int_distribution<int> distrib(100000, 999999);
    int randNum = distrib(gen);
    return std::to_string(randNum);
}

int Room::getPlayerCount(std::string roomName)
{
    //zcard,用于返回有序集合中的成员个数。
    return m_redis->zcard(roomName);
}

void Room::UpdatePlayerScore(std::string roomName, std::string userName, int score)
{
    m_redis->zadd(roomName, userName, score);
}

std::string Room::whereAmI(std::string userName)
{
    auto value = m_redis->hget("Players", userName);
    if (value.has_value())
    {
        return value.value();
    }
    return std::string();
}

int Room::getPlayerScore(std::string roomName, std::string userName)
{
    auto score = m_redis->zscore(roomName, userName);
    if (score.has_value())
    {
        return score.value();
    }
    return 0;
}

std::string Room::getPlayerOrder(std::string roomName)
{
    int index = 0;
    std::string data;
    //对房间中的玩家进行排序
    std::vector<std::pair<std::string, double>> out;
    m_redis->zrevrange(roomName, 0, -1, std::back_inserter(out));

    for (auto& it : out)
    {
        index++;
        data += it.first + "-" + std::to_string(index) + "-" + std::to_string((int)it.second) + "#";
    }
    return data;
}

void Room::leaveRoom(std::string roomName, std::string userName)
{
    if (m_redis->sismember(ThreePlayers, roomName))
    {
        m_redis->smove(ThreePlayers, Invalid, roomName);
    }
    //从房间中删除玩家
    m_redis->zrem(roomName, userName);
    auto count = m_redis->zcard(roomName);
    if (count == 0)
    {
        m_redis->del(roomName);
        m_redis->srem(Invalid, roomName);
    }
}

bool Room::searchRoom(std::string roomName)
{
    bool flag = m_redis->sismember(TwoPlayers, roomName);
    if (!flag)
    {
        flag = m_redis->sismember(OnePlayer, roomName);
    }
    return flag;
}
