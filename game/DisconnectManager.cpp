//
// Created by fb on 2025/7/31.
//

#include "DisconnectManager.h"
#include "Room.h"
#include "RoomList.h"
#include "../serialize/Information.pb.h"
#include "../serialize/Codec.h"
#include <sstream>
#include <algorithm>
#include <Communication.h>
#include <iostream>
#include <cstring>    // for memcpy
#include <arpa/inet.h>

DisconnectManager* DisconnectManager::getInstance() {
    static DisconnectManager instance;
    return &instance;
}

DisconnectManager::~DisconnectManager() {
    cleanupRedisConnection();
}


void DisconnectManager::recordPlayerDisconnect(const std::string& roomName, const std::string& userName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string key = roomName + "_" + userName;
    m_disconnectedPlayers.insert(key);

    std::cout << "记录断线玩家：" << userName << " 房间：" << roomName << std::endl;
}


bool DisconnectManager::isDisconnectedPlayer(const std::string& roomName, const std::string& userName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = roomName + "_" + userName;
    return m_disconnectedPlayers.find(key) != m_disconnectedPlayers.end();
}


void DisconnectManager::checkAndHandleNextPlayer(const std::string& roomName, const std::string& nextPlayer) {
    if (isDisconnectedPlayer(roomName, nextPlayer)) {
        std::cout << "检测到轮到断线玩家 " << nextPlayer << " 出牌，执行AI托管" << std::endl;
        executeAIPlay(roomName, nextPlayer);
    }else {
        std::cout << "玩家 " << nextPlayer << " 在线，等待其出牌" << std::endl;
    }
}


void DisconnectManager::cleanupGame(const std::string& roomName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 移除该房间的所有断线记录
    auto it = m_disconnectedPlayers.begin();
    while (it != m_disconnectedPlayers.end()) {
        if (it->find(roomName + "_") == 0) {
            it = m_disconnectedPlayers.erase(it);
        } else {
            ++it;
        }
    }

    std::cout << "清理房间 " << roomName << " 的断线记录" << std::endl;
}

void DisconnectManager::executeAIPlay(const std::string& roomName, const std::string& userName) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_redisInitialized || !m_redis) {
            std::cout << "DisconnectManager Redis连接未就绪，重新初始化" << std::endl;
            initRedisConnection();
        }

        if (!m_redis) {
            std::cout << "DisconnectManager Redis连接初始化失败，无法执行AI出牌" << std::endl;
            return;
        }
    }

    try {
        // 从Redis获取玩家真实手牌
        std::string cardData = m_redis->getPlayerCards(roomName, userName);
        if (cardData.empty()) {
            std::cout << "无法获取断线玩家 " << userName << " 的手牌" << std::endl;
            return;
        }

        // 解析手牌
        std::vector<std::pair<int, int>> handCards = parseCardData(cardData);
        if (handCards.empty()) {
            std::cout << "断线玩家 " << userName << " 手牌为空，游戏可能已结束" << std::endl;
            return;
        }

        std::cout << "断线玩家 " << userName << " 当前手牌：" << cardData << std::endl;

        // 简单AI决策
        std::string playData;

        if (isFirstHand(roomName, userName)) {
            // 第一手出牌：出最小的牌
            std::pair<int, int> playedCard = getSmallestCard(handCards);
            playData = formatCard(playedCard);

            // 从Redis中移除这张牌
            m_redis->removeCardFromPlayer(roomName, userName, playData);

            // **关键修复：AI出牌后更新游戏控制权和控牌数据**
            m_redis->setGameState(roomName, "game_controller", userName);
            
            // **新增：保存AI出牌的控牌数据（与客户端格式一致）**
            // 构造与客户端相同的二进制格式数据
            std::string binaryCardData;
            binaryCardData.resize(2 * sizeof(int));
            
            // 写入suit（转换为网络字节序）
            int networkSuit = htonl(playedCard.first);
            memcpy(&binaryCardData[0], &networkSuit, sizeof(int));
            
            // 写入point（转换为网络字节序） 
            int networkPoint = htonl(playedCard.second);
            memcpy(&binaryCardData[sizeof(int)], &networkPoint, sizeof(int));
            
            // 保存控牌数据和数量
            m_redis->setGameState(roomName, "pend_cards", binaryCardData);
            m_redis->setGameState(roomName, "pend_count", "1");
            
            std::cout << "AI代替断线玩家 " << userName << " 出牌（第一手）：" << playData 
                      << " 已保存控牌数据" << std::endl;

            if (Communication::checkGameEndAndHandle(roomName, userName)) {
                std::cout << "AI出牌触发游戏结束，完整流程已处理" << std::endl;
                return; // 游戏已结束，不需要继续处理下一个玩家
            }

        } else {
            // 跟牌：直接过牌
            playData = "";
            std::cout << "AI代替断线玩家 " << userName << " 过牌（不更新控牌数据）" << std::endl;
            // 过牌时不改变game_controller和控牌数据
        }

        // 发送AI出牌消息
        sendAIPlayMessage(roomName, userName, playData);

        // 检查游戏是否结束（手牌出完）
        if (!playData.empty()) {
            std::string remainingCards = m_redis->getPlayerCards(roomName, userName);
            if (remainingCards.empty() || remainingCards == "#") {
                std::cout << "断线玩家 " << userName << " AI出完所有手牌，游戏结束！" << std::endl;
            }
        }

        // 更新到下一个玩家
        std::string nextPlayer = getNextPlayer(roomName, userName);
        if (m_redis) {
            m_redis->setGameState(roomName, "current_turn", nextPlayer);
        }

        // **关键修改：移除递归调用，仅记录下一个玩家状态**
        std::cout << "AI出牌完成，下一个玩家：" << nextPlayer;
        if (!nextPlayer.empty() && isDisconnectedPlayer(roomName, nextPlayer)) {
            std::cout << "（也已断线，将由外层处理）";
        }
        std::cout << std::endl;

    } catch (const std::exception& e) {
        std::cout << "断线AI出牌失败：" << e.what() << std::endl;
    }
}

