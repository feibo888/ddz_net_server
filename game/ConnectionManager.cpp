//
// Created by fb on 2025/8/1.
//

#include "ConnectionManager.h"
#include "DisconnectManager.h"
#include <iostream>

// 静态成员变量定义
const int ConnectionManager::HEARTBEAT_TIMEOUT_SECONDS;
const int ConnectionManager::CHECK_INTERVAL_SECONDS;

ConnectionManager* ConnectionManager::getInstance() {
    static ConnectionManager instance;
    return &instance;
}

ConnectionManager::~ConnectionManager() {
    stopHeartbeatChecker();
}

void ConnectionManager::registerConnection(const std::string& connectionId,
                                         const std::string& userName,
                                         const std::string& roomName,
                                         Communication* comm) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ConnectionInfo info;
    info.userName = userName;
    info.roomName = roomName;
    info.lastHeartbeat = std::chrono::steady_clock::now();
    info.comm = comm;
    info.isActive = true;

    m_connections[connectionId] = info;
    std::cout << "注册连接：" << connectionId << " 用户：" << userName
              << " 房间：" << roomName << std::endl;
}

void ConnectionManager::unregisterConnection(const std::string& connectionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_connections.find(connectionId);
    if (it != m_connections.end()) {
        std::cout << "注销连接：" << connectionId << " 用户："
                  << it->second.userName << std::endl;
        m_connections.erase(it);
    }
}

void ConnectionManager::updateHeartbeat(const std::string& connectionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_connections.find(connectionId);
    if (it != m_connections.end()) {
        it->second.lastHeartbeat = std::chrono::steady_clock::now();
        // 可选：输出心跳日志（调试用）
        // std::cout << "更新心跳：" << connectionId << std::endl;
    }
}

void ConnectionManager::startHeartbeatChecker() {
    if (m_running.load()) {
        return;
    }

    m_running.store(true);
    m_checkerThread = std::thread(&ConnectionManager::heartbeatCheckerThread, this);
    std::cout << "心跳检测器已启动" << std::endl;
}

void ConnectionManager::stopHeartbeatChecker() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);
    if (m_checkerThread.joinable()) {
        m_checkerThread.join();
    }
    std::cout << "心跳检测器已停止" << std::endl;
}

void ConnectionManager::heartbeatCheckerThread() {
    while (m_running.load()) {
        try {
            // 检查超时连接
            std::vector<std::string> timeoutConnections = getTimeoutConnections();

            // 处理超时连接
            for (const auto& connectionId : timeoutConnections) {
                handleTimeoutConnection(connectionId);
            }

            // 等待下次检查
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL_SECONDS));

        } catch (const std::exception& e) {
            std::cout << "心跳检测异常：" << e.what() << std::endl;
        }
    }
}

std::vector<std::string> ConnectionManager::getTimeoutConnections() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> timeoutConnections;

    auto now = std::chrono::steady_clock::now();
    auto timeoutThreshold = std::chrono::seconds(HEARTBEAT_TIMEOUT_SECONDS);

    for (const auto& pair : m_connections) {
        const auto& connectionId = pair.first;
        const auto& info = pair.second;

        if (info.isActive && (now - info.lastHeartbeat) > timeoutThreshold) {
            timeoutConnections.push_back(connectionId);
        }
    }

    return timeoutConnections;
}

void ConnectionManager::handleTimeoutConnection(const std::string& connectionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_connections.find(connectionId);
    if (it == m_connections.end() || !it->second.isActive) {
        return;
    }

    const auto& info = it->second;
    std::cout << "检测到连接超时：" << connectionId
              << " 用户：" << info.userName
              << " 房间：" << info.roomName << std::endl;

    // 标记为非活跃
    it->second.isActive = false;

    // 触发断线处理
    if (!info.userName.empty() && !info.roomName.empty()) {
        DisconnectManager::getInstance()->recordPlayerDisconnect(info.roomName, info.userName);
        std::cout << "已将用户 " << info.userName << " 标记为断线" << std::endl;
    }

    // 可选：主动关闭连接
    if (info.comm) {
        try {
            // 触发Connection的清理逻辑
            // 这里可以调用相应的清理方法
            std::cout << "触发连接清理：" << connectionId << std::endl;
        } catch (const std::exception& e) {
            std::cout << "清理连接时异常：" << e.what() << std::endl;
        }
    }
}