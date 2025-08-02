//
// Created by fb on 2025/8/2.
//

#include "TurnTimeoutManager.h"
#include "DisconnectManager.h"
#include <iostream>


TurnTimeoutManager* TurnTimeoutManager::getInstance() {
    static TurnTimeoutManager instance;
    return &instance;
}

TurnTimeoutManager::~TurnTimeoutManager() {
    stopTimeoutChecker();
}

void TurnTimeoutManager::startPlayerTurn(const std::string& roomName, const std::string& userName, int timeoutSeconds) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string key = roomName + "_" + userName;

    TurnInfo turnInfo;
    turnInfo.roomName = roomName;
    turnInfo.userName = userName;
    turnInfo.startTime = std::chrono::steady_clock::now();
    turnInfo.timeoutSeconds = timeoutSeconds;
    turnInfo.isActive = true;

    m_activeTurns[key] = turnInfo;

    std::cout << "开始计时：玩家 " << userName << " 房间 " << roomName
              << " 超时时间 " << timeoutSeconds << "秒" << std::endl;
}

void TurnTimeoutManager::finishPlayerTurn(const std::string& roomName, const std::string& userName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string key = roomName + "_" + userName;
    auto it = m_activeTurns.find(key);
    if (it != m_activeTurns.end()) {
        m_activeTurns.erase(it);
        std::cout << "取消计时：玩家 " << userName << " 房间 " << roomName << std::endl;
    }
}

void TurnTimeoutManager::startTimeoutChecker() {
    if (m_running.load()) {
        return;
    }

    m_running.store(true);
    m_timeoutThread = std::thread(&TurnTimeoutManager::timeoutCheckerThread, this);
    std::cout << "出牌超时检测器已启动" << std::endl;
}

void TurnTimeoutManager::stopTimeoutChecker() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);
    if (m_timeoutThread.joinable()) {
        m_timeoutThread.join();
    }
    std::cout << "出牌超时检测器已停止" << std::endl;
}

void TurnTimeoutManager::timeoutCheckerThread() {
    while (m_running.load()) {
        try {
            checkTimeouts();

            // 每秒检查一次
            std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));

        } catch (const std::exception& e) {
            std::cout << "出牌超时检测异常：" << e.what() << std::endl;
        }
    }
}

void TurnTimeoutManager::checkTimeouts() {
    std::vector<std::pair<std::string, std::string>> timeoutPlayers;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();

        for (auto& pair : m_activeTurns) {
            const auto& turnInfo = pair.second;
            if (!turnInfo.isActive) continue;

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - turnInfo.startTime);

            if (elapsed.count() >= turnInfo.timeoutSeconds) {
                timeoutPlayers.push_back({turnInfo.roomName, turnInfo.userName});
                std::cout << "检测到超时：玩家 " << turnInfo.userName
                          << " 房间 " << turnInfo.roomName
                          << " 已超时 " << elapsed.count() << "秒" << std::endl;
            }
        }
    }

    // 处理超时玩家（在锁外处理，避免死锁）
    for (const auto& player : timeoutPlayers) {
        handleTimeout(player.first, player.second);
    }
}

void TurnTimeoutManager::handleTimeout(const std::string& roomName, const std::string& userName) {
    std::cout << "处理超时：房间[" << roomName << "] 玩家[" << userName << "]" << std::endl;
    
    // 1. 先移除超时记录
    finishPlayerTurn(roomName, userName);

    // 2. 标记玩家为断线
    DisconnectManager::getInstance()->recordPlayerDisconnect(roomName, userName);

    // 3. 执行AI托管出牌
    DisconnectManager::getInstance()->executeAIPlay(roomName, userName);

    // 4. 检查下一个玩家并设置适当的超时或继续AI处理
    checkAndHandleNextPlayer(roomName, userName);
}

void TurnTimeoutManager::checkAndHandleNextPlayer(const std::string& roomName, const std::string& currentPlayer) {
    DisconnectManager* disconnectMgr = DisconnectManager::getInstance();
    
    // 获取下一个玩家
    std::string nextPlayer = disconnectMgr->getNextPlayer(roomName, currentPlayer);
    
    if (nextPlayer.empty()) {
        std::cout << "超时处理：无法确定下一个玩家" << std::endl;
        return;
    }
    
    if (disconnectMgr->isDisconnectedPlayer(roomName, nextPlayer)) {
        std::cout << "超时处理：下一个玩家 " << nextPlayer << " 也断线，设置其超时检测(快速超时)" << std::endl;
        // 为断线玩家设置很短的超时时间(5秒)，让超时机制快速处理
        startPlayerTurn(roomName, nextPlayer, 5);
    } else {
        std::cout << "超时处理：下一个玩家 " << nextPlayer << " 在线，设置正常超时检测" << std::endl;
        // 为在线玩家设置正常超时时间
        startPlayerTurn(roomName, nextPlayer, 30);
    }
}

// 移除不需要的方法
std::string TurnTimeoutManager::getNextPlayer(const std::string& roomName, const std::string& currentPlayer) {
    return DisconnectManager::getInstance()->getNextPlayer(roomName, currentPlayer);
}

void TurnTimeoutManager::clearRoomTimeouts(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_activeTurns.begin();
    while (it != m_activeTurns.end()) {
        if (it->second.roomName == roomName) {
            std::cout << "清理房间 " << roomName << " 玩家 " << it->second.userName << " 的超时检测" << std::endl;
            it = m_activeTurns.erase(it);
        } else {
            ++it;
        }
    }
}

