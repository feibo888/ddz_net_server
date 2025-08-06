#include "GameStateManager.h"
#include <iostream>

GameStateManager* GameStateManager::m_instance = nullptr;
std::mutex GameStateManager::m_mutex;

GameStateManager* GameStateManager::getInstance() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_instance == nullptr) {
        m_instance = new GameStateManager();
    }
    return m_instance;
}

void GameStateManager::startGame(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    auto& state = m_roomStates[roomName];
    state.gameStarted = true;
    state.gameEnded = false;
    state.currentMultiplier = 1;  // 重置倍数
    std::cout << "房间 " << roomName << " 游戏开始" << std::endl;
}

void GameStateManager::setLord(const std::string& roomName, const std::string& lordName, int baseBet) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    auto& state = m_roomStates[roomName];
    state.lordName = lordName;
    state.baseBet = baseBet;
    std::cout << "房间 " << roomName << " 地主：" << lordName << "，底分：" << baseBet << std::endl;
}

void GameStateManager::applyBombMultiplier(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    auto& state = m_roomStates[roomName];
    state.currentMultiplier *= 2;  // 每次炸弹翻倍
    std::cout << "房间 " << roomName << " 炸弹翻倍，当前倍数：" << state.currentMultiplier << std::endl;
}

void GameStateManager::endGame(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    auto& state = m_roomStates[roomName];
    state.gameEnded = true;
    std::cout << "房间 " << roomName << " 游戏结束" << std::endl;
}

bool GameStateManager::isGameActive(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    auto it = m_roomStates.find(roomName);
    return it != m_roomStates.end() && it->second.gameStarted && !it->second.gameEnded;
}

std::string GameStateManager::getLord(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    auto it = m_roomStates.find(roomName);
    return it != m_roomStates.end() ? it->second.lordName : "";
}

int GameStateManager::getBaseBet(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    auto it = m_roomStates.find(roomName);
    return it != m_roomStates.end() ? it->second.baseBet : 0;
}

int GameStateManager::getCurrentMultiplier(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    auto it = m_roomStates.find(roomName);
    return it != m_roomStates.end() ? it->second.currentMultiplier : 1;
}

std::map<std::string, int> GameStateManager::calculateScoreChanges(const std::string& roomName,
                                                                   const std::string& winnerName,
                                                                   const std::vector<std::string>& allPlayers) {
    std::map<std::string, int> scoreChanges;

    std::lock_guard<std::mutex> lock(m_statesMutex);
    auto it = m_roomStates.find(roomName);
    if (it == m_roomStates.end()) {
        std::cout << "警告：房间 " << roomName << " 状态不存在" << std::endl;
        return scoreChanges;
    }

    const auto& state = it->second;
    int finalScore = state.baseBet * state.currentMultiplier;

    // 找出地主和农民
    std::string lordName = state.lordName;
    std::vector<std::string> farmers;

    for (const auto& player : allPlayers) {
        if (player != lordName) {
            farmers.push_back(player);
        }
    }

    if (farmers.size() != 2) {
        std::cout << "警告：农民数量不正确：" << farmers.size() << std::endl;
        return scoreChanges;
    }

    // 计算分数变化
    if (winnerName == lordName) {
        // 地主胜利：地主得分，农民失分
        scoreChanges[lordName] = finalScore * 2;  // 地主得2倍分数
        for (const auto& farmer : farmers) {
            scoreChanges[farmer] = -finalScore;  // 每个农民失1倍分数
        }
        std::cout << "地主 " << lordName << " 胜利，得分：" << scoreChanges[lordName]
                  << "，每个农民失分：" << finalScore << std::endl;
    } else {
        // 农民胜利：农民得分，地主失分
        scoreChanges[lordName] = -finalScore * 2;  // 地主失2倍分数
        for (const auto& farmer : farmers) {
            scoreChanges[farmer] = finalScore;  // 每个农民得1倍分数
        }
        std::cout << "农民胜利，地主 " << lordName << " 失分：" << scoreChanges[lordName]
                  << "，每个农民得分：" << finalScore << std::endl;
    }

    return scoreChanges;
}

void GameStateManager::clearRoom(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    m_roomStates.erase(roomName);
    std::cout << "清理房间 " << roomName << " 状态" << std::endl;
}

GameRoomState* GameStateManager::getRoomState(const std::string& roomName) {
    auto it = m_roomStates.find(roomName);
    return it != m_roomStates.end() ? &it->second : nullptr;
}