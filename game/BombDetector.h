#ifndef BOMBDETECTOR_H
#define BOMBDETECTOR_H

#include <vector>
#include <string>
#include <map>

// 简化的牌结构
struct Card {
    int point;    // 牌点数 (3-15, 其中14=小王, 15=大王)
    int suit;     // 花色 (0-3, 对于王牌可以忽略)

    Card(int p = 0, int s = 0) : point(p), suit(s) {}
};

// 炸弹检测器
class BombDetector {
public:
    BombDetector() = default;

    // 检测是否为炸弹牌型
    bool detectBomb(const std::vector<Card>& cards);

    // 检测王炸（大王+小王）
    bool isJokerBomb(const std::vector<Card>& cards);

    // 检测普通炸弹（4张相同点数）
    bool isNormalBomb(const std::vector<Card>& cards);

    // 从字符串解析牌数据（用于网络传输的牌数据）
    std::vector<Card> parseCards(const std::string& cardData);

private:
    // 统计每个点数的牌数
    std::map<int, int> countCardPoints(const std::vector<Card>& cards);
};

#endif // BOMBDETECTOR_H