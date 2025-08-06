#include "BombDetector.h"
#include <map>
#include <iostream>
#include <sstream>

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
        if (card.point == 14) {  // 小王
            hasSmallJoker = true;
        } else if (card.point == 15) {  // 大王
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

    // 假设卡牌数据格式为 "point1-suit1#point2-suit2#..."
    std::istringstream iss(cardData);
    std::string cardStr;

    while (std::getline(iss, cardStr, '#')) {
        if (cardStr.empty()) continue;

        size_t dashPos = cardStr.find('-');
        if (dashPos != std::string::npos) {
            try {
                int point = std::stoi(cardStr.substr(0, dashPos));
                int suit = std::stoi(cardStr.substr(dashPos + 1));
                cards.emplace_back(point, suit);
            } catch (const std::exception& e) {
                std::cout << "解析卡牌数据失败: " << cardStr << std::endl;
            }
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