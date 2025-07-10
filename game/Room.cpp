//
// Created by fb on 2025/6/12.
//

#include "Room.h"

#include <JsonParse.h>
#include <random>
#include <thread>
#include <chrono>


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
    const int MAX_RETRY = 3;      // 最大重试次数
    const int RETRY_DELAY_MS = 50; // 重试间隔(毫秒)

    for (int retry = 0; retry < MAX_RETRY; ++retry) {
        // 尝试获取全局锁
        std::string lockKey = "room_join_lock";
        if (acquireLock(lockKey, 2)) { // 减少锁持有时间

            std::optional<std::string> room;

            // 在锁保护下选择房间
            if (m_redis->scard(TwoPlayers) > 0) {
                room = m_redis->srandmember(TwoPlayers);
            } else if (m_redis->scard(OnePlayer) > 0) {
                room = m_redis->srandmember(OnePlayer);
            } else {
                // 没有合适房间，创建新房间
                room = getRoomName();
            }

            // 尝试加入选中的房间
            bool success = joinRoom(userName, room.value());
            releaseLock(lockKey);

            if (success) {
                return room.value();
            }
            // 加入失败，继续重试
        }

        // 获取锁失败或加入房间失败，短暂等待后重试
        if (retry < MAX_RETRY - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS * (retry + 1)));
        }
    }

    // 所有重试都失败，创建新房间作为最后的降级策略
    std::string newRoom = getRoomName();
    joinRoom(userName, newRoom);
    return newRoom;
}

bool Room::joinRoom(std::string userName, std::string roomName)
{
    // 为特定房间加锁，防止多个玩家同时加入同一房间
    std::string roomLockKey = "room_lock_" + roomName;
    if (!acquireLock(roomLockKey, 3)) {
        return false; // 获取房间锁失败
    }

    // 再次检查房间人数（双重检查锁定模式）
    if (m_redis->zcard(roomName) >= 3)
    {
        releaseLock(roomLockKey);
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
        releaseLock(roomLockKey);
        return false;
    }

    //将玩家添加到房间，使用的结构是sortedset
    m_redis->zadd(roomName, userName, 0);
    //将玩家存储起来， hash-> 通过玩家找到玩家的房间
    m_redis->hset("Players", userName, roomName);

    // 释放房间锁
    releaseLock(roomLockKey);
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
    return m_redis->exists(roomName);
}

// 新增：生成唯一锁标识符
std::string Room::generateLockValue()
{
    // 使用线程ID + 时间戳 + 随机数生成唯一值
    auto threadId = std::this_thread::get_id();
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distrib(1000, 9999);
    int randNum = distrib(gen);

    std::ostringstream oss;
    oss << threadId << "_" << timestamp << "_" << randNum;
    return oss.str();
}

// 使用lua脚本实现分布式锁
bool Room::acquireLock(const std::string& lockKey, int expireSeconds)
{
    try
    {
        // 生成唯一的锁值
        std::string lockValue = generateLockValue();

        // 使用SET命令的NX选项实现分布式锁
        bool result = m_redis->set(lockKey, lockValue,
                                  std::chrono::seconds(expireSeconds),
                                  sw::redis::UpdateType::NOT_EXIST);

        if (result)
        {
            // 获取锁成功，保存锁值用于后续安全释放
            m_lockValues[lockKey] = lockValue;
        }

        return result;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

void Room::releaseLock(const std::string& lockKey)
{
    try
    {
        // 查找当前进程持有的锁值
        auto it = m_lockValues.find(lockKey);
        if (it == m_lockValues.end())
        {
            return; // 当前进程没有持有这个锁
        }

        std::string lockValue = it->second;

        // 使用Lua脚本原子性地检查并删除锁
        std::string luaScript = R"(
            if redis.call("GET", KEYS[1]) == ARGV[1] then
                return redis.call("DEL", KEYS[1])
            else
                return 0
            end
        )";

        // 执行Lua脚本
        // KEYS[1] = lockKey, ARGV[1] = lockValue
        auto result = m_redis->eval<long long>(luaScript, {lockKey}, {lockValue});

        // 清理本地记录
        m_lockValues.erase(it);

    }
    catch (const std::exception& e)
    {
        // 忽略删除锁时的异常，但清理本地记录
        m_lockValues.erase(lockKey);
    }
}

// 新增：无锁房间匹配算法
std::string Room::joinRoomWithoutLock(std::string userName)
{
    const int MAX_ATTEMPTS = 5;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
    {
        // 1. 按优先级顺序尝试加入房间
        // 优先级：2人房间 > 1人房间 > 创建新房间

        // 1.1 优先尝试加入2人房间（可以立即开始游戏）
        std::vector<std::string> twoPlayerRooms = getTwoPlayerRooms();
        for (const std::string& roomName : twoPlayerRooms)
        {
            if (joinRoom(userName, roomName))
            {
                return roomName; // 成功加入2人房间
            }
        }

        // 1.2 然后尝试加入1人房间
        std::vector<std::string> onePlayerRooms = getOnePlayerRooms();
        for (const std::string& roomName : onePlayerRooms)
        {
            if (joinRoom(userName, roomName))
            {
                return roomName; // 成功加入1人房间
            }
        }

        // 1.3 没有合适的现有房间，尝试创建新房间
        std::string newRoom = getRoomName();
        if (joinRoom(userName, newRoom))
        {
            return newRoom; // 成功创建并加入新房间
        }

        // 1.4 短暂等待后重试（避免创建过多房间）
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + attempt * 10));
    }

    // 最后的保底方案：强制创建新房间
    std::string fallbackRoom = getRoomName();
    joinRoom(userName, fallbackRoom);
    return fallbackRoom;
}

// 获取2人房间列表（随机化以避免热点）
std::vector<std::string> Room::getTwoPlayerRooms()
{
    std::vector<std::string> rooms;

    try
    {
        m_redis->smembers(TwoPlayers, std::back_inserter(rooms));

        // 随机化2人房间列表，避免所有人都抢同一个房间
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(rooms.begin(), rooms.end(), gen);

    }
    catch (const std::exception&)
    {
        // 异常时返回空列表
    }

    return rooms;
}

// 获取1人房间列表（随机化以避免热点）
std::vector<std::string> Room::getOnePlayerRooms()
{
    std::vector<std::string> rooms;

    try
    {
        m_redis->smembers(OnePlayer, std::back_inserter(rooms));

        // 随机化1人房间列表，避免所有人都抢同一个房间
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(rooms.begin(), rooms.end(), gen);

    }
    catch (const std::exception&)
    {
        // 异常时返回空列表
    }

    return rooms;
}

// 保留原来的方法用于向后兼容
std::vector<std::string> Room::getAvailableRooms()
{
    std::vector<std::string> rooms;

    try
    {
        // 1. 优先获取2人房间（最快开始游戏）
        std::vector<std::string> twoPlayerRooms;
        m_redis->smembers(TwoPlayers, std::back_inserter(twoPlayerRooms));
        for (const auto& room : twoPlayerRooms)
        {
            rooms.push_back(room);
        }

        // 2. 然后获取1人房间
        std::vector<std::string> onePlayerRooms;
        m_redis->smembers(OnePlayer, std::back_inserter(onePlayerRooms));
        for (const auto& room : onePlayerRooms)
        {
            rooms.push_back(room);
        }

        // 3. 不再随机打乱整体顺序，保持优先级
        // 只在同类型房间内随机化以避免热点

    }
    catch (const std::exception&)
    {
        // 发生异常时返回空列表，后续会创建新房间
    }

    return rooms;
}
