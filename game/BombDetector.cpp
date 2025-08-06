#include "BombDetector.h"
#include <map>
#include <iostream>
#include <sstream>
#include <cstring>  // for memcpy
#include <arpa/inet.h>  // for ntohl
#include <cstdio>  // for printf

bool BombDetector::detectBomb(const std::vector<Card>& cards) {
    if (cards.empty()) {
        return false;
    }

    // 检测王炸
    if (isJokerBomb(cards)) {
        std::cout << "检测到王炸！" << std::endl;
        return true;
    }

    // 检测普通炸弹
    if (isNormalBomb(cards)) {
        std::cout << "检测到普通炸弹！" << std::endl;
        return true;
    }

    return false;
}

bool BombDetector::isJokerBomb(const std::vector<Card>& cards) {
    if (cards.size() != 2) {
        return false;
    }

    bool hasSmallJoker = false;
    bool hasBigJoker = false;

    for (const auto& card : cards) {
        if (card.point == 14) {  // 小王 (客户端 Card_SJ=14)
            hasSmallJoker = true;
        } else if (card.point == 15) {  // 大王 (客户端 Card_BJ=15)
            hasBigJoker = true;
        }
    }

    return hasSmallJoker && hasBigJoker;
}

bool BombDetector::isNormalBomb(const std::vector<Card>& cards) {
    if (cards.size() != 4) {
        return false;
    }

    auto pointCounts = countCardPoints(cards);

    // 普通炸弹：4张相同点数的牌
    for (const auto& pair : pointCounts) {
        if (pair.second == 4 && pair.first <= 13) {  // 排除王牌
            return true;
        }
    }

    return false;
}

std::vector<Card> BombDetector::parseCards(const std::string& cardData) {
    std::vector<Card> cards;
    
    if (cardData.empty()) {
        return cards;
    }
    
    // 参考Communication.cpp中的正确解析逻辑
    // 客户端使用QDataStream序列化卡牌数据，大端序格式
    // 每张牌包含两个int：suit和point
    const char* data = cardData.c_str();
    int dataSize = cardData.size();
    int cardCount = dataSize / (2 * sizeof(int)); // 每张牌占用2个int的空间
    
    std::cout << "BombDetector解析出牌数据，数据大小：" << dataSize << "，卡牌数量：" << cardCount << std::endl;
    
    // 解析每张牌
    for (int i = 0; i < cardCount; ++i) {
        int offset = i * 2 * sizeof(int);
        if (offset + 2 * sizeof(int) <= dataSize) {
            // 读取 suit 和 point (按QDataStream的大端序格式)
            int suit = ntohl(*reinterpret_cast<const int*>(data + offset));
            int point = ntohl(*reinterpret_cast<const int*>(data + offset + sizeof(int)));
            
            cards.emplace_back(point, suit);
            std::cout << "BombDetector解析到卡牌：point=" << point << ", suit=" << suit << std::endl;
        }
    }
    
    return cards;
}

std::map<int, int> BombDetector::countCardPoints(const std::vector<Card>& cards) {
    std::map<int, int> pointCounts;

    for (const auto& card : cards) {
        pointCounts[card.point]++;
    }

    return pointCounts;
}