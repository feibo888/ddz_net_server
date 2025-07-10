//
// Created by fb on 2025/6/12.
//

#ifndef ROOM_H
#define ROOM_H
#include <string>
#include <sw/redis++/redis++.h>
#include <unordered_map>


class Room
{
public:
      Room() = default;
      ~Room();

      //初始化 连接redis, redis++
      bool initEnvironment();
      //清空数据
      void clear();
      //保存RSA密钥对
      //key(field, value)
      //hset
      void saveRsaSecKey(std::string field, std::string value);
      //读密钥
      //hget
      std::string getRsaSecKey(std::string field);

      //加入房间
      std::string joinRoom(std::string userName);
      bool joinRoom(std::string userName, std::string roomName);
      //随机生成房间的名字
      std::string getRoomName();
      //得到房间内玩家的数量
      int getPlayerCount(std::string roomName);
      //更新玩家分数
      void UpdatePlayerScore(std::string roomName, std::string userName, int score);
      //查找玩家所在房间
      std::string whereAmI(std::string userName);
      int getPlayerScore(std::string roomName, std::string userName);
      //得到抢地主的次序
      std::string getPlayerOrder(std::string roomName);
      //离开房间
      void leaveRoom(std::string roomName, std::string userName);
      //搜索房间
      bool searchRoom(std::string roomName);

      // 改进：更安全的分布式锁方法
      bool acquireLock(const std::string& lockKey, int expireSeconds = 10);
      void releaseLock(const std::string& lockKey);
      // 新增：生成唯一标识符
      std::string generateLockValue();

      // 新增：无锁房间匹配算法
      std::string joinRoomWithoutLock(std::string userName);
      std::vector<std::string> getAvailableRooms();

      // 新增：分离的房间获取方法，实现真正的优先级匹配
      std::vector<std::string> getTwoPlayerRooms();
      std::vector<std::string> getOnePlayerRooms();

      // 新增：Redis集合操作方法，用于继续游戏状态管理
      int scard(const std::string& key) { return m_redis->scard(key); }
      void sadd(const std::string& key, const std::string& member) { m_redis->sadd(key, member); }
      void del(const std::string& key) { m_redis->del(key); }
      void hset(const std::string& key, const std::string& field, const std::string& value) { m_redis->hset(key, field, value); }

private:
      sw::redis::Redis* m_redis;
      const std::string OnePlayer = "OnePlayer";
      const std::string TwoPlayers = "TwoPlayers";
      const std::string ThreePlayers = "ThreePlayers";
      const std::string Invalid = "Invalid";

      // 新增：存储当前进程的锁值，用于安全释放
      std::unordered_map<std::string, std::string> m_lockValues;
};


#endif //ROOM_H
