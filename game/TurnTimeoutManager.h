//
// Created by fb on 2025/8/2.
//

#ifndef TURNTIMEOUTMANAGER_H
#define TURNTIMEOUTMANAGER_H

#include <string>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

class TurnTimeoutManager
{
public:
    static TurnTimeoutManager* getInstance();

    // 开始新回合时调用
    void startPlayerTurn(const std::string& roomName, const std::string& userName, int timeoutSeconds);
    // 玩家出牌时调用
    void finishPlayerTurn(const std::string& roomName, const std::string& userName);
    // 启动/停止超时检测线程
    void startTimeoutChecker();
    void stopTimeoutChecker();
    void clearRoomTimeouts(const std::string& roomName);
;
private:
    TurnTimeoutManager() = default;
    ~TurnTimeoutManager();

    struct TurnInfo {
        std::string roomName;
        std::string userName;
        std::chrono::steady_clock::time_point startTime;
        int timeoutSeconds;
        bool isActive;
    };

    std::unordered_map<std::string, TurnInfo> m_activeTurns;  // key: roomName_userName
    std::mutex m_mutex;
    std::thread m_timeoutThread;
    std::atomic<bool> m_running{false};

    // 超时检测线程主函数
    void timeoutCheckerThread();
    // 检查并处理超时
    void checkTimeouts();
    // 处理单个超时事件
    void handleTimeout(const std::string& roomName, const std::string& userName);
    // 检查并处理下一个玩家
    void checkAndHandleNextPlayer(const std::string& roomName, const std::string& currentPlayer);
    // 获取下一个玩家(辅助方法)
    std::string getNextPlayer(const std::string& roomName, const std::string& currentPlayer);

    static constexpr int CHECK_INTERVAL_MS = 1000;  // 每秒检查一次

};



#endif //TURNTIMEOUTMANAGER_H
