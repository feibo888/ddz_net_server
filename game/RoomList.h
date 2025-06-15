//
// Created by fb on 2025/6/12.
//

#ifndef ROOMLIST_H
#define ROOMLIST_H
#include <map>
#include <string>
#include <functional>
#include <mutex>

using callback = std::function<void(std::string)>;
using userMap = std::map<std::string, callback>;

class RoomList
{
public:
    RoomList(const RoomList &) = delete;
    RoomList &operator=(const RoomList &) = delete;
    static RoomList* getInstance();

    //添加用户
    void addUser(std::string roomName, std::string userName, callback sendMessage);

    //获取房间用户的信息
    userMap getPlayers(std::string roomName);
    //得到当前房间除了指定玩家外的其他玩家
    userMap getPartners(std::string roomName, std::string userName);
    //删除指定房间中的玩家
    void removePlayer(std::string roomName, std::string userName);
    //清空房间中的所有玩家
    void removeRoom(std::string roomName);

private:
    RoomList() = default;
    std::map<std::string, userMap> m_roomMap;
    std::mutex m_mutex;

};



#endif //ROOMLIST_H
