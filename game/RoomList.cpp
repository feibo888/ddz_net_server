//
// Created by fb on 2025/6/12.
//

#include "RoomList.h"

#include <iostream>
#include <ostream>

RoomList * RoomList::getInstance()
{
    static RoomList instance;
    return &instance;
}

void RoomList::addUser(std::string roomName, std::string userName, callback sendMessage)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    //在当前map搜索roomName
    if (m_roomMap.find(roomName) != m_roomMap.end())
    {
        //找到了
        auto& value = m_roomMap[roomName];
        value.insert(make_pair(userName, sendMessage));
    }
    else
    {
        //没找到
        userMap value = {{userName, sendMessage}};
        m_roomMap[roomName] = value;
    }
}

userMap RoomList::getPlayers(std::string roomName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_roomMap.find(roomName);
    if (it == m_roomMap.end())
    {
        return userMap();
    }
    userMap players = it->second;
    return players;
}

userMap RoomList::getPartners(std::string roomName, std::string userName)
{
    auto players = getPlayers(roomName);
    if (players.size() > 1)
    {
        auto self = players.find(userName);
        if (self != players.end())
        {
            players.erase(self);
            return players;
        }
    }
    return userMap();
}

void RoomList::removePlayer(std::string roomName, std::string userName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto item = m_roomMap.find(roomName);
    if (item != m_roomMap.end())
    {
        //找人
        // auto player = item->second;
        // auto it = player.find(userName);
        // if (it != player.end() && player.size() > 1)
        // {
        //     std::cout << "Removing player " << userName << " from room " << roomName << std::endl;
        //     m_roomMap[roomName].erase(it);
        //     std::cout << "remove over" << std::endl;
        // }
        auto& player = item->second;  // 使用引用而非拷贝
        auto it = player.find(userName);
        if (it != player.end() && player.size() > 1)
        {
            player.erase(it);  // 在同一个容器上使用迭代器
        }
        else if (it != player.end() && player.size() == 1)
        {
            m_roomMap.erase(roomName);
        }
    }
}

void RoomList::removeRoom(std::string roomName)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto item = m_roomMap.find(roomName);
    if (item != m_roomMap.end())
    {
        m_roomMap.erase(roomName);
    }
}
