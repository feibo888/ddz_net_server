//
// Created by fb on 2025/6/11.
//

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <AesCrypto.h>

#include "Buffer.h"
#include <Codec.h>
#include "MysqlConn.h"
#include "Room.h"
#include "RoomList.h"


class Communication {
public:
    Communication();
    ~Communication();

    using sendCallback = function<void(string)>;
    using deleteCallback = function<void()>;

    //设置回调
    void setCallback(sendCallback cb1, deleteCallback cb2);

    //解析客户端发来的请求
    void parseRequest(Buffer* buf);
    //处理aes密钥分发
    void handleAesFenFa(Message* reqMsg, Message& resMsg);
    //处理用户注册
    void handleRegister(Message* reqMsg, Message& resMsg);
    //处理用户注册
    void handleLogin(Message* reqMsg, Message& resMsg);
    //处理用户加入房间
    void handleAddRoom(Message* reqMsg, Message& resMsg);
    //处理用户离开房间
    void handleLeaveRoom(Message* reqMsg, Message& resMsg);
    //处理断开连接
    void handleGoodBye(Message* reqMsg);
    //处理游戏结束
    void handleGameOver(Message* reqMsg);
    //处理搜索房间
    void handleSearchRoom(Message* reqMsg, Message& resMsg);
    //准备开始游戏
    void readyForPlay(std::string roomName, std::string data);
    //发牌
    void dealCards(userMap players);
    //洗牌
    void initCards();
    //随机取出一张牌
    std::pair<int, int> takeOneCard();
    //数据转发函数
    void notifyOtherPlayers(std::string data, std::string roomName, std::string userName);
    //重新开始游戏
    void restartGame(Message* reqMsg);
    //开始游戏
    void startGame(std::string roomName, userMap players);



private:
    sendCallback m_sendCallback;
    deleteCallback m_deleteCallback;
    AesCrypto* m_aes = nullptr;
    MysqlConn* m_mysql = nullptr;
    Room* m_redis = nullptr;
    std::multimap<int, int> m_cards;
};


#endif //COMMUNICATION_H
