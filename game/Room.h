//
// Created by fb on 2025/6/12.
//

#ifndef ROOM_H
#define ROOM_H
#include <string>
#include <sw/redis++/redis++.h>


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

private:
      sw::redis::Redis* m_redis;
      const std::string OnePlayer = "OnePlayer";
      const std::string TwoPlayers = "TwoPlayers";
      const std::string ThreePlayers = "ThreePlayers";
      const std::string Invalid = "Invalid";
};


#endif //ROOM_H
