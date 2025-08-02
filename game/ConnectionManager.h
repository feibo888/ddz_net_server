//
// Created by fb on 2025/8/1.
//

#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H


#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include "Communication.h"

class Communication;

struct ConnectionInfo {
    std::string userName;
    std::string roomName;
    std::chrono::steady_clock::time_point lastHeartbeat;
    Communication* comm;  // 关联的Communication对象
    bool isActive;
};

class ConnectionManager {
public:
    static ConnectionManager* getInstance();

    // 连接管理
    void registerConnection(const std::string& connectionId,
                          const std::string& userName,
                          const std::string& roomName,
                          Communication* comm);
    void unregisterConnection(const std::string& connectionId);

    // 心跳管理
    void updateHeartbeat(const std::string& connectionId);
    void startHeartbeatChecker();
    void stopHeartbeatChecker();

    // 超时检测
    std::vector<std::string> getTimeoutConnections();
    void handleTimeoutConnection(const std::string& connectionId);

private:
    ConnectionManager() = default;
    ~ConnectionManager();

    void heartbeatCheckerThread();

    std::unordered_map<std::string, ConnectionInfo> m_connections;
    std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    std::thread m_checkerThread;

    // 配置参数
    static const int HEARTBEAT_TIMEOUT_SECONDS = 30;  // 90秒超时
    static const int CHECK_INTERVAL_SECONDS = 10;     // 10秒检查一次
};




#endif //CONNECTIONMANAGER_H
