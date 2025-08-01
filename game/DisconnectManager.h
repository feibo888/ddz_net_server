//
// Created by fb on 2025/7/31.
//

#ifndef DISCONNECTMANAGER_H
#define DISCONNECTMANAGER_H


#include <string>
#include <set>
#include <mutex>
#include <memory>
#include <vector>
#include <iostream>

class Room;

class DisconnectManager {
public:
    static DisconnectManager* getInstance();

    // 玩家断线时调用
    //void handlePlayerDisconnect(const std::string& roomName, const std::string& userName);
    // 玩家断线时调用（仅记录断线状态）
    void recordPlayerDisconnect(const std::string& roomName, const std::string& userName);

    // 检查是否为断线玩家
    bool isDisconnectedPlayer(const std::string& roomName, const std::string& userName);

    // 当轮到断线玩家出牌时调用
    //void handleDisconnectedPlayerTurn(const std::string& roomName, const std::string& userName);
    // 关键方法：每次有玩家出牌后调用，检查下一个玩家是否断线
    void checkAndHandleNextPlayer(const std::string& roomName, const std::string& nextPlayer);

    // 游戏结束时清理
    void cleanupGame(const std::string& roomName);


    // **新增：独立Redis连接管理**
    void initRedisConnection();
    void cleanupRedisConnection();

    // 设置Redis连接
    void setRedisConnection(Room* redis) { std::cout << "警告：setRedisConnection已废弃，DisconnectManager使用独立连接" << std::endl; }

    std::string getNextPlayer(const std::string& roomName, const std::string& currentPlayer);

    // 执行AI出牌逻辑（基于Redis中的真实手牌数据）
    void executeAIPlay(const std::string& roomName, const std::string& userName);
private:
    DisconnectManager() = default;
    ~DisconnectManager();


    // 构造并发送AI出牌消息
    void sendAIPlayMessage(const std::string& roomName, const std::string& userName, const std::string& playData);

    // 辅助方法
    std::pair<int, int> getSmallestCard(const std::vector<std::pair<int, int>>& cards);
    std::vector<std::pair<int, int>> parseCardData(const std::string& cardData);
    bool isFirstHand(const std::string& roomName, const std::string& userName);
    std::string formatCard(const std::pair<int, int>& card);


    std::set<std::string> m_disconnectedPlayers; // 格式："roomName_userName"
    std::mutex m_mutex;
    Room* m_redis = nullptr;
    bool m_redisInitialized = false;
};




#endif //DISCONNECTMANAGER_H