void DisconnectManager::sendAIPlayMessage(const std::string& roomName, const std::string& userName, const std::string& playData) {
    // 构造消息并通知其他在线玩家
    Message aiMsg;
    aiMsg.resCode = ResponseCode::OtherPlayHand;
    aiMsg.roomName = roomName;
    aiMsg.userName = userName;

    if (playData.empty()) {
        // 过牌
        aiMsg.data1 = "0";  // 牌数量为0
        aiMsg.data2 = "";   // 无牌数据
    } else {
        // 出牌：构造符合客户端期望的格式
        aiMsg.data1 = "1";  // 出1张牌

        // 解析playData格式："suit-point"
        size_t dashPos = playData.find('-');
        if (dashPos != std::string::npos) {
            int suit = std::stoi(playData.substr(0, dashPos));
            int point = std::stoi(playData.substr(dashPos + 1));

            // 构造QDataStream格式的data2（大端序）
            std::string cardData;
            cardData.resize(2 * sizeof(int));

            // 写入suit（转换为网络字节序）
            int networkSuit = htonl(suit);
            memcpy(&cardData[0], &networkSuit, sizeof(int));

            // 写入point（转换为网络字节序）
            int networkPoint = htonl(point);
            memcpy(&cardData[sizeof(int)], &networkPoint, sizeof(int));

            aiMsg.data2 = cardData;
        } else {
            // 格式错误，发送过牌
            aiMsg.data1 = "0";
            aiMsg.data2 = "";
        }
    }

    Codec codec(&aiMsg);
    std::string encodedMsg = codec.enCodeMsg();

    // 通知房间内其他在线玩家
    auto players = RoomList::getInstance()->getPlayers(roomName);

    std::cout << "AI出牌消息 - 房间：" << roomName << " 断线玩家：" << userName << std::endl;
    std::cout << "当前房间在线玩家数量：" << players.size() << std::endl;

    for (const auto& player : players) {
        std::cout << "检查玩家：" << player.first << " 是否需要通知" << std::endl;

        // **重要：只通知在线玩家，跳过断线玩家（包括AI玩家自己和其他断线玩家）**
        if (player.first != userName && !isDisconnectedPlayer(roomName, player.first)) {
            player.second(encodedMsg);
            std::cout << "已通知在线玩家：" << player.first << " AI玩家 " << userName << " 的出牌" << std::endl;
        } else {
            if (player.first == userName) {
                std::cout << "跳过断线玩家：" << userName << " 自己，避免消息混乱" << std::endl;
            } else {
                std::cout << "跳过断线玩家：" << player.first << " 避免发送超时" << std::endl;
            }
        }
    }
}

std::pair<int, int> DisconnectManager::getSmallestCard(const std::vector<std::pair<int, int>>& cards) {
    if (cards.empty()) {
        return std::make_pair(0, 0);
    }

    // 找到最小的牌（按点数排序，花色为辅）
    auto minCard = *std::min_element(cards.begin(), cards.end(),
        [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            if (a.second != b.second) {
                return a.second < b.second;  // 按点数排序
            }
            return a.first < b.first;  // 点数相同按花色排序
        });

    return minCard;
}

std::vector<std::pair<int, int>> DisconnectManager::parseCardData(const std::string& cardData) {
    std::vector<std::pair<int, int>> cards;

    if (cardData.empty()) {
        return cards;
    }

    // 解析格式："1-2#3-4#5-6#"
    std::stringstream ss(cardData);
    std::string cardStr;

    while (std::getline(ss, cardStr, '#')) {
        if (cardStr.empty()) continue;

        size_t dashPos = cardStr.find('-');
        if (dashPos != std::string::npos) {
            try {
                int suit = std::stoi(cardStr.substr(0, dashPos));
                int rank = std::stoi(cardStr.substr(dashPos + 1));
                cards.push_back(std::make_pair(suit, rank));
            } catch (const std::exception& e) {
                std::cout << "解析卡牌数据失败: " << cardStr << std::endl;
            }
        }
    }

    return cards;
}

