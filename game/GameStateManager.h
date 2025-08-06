#ifndef GAMESTATEMANAGER_H
#define GAMESTATEMANAGER_H

#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <vector>

// 单局游戏状态
struct GameRoomState {
    std::string lordName;           // 地主名字
    int baseBet;                   // 底分
    int currentMultiplier;         // 当前倍数（炸弹翻倍）
    bool gameStarted;              // 游戏是否开始
    bool gameEnded;                // 游戏是否结束

    GameRoomState() : baseBet(0), currentMultiplier(1), gameStarted(false), gameEnded(false) {}
};

// 游戏状态管理器（单例模式）
class GameStateManager {
public:
    static GameStateManager* getInstance();

    // 游戏状态管理
    void startGame(const std::string& roomName);
    void setLord(const std::string& roomName, const std::string& lordName, int baseBet);
    void applyBombMultiplier(const std::string& roomName);
    void endGame(const std::string& roomName);

    // 获取游戏状态
    bool isGameActive(const std::string& roomName);
    std::string getLord(const std::string& roomName);
    int getBaseBet(const std::string& roomName);
    int getCurrentMultiplier(const std::string& roomName);

    // 分数计算
    std::map<std::string, int> calculateScoreChanges(const std::string& roomName, const std::string& winnerName, const std::vector<std::string>& allPlayers);

    // 清理房间状态
    void clearRoom(const std::string& roomName);

private:
    GameStateManager() = default;
    static GameStateManager* m_instance;
    static std::mutex m_mutex;

    std::map<std::string, GameRoomState> m_roomStates;
    std::mutex m_statesMutex;

    GameRoomState* getRoomState(const std::string& roomName);
};

#endif // GAMESTATEMANAGER_H