bool DisconnectManager::isFirstHand(const std::string& roomName, const std::string& userName) {
    if (!m_redis) {
        return false;
    }

    // 检查当前控制权玩家
    std::string controller = m_redis->getGameState(roomName, "game_controller");

    // 修正逻辑：
    // 1. 没有控制权玩家（游戏开始时） = 先手出牌
    // 2. 控制权玩家就是当前玩家 = 先手出牌  
    // 3. 控制权玩家是其他人 = 跟牌
    if (controller.empty()) 
    {
        return true;
    } 
    else if (controller == userName) 
    {
        return true;
    } 
    else 
    {
        return false;
    }
}

std::string DisconnectManager::formatCard(const std::pair<int, int>& card) {
    return std::to_string(card.first) + "-" + std::to_string(card.second);
}

std::string DisconnectManager::getNextPlayer(const std::string& roomName, const std::string& currentPlayer) {

    if (!m_redis) {
        return "";
    }

    // 获取Redis中按分数排序的玩家顺序
    std::string orderData = m_redis->getGamePlayerOrder(roomName);
    if (orderData.empty()) {
        std::cout << "警告：无法获取房间 " << roomName << " 的玩家顺序数据" << std::endl;
        return "";
    }

    // 解析玩家顺序："playerA-55#playerC-43#playerB-34#"
    std::vector<std::string> orderedPlayers;
    std::stringstream ss(orderData);
    std::string playerEntry;

    while (std::getline(ss, playerEntry, '#')) {
        if (playerEntry.empty()) continue;

        size_t dashPos = playerEntry.find('-');
        if (dashPos != std::string::npos) {
            std::string playerName = playerEntry.substr(0, dashPos);
            orderedPlayers.push_back(playerName);
        }
    }

    if (orderedPlayers.size() != 3) {
        std::cout << "警告：房间 " << roomName << " 玩家数量异常：" << orderedPlayers.size() << std::endl;
        return "";
    }

    std::cout << "房间 " << roomName << " 玩家顺序：";
    for (const auto& player : orderedPlayers) {
        std::cout << player << " ";
    }
    std::cout << std::endl;

    // 找到当前玩家的位置，返回下一个玩家
    for (int i = 0; i < 3; ++i) {
        if (orderedPlayers[i] == currentPlayer) {
            std::string nextPlayer = orderedPlayers[(i + 1) % 3];
            std::cout << "当前玩家：" << currentPlayer << " -> 下一个玩家：" << nextPlayer << std::endl;
            return nextPlayer;
            //return orderedPlayers[(i + 1) % 3];
        }
    }

    std::cout << "警告：在玩家顺序中未找到当前玩家：" << currentPlayer << std::endl;
    return orderedPlayers[0]; // 默认返回第一个
}


void DisconnectManager::initRedisConnection() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_redisInitialized && m_redis) {
        std::cout << "DisconnectManager Redis连接已初始化" << std::endl;
        return;
    }

    try {
        m_redis = new Room();
        if (m_redis && m_redis->initEnvironment()) {  // 关键修复：必须调用initEnvironment()
            std::cout << "DisconnectManager 成功创建独立Redis连接" << std::endl;
            m_redisInitialized = true;
        } else {
            std::cout << "DisconnectManager Redis连接初始化失败" << std::endl;
            if (m_redis) {
                delete m_redis;
                m_redis = nullptr;
            }
            m_redisInitialized = false;
        }
    } catch (const std::exception& e) {
        std::cout << "DisconnectManager Redis连接初始化异常：" << e.what() << std::endl;
        if (m_redis) {
            delete m_redis;
            m_redis = nullptr;
        }
        m_redisInitialized = false;
    }
}


void DisconnectManager::cleanupRedisConnection() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_redis) {
        try {
            delete m_redis;
            //std::cout << "DisconnectManager Redis连接已清理" << std::endl;
        } catch (const std::exception& e) {
            //std::cout << "DisconnectManager Redis连接清理异常：" << e.what() << std::endl;
        }
        m_redis = nullptr;
        m_redisInitialized = false;
    }
}

void DisconnectManager::removePlayerFromDisconnectList(const std::string& roomName, const std::string& userName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string key = roomName + "_" + userName;
    auto erased = m_disconnectedPlayers.erase(key);

    if (erased > 0) {
        std::cout << "从断线列表移除玩家：" << userName << " 房间：" << roomName << std::endl;
    }
}