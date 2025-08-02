//
// Created by fb on 2025/6/11.
//

#include "Communication.h"

#include <RsaCrypto.h>
#include <netinet/in.h>
#include <memory>
#include <glog/logging.h>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>

#include "JsonParse.h"
#include "RoomList.h"

Communication::Communication()
{
    JsonParse js;
    shared_ptr<DBInfo> info = js.getDBInfo(JsonParse::Mysql);

    m_mysql = new MysqlConn();
    bool flag = m_mysql->connect(info->user, info->password, info->dbname, info->ip, info->port);

    assert(flag);

    m_redis = new Room();
    flag = m_redis->initEnvironment();

    assert(flag);

    m_pendingDisconnectCheck.clear();

    generateConnectionId();

}

Communication::~Communication()
{
    // 注销连接
    ConnectionManager::getInstance()->unregisterConnection(m_connectionId);

    if (!m_currentRoomName.empty() && !m_currentUserName.empty()) {
        DisconnectManager::getInstance()->recordPlayerDisconnect(m_currentRoomName, m_currentUserName);
        std::cout << "玩家 " << m_currentUserName << " 从房间 " << m_currentRoomName << " 断线" << std::endl;
    }

    if (m_redis)
    {
        delete m_redis;
    }
    if (m_mysql)
    {
        delete m_mysql;
    }
    if (m_aes)
    {
        delete m_aes;
    }
}

void Communication::setCallback(sendCallback cb1, deleteCallback cb2)
{
    m_sendCallback = cb1;
    m_deleteCallback = cb2;
}

void Communication::parseRequest(Buffer* buf)
{
    //读数据头
    string data = buf->data(sizeof(int));
    int length = *(int*)data.data();
    //读数据块
    length = ntohl(length);
    data = buf->data(length);

    if (m_aes)
    {
        data = m_aes->dectypt(data);
    }

    //数据反序列化
    Codec codec(data);
    shared_ptr<Message> msg = codec.deCodeMsg();

    sendCallback realFunc = m_sendCallback;

    // 更新当前用户信息（用于断线检测）
    if (!msg->roomName.empty() && !msg->userName.empty()) {
        m_currentRoomName = msg->roomName;
        m_currentUserName = msg->userName;
    }

    // 检查是否为断线玩家的回合
    if (msg->reqCode == RequestCode::PlayAHand) {
        DisconnectManager* disconnectMgr = DisconnectManager::getInstance();
        if (disconnectMgr->isDisconnectedPlayer(msg->roomName, msg->userName)) {
            std::cout << "忽略断线玩家 " << msg->userName << " 的出牌请求" << std::endl;
            return; // 断线玩家的请求应该被忽略
        }
    }

    //cout << "收到请求码: " << msg->reqCode << ", 用户: " << msg->userName << endl;

    Message resMsg;
    switch (msg->reqCode)
    {
        case RequestCode::AesFenFa:
            cout << "处理AES密钥分发" << endl;
            handleAesFenFa(msg.get(), resMsg);
            break;
        case RequestCode::UserLogin:
            cout << "处理用户登录: " << msg->userName << endl;
            handleLogin(msg.get(), resMsg);
            break;
        case RequestCode::Register:
            handleRegister(msg.get(), resMsg);
            break;
        case RequestCode::AutoRoom:
        case RequestCode::ManualRoom:
            handleAddRoom(msg.get(), resMsg);
            realFunc = bind(&Communication::readyForPlay, this, resMsg.roomName, placeholders::_1);
            break;
        case RequestCode::SearchRoom:
            handleSearchRoom(msg.get(), resMsg);
            break;
        case RequestCode::Heartbeat:
            handleHeartbeat(msg.get(), resMsg);
            break;
        case RequestCode::GrabLord:
        {
            resMsg.data1 = msg->data1;
            resMsg.resCode = ResponseCode::OtherGrabLord;
            realFunc = bind(&Communication::notifyOtherPlayers, this, placeholders::_1, msg->roomName, msg->userName);

            // 检查是否确定了地主
            if (isLordConfirmed(msg->data1)) {
                // 地主开始出牌，设置超时
                TurnTimeoutManager::getInstance()->startPlayerTurn(msg->roomName, msg->userName, 30);
                // 将底牌加入地主手牌
                if (m_redis) {
                    std::string bottomCards = m_redis->getBottomCards(msg->roomName);
                    if (!bottomCards.empty()) {
                        m_redis->addCardsToPlayer(msg->roomName, msg->userName, bottomCards);
                        std::cout << "地主 " << msg->userName << " 获得底牌：" << bottomCards << std::endl;

                        // 设置游戏状态
                        m_redis->setGameState(msg->roomName, "lord", msg->userName);
                        m_redis->setGameState(msg->roomName, "current_turn", msg->userName);
                        m_redis->setGameState(msg->roomName, "game_controller", "");
                        m_redis->setGameState(msg->roomName, "game_phase", "playing");
                        std::cout << "地主 " << msg->userName << " 开始出牌" << std::endl;
                    }
                }
            }

            break;
        }
        case RequestCode::PlayAHand:
        {
            // 先取消当前玩家的超时检测
            TurnTimeoutManager::getInstance()->finishPlayerTurn(msg->roomName, msg->userName);

            // 更新游戏状态：记录当前出牌玩家
            if (m_redis) {
                m_redis->setGameState(msg->roomName, "current_turn", msg->userName);
                // 检查是否为过牌：data1为"0"或空表示过牌
                std::string cardCount = msg->data1;
                if (cardCount != "0" && !cardCount.empty() && !msg->data2.empty()) {
                    // 不是过牌，更新控牌玩家
                    m_redis->setGameState(msg->roomName, "game_controller", msg->userName);
                }
            }

            // 更新玩家手牌（移除出的牌）
            updatePlayerHandAfterPlay(msg->roomName, msg->userName, msg->data2);


            resMsg.data1 = msg->data1;
            resMsg.data2 = msg->data2;
            resMsg.resCode = ResponseCode::OtherPlayHand;
            realFunc = bind(&Communication::notifyOtherPlayers, this, placeholders::_1, msg->roomName, msg->userName);

            // 确定下一个玩家
            std::string nextPlayer = getNextPlayer(msg->roomName, msg->userName);
            if (m_redis) {
                m_redis->setGameState(msg->roomName, "current_turn", nextPlayer);
            }

            if (!nextPlayer.empty()) {
                DisconnectManager* disconnectMgr = DisconnectManager::getInstance();
                if (disconnectMgr->isDisconnectedPlayer(msg->roomName, nextPlayer)) {
                    // 保存断线检测信息，延迟到消息发送后处理
                    m_pendingDisconnectCheck = PendingDisconnectCheck(msg->roomName, nextPlayer);
                    std::cout << "准备延迟处理断线玩家：" << nextPlayer << std::endl;
                } else {
                    std::cout << "下一个玩家 " << nextPlayer << " 在线，无需AI托管" << std::endl;
                    m_pendingDisconnectCheck.clear();
                    // 在线玩家设置出牌超时
                    TurnTimeoutManager::getInstance()->startPlayerTurn(msg->roomName, nextPlayer, 30);
                }
            } else {
                std::cout << "警告：无法确定下一个出牌玩家！" << std::endl;
                m_pendingDisconnectCheck.clear();
            }

            break;
        }
        case RequestCode::GameOver:
        {
            handleGameOver(msg.get());

            // 清理手牌数据
            if (m_redis) {
                m_redis->clearRoomCards(msg->roomName);
                std::cout << "游戏结束，清理房间 " << msg->roomName << " 的所有数据" << std::endl;
            }
            // 清理断线记录
            DisconnectManager::getInstance()->cleanupGame(msg->roomName);
            // 清理该房间所有超时检测
            TurnTimeoutManager::getInstance()->clearRoomTimeouts(msg->roomName);

            realFunc = nullptr;
            break;
        }
        case RequestCode::Continue:
            restartGame(msg.get());
            realFunc = nullptr;
            break;
        case RequestCode::LeaveRoom:
            handleLeaveRoom(msg.get(), resMsg);
            //TurnTimeoutManager::getInstance()->finishPlayerTurn(msg->roomName, msg->userName);
            realFunc = nullptr;
            break;
        case RequestCode::GoodBye:
            handleGoodBye(msg.get());
            realFunc = nullptr;
            break;
        case RequestCode::ReDealCards:
            handleReDealCards(msg.get(), resMsg);
            realFunc = nullptr;
        default:
            break;
    }
    if (realFunc != nullptr)
    {
        codec.reload(&resMsg);
        realFunc(codec.enCodeMsg());
        //cout << "已回复请求: " << msg->reqCode << ", 响应码: " << resMsg.resCode << endl;

        if (!m_pendingDisconnectCheck.isEmpty()) {
            try {
                std::cout << "当前玩家消息已发送，开始处理断线玩家AI出牌" << std::endl;

                DisconnectManager* disconnectMgr = DisconnectManager::getInstance();

                // **新增：支持连续多个断线玩家的迭代处理**
                std::string currentDisconnectedPlayer = m_pendingDisconnectCheck.nextPlayer;
                std::string roomName = m_pendingDisconnectCheck.roomName;
                int maxIterations = 3; // 最多处理3个连续断线玩家
                int iteration = 0;

                while (!currentDisconnectedPlayer.empty() &&
                       disconnectMgr->isDisconnectedPlayer(roomName, currentDisconnectedPlayer) &&
                       iteration < maxIterations) {

                    std::cout << "迭代处理断线玩家 [" << (iteration + 1) << "/3]: " << currentDisconnectedPlayer << std::endl;

                    // 执行AI出牌（不会递归）
                    disconnectMgr->executeAIPlay(roomName, currentDisconnectedPlayer);

                    // 获取下一个玩家
                    std::string nextPlayer = getNextPlayer(roomName, currentDisconnectedPlayer);

                    // 更新游戏状态
                    if (m_redis) {
                        m_redis->setGameState(roomName, "current_turn", nextPlayer);
                    }

                    // 检查下一个玩家是否也断线
                    if (!nextPlayer.empty() && disconnectMgr->isDisconnectedPlayer(roomName, nextPlayer)) {
                        std::cout << "检测到下一个玩家 " << nextPlayer << " 也断线，继续迭代处理" << std::endl;
                        currentDisconnectedPlayer = nextPlayer;
                        iteration++;
                    } else {
                        if (nextPlayer.empty()) {
                            std::cout << "无法确定下一个玩家，停止处理" << std::endl;
                        } else {
                            std::cout << "下一个玩家 " << nextPlayer << " 在线，完成断线处理" << std::endl;
                        }
                        break;
                    }
                }

                if (iteration >= maxIterations) {
                    std::cout << "警告：连续断线玩家处理达到上限(" << maxIterations << ")，停止处理" << std::endl;
                }

                std::cout << "多玩家断线AI出牌处理完成，共处理 " << (iteration + 1) << " 个断线玩家" << std::endl;

            } catch (const std::exception& e) {
                std::cout << "处理多玩家断线AI出牌时发生异常：" << e.what() << std::endl;
            } catch (...) {
                std::cout << "处理多玩家断线AI出牌时发生未知异常" << std::endl;
            }

            // 清理临时数据
            m_pendingDisconnectCheck.clear();
        }
    }

}

void Communication::handleAesFenFa(Message* reqMsg, Message& resMsg)
{
    RsaCrypto rsa;
    rsa.parseStringToKey(m_redis->getRsaSecKey("PrivateKey"), RsaCrypto::PrivateKey);

    string aesKey = rsa.priKeyDecrypt(reqMsg->data1);

    //哈希检验
    Hash h(HashType::Sha224);
    h.addData(aesKey);
    string res = h.result();


    resMsg.resCode = ResponseCode::AesVerifyOk;
    if (reqMsg->data2 != res)
    {
        cout << "AesFenFa failed: hash mismatch" << endl;
        cout << "Expected: " << res << endl;
        cout << "Got: " << reqMsg->data2 << endl;
        resMsg.resCode = ResponseCode::Failed;
        resMsg.data1 = "Aes密钥哈希校验失败...";
    }
    else
    {
        m_aes = new AesCrypto(AesCrypto::AES_CBC_256, aesKey);
        cout << "AesFenFa success, 响应码: " << (int)resMsg.resCode << endl;
        LOG(INFO) << "AesFenFa success";
    }

}

void Communication::handleRegister(Message *reqMsg, Message &resMsg)
{
    // 1. 查询数据库中是否有该用户
    std::string sql = "select name from user where name = ?";
    if (!m_mysql->prepare(sql)) {
        resMsg.resCode = ResponseCode::Failed;
        resMsg.data1 = "数据库操作失败";
        return;
    }
    std::string name = reqMsg->userName;
    MYSQL_BIND bind[1] = {0};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)name.c_str();
    bind[0].buffer_length = name.size();

    if (!m_mysql->bindParam(bind) || !m_mysql->execute() || !m_mysql->storeResult()) {
        resMsg.resCode = ResponseCode::Failed;
        resMsg.data1 = "数据库操作失败";
        m_mysql->closeStmt();
        return;
    }

    bool exists = m_mysql->fetch();
    m_mysql->closeStmt();

    if (!exists) // 没有该用户，可以注册
    {
        m_mysql->transaction();

        // 2. 插入user表
        std::string insertUserSql = "insert into user (name, passwd, phone, date) values (?, ?, ?, now())";
        if (!m_mysql->prepare(insertUserSql)) {
            m_mysql->rollback();
            resMsg.resCode = ResponseCode::Failed;
            resMsg.data1 = "数据库操作失败";
            return;
        }
        std::string passwd = reqMsg->data1;
        std::string phone = reqMsg->data2;
        MYSQL_BIND insertBind[3] = {0};
        insertBind[0].buffer_type = MYSQL_TYPE_STRING;
        insertBind[0].buffer = (void*)name.c_str();
        insertBind[0].buffer_length = name.size();
        insertBind[1].buffer_type = MYSQL_TYPE_STRING;
        insertBind[1].buffer = (void*)passwd.c_str();
        insertBind[1].buffer_length = passwd.size();
        insertBind[2].buffer_type = MYSQL_TYPE_STRING;
        insertBind[2].buffer = (void*)phone.c_str();
        insertBind[2].buffer_length = phone.size();

        bool fl1 = m_mysql->bindParam(insertBind) && m_mysql->execute();
        m_mysql->closeStmt();

        // 3. 插入information表
        std::string insertInfoSql = "insert into information (name, score, status) values (?, 0, 0)";
        if (!m_mysql->prepare(insertInfoSql)) {
            m_mysql->rollback();
            resMsg.resCode = ResponseCode::Failed;
            resMsg.data1 = "数据库操作失败";
            return;
        }
        MYSQL_BIND infoBind[1] = {0};
        infoBind[0].buffer_type = MYSQL_TYPE_STRING;
        infoBind[0].buffer = (void*)name.c_str();
        infoBind[0].buffer_length = name.size();

        bool fl2 = m_mysql->bindParam(infoBind) && m_mysql->execute();
        m_mysql->closeStmt();

        if (fl1 && fl2)
        {
            m_mysql->commit();
            resMsg.resCode = ResponseCode::RegisterOk;
            LOG(INFO) << "user register OK: " << name;
        }
        else
        {
            m_mysql->rollback();
            resMsg.resCode = ResponseCode::Failed;
            resMsg.data1 = "数据库插入数据失败";
        }
    }
    else
    {
        resMsg.resCode = ResponseCode::Failed;
        resMsg.data1 = "用户名已存在，无法注册";
    }

}

void Communication::handleLogin(Message *reqMsg, Message &resMsg)
{

    // 参数化查询：查找用户
    std::string sql = "select name from user where name = ? and passwd = ? and "
                      "(select count(*) from information where name = ? and status = 0)";
    if (!m_mysql->prepare(sql)) {
        resMsg.resCode = ResponseCode::Failed;
        resMsg.data1 = "数据库操作失败";
        return;
    }

    MYSQL_BIND bind[3] = {0};
    // name
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)reqMsg->userName.data();
    bind[0].buffer_length = reqMsg->userName.size();
    // passwd
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)reqMsg->data1.data();
    bind[1].buffer_length = reqMsg->data1.size();
    // name again
    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)reqMsg->userName.data();
    bind[2].buffer_length = reqMsg->userName.size();

    if (!m_mysql->bindParam(bind) || !m_mysql->execute() || !m_mysql->storeResult()) {
        resMsg.resCode = ResponseCode::Failed;
        resMsg.data1 = "数据库操作失败";
        m_mysql->closeStmt();
        return;
    }

    if (m_mysql->fetch()) {
        m_mysql->closeStmt();
        // 登录成功，更新状态
        m_mysql->transaction();
        std::string updateSql = "update information set status = 1 where name = ?";
        if (!m_mysql->prepare(updateSql)) {
            m_mysql->rollback();
            resMsg.resCode = ResponseCode::Failed;
            resMsg.data1 = "数据库操作失败";
            return;
        }
        MYSQL_BIND updateBind[1] = {0};
        updateBind[0].buffer_type = MYSQL_TYPE_STRING;
        updateBind[0].buffer = (void*)reqMsg->userName.data();
        updateBind[0].buffer_length = reqMsg->userName.size();

        bool flag1 = m_mysql->bindParam(updateBind) && m_mysql->execute();
        m_mysql->closeStmt();

        if (flag1) {
            m_mysql->commit();
            resMsg.resCode = ResponseCode::LoginOk;
            m_currentUserName = reqMsg->userName;
            ConnectionManager::getInstance()->registerConnection(m_connectionId, reqMsg->userName, "", this);
            LOG(INFO) << "user login success: " << reqMsg->userName;
            return;
        }
        m_mysql->rollback();
    } else {
        m_mysql->closeStmt();
    }
    resMsg.resCode = ResponseCode::Failed;
    resMsg.data1 = "用户名或密码错误，或者当前玩家已经成功登录...";

}

void Communication::handleAddRoom(Message *reqMsg, Message &resMsg)
{
    //如果当前玩家已经不是第一次加入房间
    std::string oldRoom = m_redis->whereAmI(reqMsg->userName);
    //读这个玩家上次加入房间结束后的分数
    int score = m_redis->getPlayerScore(oldRoom, reqMsg->userName);

    if (oldRoom != std::string())
    {
        m_redis->leaveRoom(oldRoom, reqMsg->userName);
        RoomList::getInstance()->removePlayer(oldRoom, reqMsg->userName);
    }

    bool flag = true;
    string roomName;
    if (reqMsg->reqCode == RequestCode::AutoRoom)
    {
        // 使用无锁算法进行房间匹配
        roomName = m_redis->joinRoomWithoutLock(reqMsg->userName);
    }
    else
    {
        roomName = reqMsg->roomName;
        flag = m_redis->joinRoom(reqMsg->userName, roomName);
    }

    if (flag)
    {
        //第一次加载分数，在redis中更新分数，最后将分数同步到mysql
        if (score == 0)
        {
            // 使用分布式锁确保分数加载的原子性
            std::string scoreLockKey = "score_load_" + reqMsg->userName;
            bool lockAcquired = m_redis->acquireLockWithRetry(scoreLockKey, 5, 3);
            
            if (lockAcquired) {
                try {
                    // 参数化查询mysql, 并将其存储到redis中
                    std::string sql = "select score from information where name = ?";
                    if (m_mysql->prepare(sql)) {
                        std::string name = reqMsg->userName;
                        MYSQL_BIND bind[1] = {0};
                        bind[0].buffer_type = MYSQL_TYPE_STRING;
                        bind[0].buffer = (void*)name.c_str();
                        bind[0].buffer_length = name.size();

                        if (m_mysql->bindParam(bind) && m_mysql->execute() && m_mysql->storeResult()) {
                            int scoreResult = 0;
                            if (m_mysql->fetchInt(scoreResult)) {
                                score = scoreResult;
                                LOG(INFO) << "Loaded score from MySQL: " << name << " score: " << score;
                            }
                        }
                        m_mysql->closeStmt();
                    }
                } catch (const std::exception& e) {
                    cout << "加载玩家分数时发生错误: " << e.what() << endl;
                    LOG(ERROR) << "Error loading player score: " << e.what();
                    // 使用默认分数0
                    score = 0;
                }
                
                m_redis->releaseLock(scoreLockKey);
            } else {
                cout << "获取分数加载锁失败，使用默认分数: " << reqMsg->userName << endl;
                score = 0;
            }
        }
        
        // 原子性地更新Redis中的分数
        try {
            m_redis->UpdatePlayerScore(roomName, reqMsg->userName, score);
        } catch (const std::exception& e) {
            cout << "更新Redis分数失败: " << e.what() << endl;
            LOG(ERROR) << "Failed to update Redis score: " << e.what();
            // 可以考虑重试或使用MySQL作为备用
        }

        //将房间和玩家的关系保存到单例对象中
        RoomList* roomList = RoomList::getInstance();
        roomList->addUser(roomName, reqMsg->userName, m_sendCallback);

        //给客户端回复数据
        resMsg.resCode = ResponseCode::JoinRoomOK;
        resMsg.data1 = to_string(m_redis->getPlayerCount(roomName));
        resMsg.roomName = roomName;
        m_currentRoomName = roomName;

        // 更新连接管理器中的房间信息
        ConnectionManager::getInstance()->unregisterConnection(m_connectionId);
        ConnectionManager::getInstance()->registerConnection(m_connectionId, m_currentUserName, m_currentRoomName, this);

        LOG(INFO) << "玩家: " << reqMsg->userName << "进入房间: " << roomName;
    }
    else
    {
        resMsg.resCode = ResponseCode::Failed;
        resMsg.data1 = "加入的房间已满";
    }
}

void Communication::handleLeaveRoom(Message* reqMsg, Message& resMsg)
{
    m_redis->leaveRoom(reqMsg->roomName, reqMsg->userName);

    RoomList::getInstance()->removePlayer(reqMsg->roomName, reqMsg->userName);
    resMsg.resCode = ResponseCode::OtherLeaveRoom;

    auto players = RoomList::getInstance()->getPlayers(reqMsg->roomName);
    resMsg.data1 = to_string(players.size());
    for (auto item : players)
    {
        Codec codec(&resMsg);
        item.second(codec.enCodeMsg());
    }
    LOG(INFO) << "玩家: " << reqMsg->userName << "离开房间: " << reqMsg->roomName;
}

void Communication::handleGoodBye(Message *reqMsg)
{
    // //修改玩家的登录状态
    // char sql[1024] = {0};
    // sprintf(sql, "update information set status = 0 where name = '%s';", reqMsg->userName.data());
    // cout << sql << endl;
    // m_mysql->update(sql);
    // //和客户端断开连接
    // m_deleteCallback();

    // 参数化更新玩家的登录状态
    std::string sql = "update information set status = 0 where name = ?";
    if (m_mysql->prepare(sql)) {
        std::string name = reqMsg->userName;
        MYSQL_BIND bind[1] = {0};
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void*)name.c_str();
        bind[0].buffer_length = name.size();
        m_mysql->bindParam(bind);
        m_mysql->execute();
        m_mysql->closeStmt();
    }
    // 和客户端断开连接
    m_deleteCallback();
}

void Communication::handleGameOver(Message *reqMsg)
{
    int score = std::stoi(reqMsg->data1);
    std::string userName = reqMsg->userName;
    std::string roomName = reqMsg->roomName;
    
    cout << "开始更新玩家分数: " << userName << " 分数: " << score << endl;
    
    // 使用分布式锁确保更新操作的原子性
    std::string lockKey = "score_update_" + userName;
    bool lockAcquired = m_redis->acquireLockWithRetry(lockKey, 10, 3);
    
    if (!lockAcquired) {
        cout << "获取分数更新锁失败: " << userName << endl;
        return;
    }
    
    try {
        // 开启MySQL事务
        m_mysql->transaction();
        
        // 1. 先更新MySQL（作为权威数据源）
        std::string sql = "update information set score = ? where name = ?";
        bool mysqlSuccess = false;
        
        if (m_mysql->prepare(sql)) {
            MYSQL_BIND bind[2] = {0};
            bind[0].buffer_type = MYSQL_TYPE_LONG;
            bind[0].buffer = &score;
            bind[0].is_unsigned = 0;
            bind[1].buffer_type = MYSQL_TYPE_STRING;
            bind[1].buffer = (void*)userName.c_str();
            bind[1].buffer_length = userName.size();
            
            mysqlSuccess = m_mysql->bindParam(bind) && m_mysql->execute();
            m_mysql->closeStmt();
        }
        
        if (mysqlSuccess) {
            // 2. MySQL更新成功，提交事务
            m_mysql->commit();
            
            // 3. 更新Redis缓存
            try {
                m_redis->UpdatePlayerScore(roomName, userName, score);
                cout << "分数更新成功: " << userName << " 新分数: " << score << endl;
                LOG(INFO) << "Score updated successfully: " << userName << " score: " << score;
            } catch (const std::exception& e) {
                // Redis更新失败，记录错误但不回滚MySQL（最终一致性）
                cout << "Redis分数更新失败: " << userName << " 错误: " << e.what() << endl;
                LOG(ERROR) << "Redis score update failed: " << userName << " error: " << e.what();
                
                // 可以考虑加入重试队列或者异步修复机制
                scheduleRedisScoreSync(roomName, userName, score);
            }
        } else {
            // MySQL更新失败，回滚事务
            m_mysql->rollback();
            cout << "MySQL分数更新失败，事务已回滚: " << userName << endl;
            LOG(ERROR) << "MySQL score update failed, transaction rolled back: " << userName;
        }
        
    } catch (const std::exception& e) {
        // 发生异常，回滚MySQL事务
        m_mysql->rollback();
        cout << "分数更新过程中发生异常: " << e.what() << endl;
        LOG(ERROR) << "Exception during score update: " << e.what();
    }
    
    // 释放分布式锁
    m_redis->releaseLock(lockKey);
}

void Communication::handleSearchRoom(Message *reqMsg, Message &resMsg)
{
    bool flag = m_redis->searchRoom(reqMsg->roomName);
    resMsg.resCode = ResponseCode::SearchRoomOK;
    resMsg.data1 = flag ? "true" : "false";
}

void Communication::handleReDealCards(Message *reqMsg, Message &resMsg)
{
    auto players = RoomList::getInstance()->getPlayers(reqMsg->roomName);

    // 清理旧的手牌数据
    if (m_redis)
    {
        m_redis->clearRoomCards(reqMsg->roomName);
        std::cout << "重新发牌，清理房间 " << reqMsg->roomName << " 的旧数据" << std::endl;
    }

    //发牌数据
    dealCards(players);
    //通知客户端可以开始游戏了
    Message msg;
    msg.resCode = ResponseCode::StartGame;
    // data1 : userName-次序-分数
    msg.data1 = m_redis->getPlayerOrder(reqMsg->roomName);

    cout << "Starting game: " << msg.data1 << endl;
    Codec codec(&msg);

    for (const auto& it : players)
    {
        it.second(codec.enCodeMsg());
    }
}


void Communication::readyForPlay(std::string roomName, std::string data)
{
    RoomList* instance = RoomList::getInstance();
    userMap players = instance->getPlayers(roomName);

    //房间没满
    for (auto it : players)
    {
        it.second(data);
    }

    if (players.size() == 3)
    {
        startGame(roomName, players);
    }
}

void Communication::dealCards(userMap players)
{
    Message msg;
    initCards();

    // 获取房间名（从第一个玩家的连接中推导）
    std::string roomName = m_currentRoomName;

    if (roomName.empty()) {
        std::cout << "无法确定房间名，跳过手牌跟踪" << std::endl;
        // 回退到原始逻辑
        std::string& all = msg.data1;
        for (int i = 0; i < 51; ++i) {
            auto card = takeOneCard();
            std::string sub = std::to_string(card.first) + "-" + std::to_string(card.second) + "#";
            all += sub;
        }

        std::string& lastCard = msg.data2;
        for (const auto& it : m_cards) {
            std::string sub = std::to_string(it.first) + "-" + std::to_string(it.second) + "#";
            lastCard += sub;
        }

        msg.resCode = ResponseCode::DealCards;
        Codec codec(&msg);
        for (const auto& it : players) {
            it.second(codec.enCodeMsg());
        }
        return;
    }

    // 清理之前的手牌数据
    if (m_redis) {
        m_redis->clearRoomCards(roomName);
    }

    // 获取玩家顺序（基于分数排序）
    std::string orderData = m_redis->getPlayerOrder(roomName);
    std::vector<std::string> playerNames = parsePlayerOrder(orderData);

    if (playerNames.size() != 3) {
        std::cout << "玩家数量不正确，跳过手牌跟踪" << std::endl;
        // 回退到原始逻辑...
        return;
    }

    if (m_redis) {
        m_redis->setGamePlayerOrder(roomName, orderData);
        std::cout << "保存固定游戏顺序：" << orderData << std::endl;
    }

    std::string& all = msg.data1;

    // 按客户端逻辑：轮流分配51张牌（模拟客户端的分牌过程）
    std::vector<std::string> playerCards(3);  // 按分数顺序的3个玩家的手牌

    for (int i = 0; i < 51; ++i) {
        auto card = takeOneCard();
        std::string cardStr = std::to_string(card.first) + "-" + std::to_string(card.second) + "#";
        all += cardStr;

        // 关键：按分数顺序轮流分配给玩家（每人17张）
        // 第1张给分数最高的玩家(playerNames[0])，第2张给分数第二的玩家(playerNames[1])，依此类推
        int playerIndex = i % 3;  // 轮流分配：0->1->2->0->1->2...
        playerCards[playerIndex] += cardStr;
    }

    // 将跟踪的手牌存储到Redis（按分数顺序）
    for (int i = 0; i < 3; ++i) {
        if (m_redis && !playerCards[i].empty()) {
            m_redis->setPlayerCards(roomName, playerNames[i], playerCards[i]);
            std::cout << "跟踪玩家 " << playerNames[i] << " 手牌：" << playerCards[i] << std::endl;
        }
    }

    // 底牌处理（保持现有逻辑）
    std::string& lastCard = msg.data2;
    for (const auto& it : m_cards) {
        std::string cardStr = std::to_string(it.first) + "-" + std::to_string(it.second) + "#";
        lastCard += cardStr;
    }

    // 存储底牌
    if (m_redis) {
        m_redis->setBottomCards(roomName, lastCard);
    }

    msg.resCode = ResponseCode::DealCards;
    Codec codec(&msg);

    // 发送给所有玩家（保持现有协议）
    for (const auto& it : players) {
        it.second(codec.enCodeMsg());
    }

}

std::vector<std::string> Communication::parsePlayerOrder(const std::string& orderData) {
    std::vector<std::string> names;

    if (orderData.empty()) {
        return names;
    }

    // 解析格式："playerA-55#playerC-43#playerB-34#"
    // Redis中已经按分数从高到低排序了
    std::stringstream ss(orderData);
    std::string playerEntry;

    while (std::getline(ss, playerEntry, '#')) {
        if (playerEntry.empty()) continue;

        size_t dashPos = playerEntry.find('-');
        if (dashPos != std::string::npos) {
            std::string playerName = playerEntry.substr(0, dashPos);
            names.push_back(playerName);
        }
    }

    return names;
}

void Communication::initCards()
{
    m_cards.clear();
    //花色
    for (int i = 1; i <= 4; ++i)
    {
        //点数
        for (int j = 1; j <= 13; ++j)
        {
            m_cards.push_back(make_pair(i, j));  // 改为push_back
        }
    }
    m_cards.push_back(make_pair(0, 14));  // 大王
    m_cards.push_back(make_pair(0, 15));  // 小王

    // 真正的洗牌：使用std::shuffle打乱牌的顺序
    // 使用高精度时间作为随机种子，确保每次洗牌结果不同
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937 gen(seed);
    std::shuffle(m_cards.begin(), m_cards.end(), gen);

    cout << "洗牌完成，牌库大小: " << m_cards.size() << endl;
}

std::pair<int, int> Communication::takeOneCard()
{
    // 由于已经在initCards()中进行了洗牌，直接从牌库末尾取牌即可
    if (m_cards.empty()) {
        cout << "错误：牌库为空，无法取牌！" << endl;
        return make_pair(-1, -1);  // 返回无效牌表示错误
    }

    auto card = m_cards.back();  // 取最后一张牌
    m_cards.pop_back();          // 移除最后一张牌

    return card;
}

void Communication::notifyOtherPlayers(std::string data, std::string roomName, std::string userName)
{
    //得到另外两个玩家
    auto players = RoomList::getInstance()->getPartners(roomName, userName);
    for (const auto& it : players)
    {
        it.second(data);
    }
}

void Communication::restartGame(Message *reqMsg)
{
    cout << "开始处理继续游戏" << endl;

    // 为继续游戏操作加锁，确保同一房间的重启操作串行化
    std::string restartLockKey = "restart_lock_" + reqMsg->roomName;

    // 增加重试逻辑
    bool lockAcquired = false;
    const int maxRetries = 5;  // 最多重试5次
    const int retryDelayMs = 100;  // 每次重试间隔100毫秒

    for (int i = 0; i < maxRetries; ++i) {
        if (m_redis->acquireLock(restartLockKey, 10)) {
            lockAcquired = true;
            break;
        }
        cout << "获取房间锁失败: " << reqMsg->roomName << "，正在重试 (" << (i+1) << "/" << maxRetries << ")" << endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
    }

    if (!lockAcquired) {
        cout << "获取房间锁失败: " << reqMsg->roomName << "，达到最大重试次数，放弃处理" << endl;
        return;
    }

    try {
        // 在锁保护下执行继续游戏逻辑
        auto players = RoomList::getInstance()->getPlayers(reqMsg->roomName);
        cout << "Current players in room " << reqMsg->roomName << ": " << players.size() << endl;

        // 检查房间是否已满（3人），如果满了需要重置房间状态
        if (players.size() == 3) {
            cout << "Room is full, removing existing room state" << endl;
            RoomList::getInstance()->removeRoom(reqMsg->roomName);
        }

        // 将当前玩家添加到单例对象中
        RoomList::getInstance()->addUser(reqMsg->roomName, reqMsg->userName, m_sendCallback);
        cout << "Added player " << reqMsg->userName << " to room " << reqMsg->roomName << endl;

        // 重新获取房间中的玩家数量
        players = RoomList::getInstance()->getPlayers(reqMsg->roomName);
        cout << "Players after adding " << reqMsg->userName << ": " << players.size() << endl;

        // 检查是否达到3人，如果是则开始游戏
        if (players.size() == 3) {
            cout << "Room is full, starting game for room: " << reqMsg->roomName << endl;
            // 在开始游戏前，确保Redis中的房间状态也正确
            //syncRoomStateToRedis(reqMsg->roomName, players);
            startGame(reqMsg->roomName, players);
        }

    } catch (const std::exception& e) {
        cout << "Error in restartGame: " << e.what() << endl;
    }

    // 释放锁
    m_redis->releaseLock(restartLockKey);
    cout << "继续游戏处理完成，房间: " << reqMsg->roomName << " 玩家: " << reqMsg->userName << endl;
}

void Communication::startGame(std::string roomName, userMap players)
{
    //房间满了
    //发牌数据
    dealCards(players);
    //通知客户端可以开始游戏了
    Message msg;
    msg.resCode = ResponseCode::StartGame;
    // data1 : userName-次序-分数
    msg.data1 = m_redis->getPlayerOrder(roomName);

    cout << "Starting game: " << msg.data1 << endl;
    Codec codec(&msg);

    for (const auto& it : players)
    {
        it.second(codec.enCodeMsg());
    }
}

// void Communication::syncRoomStateToRedis(std::string roomName, userMap players)
// {
//     try {
//         // 确保Redis中的房间状态与内存中的状态一致
//         // 1. 先清理Redis中所有相关的房间集合状态
//         m_redis->srem("OnePlayer", roomName);
//         m_redis->srem("TwoPlayers", roomName);
//         m_redis->srem("ThreePlayers", roomName);
//         m_redis->srem("Invalid", roomName);
//
//         // 如果没有玩家，直接删除房间数据并返回
//         if (players.empty()) {
//             m_redis->del(roomName);
//             return;
//         }
//
//         // 2. 重新添加所有玩家到Redis房间
//         // 首先清除旧数据，保留分数信息
//         std::map<std::string, int> playerScores;
//         for (const auto& player : players) {
//             const std::string& userName = player.first;
//             // 先获取玩家现有分数（如果有的话）
//             int score = m_redis->getPlayerScore(roomName, userName);
//             playerScores[userName] = score;
//         }
//
//         // 清理房间数据
//         m_redis->del(roomName);
//
//         // 重新设置玩家数据和分数
//         for (const auto& player : players) {
//             const std::string& userName = player.first;
//             m_redis->UpdatePlayerScore(roomName, userName, playerScores[userName]);
//             // 更新玩家-房间映射
//             m_redis->hset("Players", userName, roomName);
//         }
//
//         // 3. 根据玩家数量更新房间集合状态
//         int playerCount = players.size();
//         if (playerCount == 3) {
//             m_redis->sadd("ThreePlayers", roomName);
//         } else if (playerCount == 2) {
//             m_redis->sadd("TwoPlayers", roomName);
//         } else if (playerCount == 1) {
//             m_redis->sadd("OnePlayer", roomName);
//         }
//
//         cout << "房间状态已同步到Redis: " << roomName
//              << " 玩家数: " << playerCount << endl;
//
//     } catch (const std::exception& e) {
//         cout << "同步房间状态到Redis时出错: " << e.what() << endl;
//     }
// }

// 私有方法：异步修复Redis分数数据
void Communication::scheduleRedisScoreSync(const std::string& roomName, const std::string& userName, int score)
{
    // 简单的重试机制，实际项目中可以使用消息队列
    std::thread([this, roomName, userName, score]() {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待5秒后重试
        
        try {
            m_redis->UpdatePlayerScore(roomName, userName, score);
            LOG(INFO) << "Redis score sync repair successful: " << userName << " score: " << score;
        } catch (const std::exception& e) {
            LOG(ERROR) << "Redis score sync repair failed: " << userName << " error: " << e.what();
            // 可以考虑写入错误队列或者继续重试
        }
    }).detach();
}

// 数据一致性检查方法
bool Communication::verifyScoreConsistency(const std::string& userName)
{
    try {
        // 从MySQL获取权威分数
        int mysqlScore = 0;
        std::string sql = "select score from information where name = ?";
        if (m_mysql->prepare(sql)) {
            MYSQL_BIND bind[1] = {0};
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (void*)userName.c_str();
            bind[0].buffer_length = userName.size();

            if (m_mysql->bindParam(bind) && m_mysql->execute() && m_mysql->storeResult()) {
                m_mysql->fetchInt(mysqlScore);
            }
            m_mysql->closeStmt();
        }
        
        // 从Redis获取缓存分数
        std::string currentRoom = m_redis->whereAmI(userName);
        if (!currentRoom.empty()) {
            int redisScore = m_redis->getPlayerScore(currentRoom, userName);
            
            if (mysqlScore != redisScore) {
                LOG(WARNING) << "Score inconsistency detected: " << userName 
                           << " MySQL: " << mysqlScore << " Redis: " << redisScore;
                
                // 修复不一致，以MySQL为准
                m_redis->UpdatePlayerScore(currentRoom, userName, mysqlScore);
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Error verifying score consistency: " << e.what();
        return false;
    }
}

bool Communication::isLordConfirmed(const std::string& grabData) {
    // 根据实际的抢地主协议判断
    // 这里需要根据具体的协议来实现
    // 临时实现：假设"3"表示抢地主成功
    return grabData == "3";
}

void Communication::updatePlayerHandAfterPlay(const std::string& roomName, const std::string& userName, const std::string& playedCards) {
    if (!m_redis) return;

    // playedCards是data2，包含序列化的Card对象
    if (playedCards.empty()) {
        std::cout << "玩家 " << userName << " 过牌" << std::endl;
        return; // 过牌，不需要移除手牌
    }

    // 解析data2中的Card对象（QDataStream格式）
    // data2格式：每张牌是两个int（suit, point）
    const char* data = playedCards.c_str();
    int dataSize = playedCards.size();
    int cardCount = dataSize / (2 * sizeof(int)); // 每张牌占用2个int的空间

    std::cout << "玩家 " << userName << " 出牌数量：" << cardCount << std::endl;

    // 解析每张牌并从Redis中移除
    for (int i = 0; i < cardCount; ++i) {
        int offset = i * 2 * sizeof(int);
        if (offset + 2 * sizeof(int) <= dataSize) {
            // 读取 suit 和 point (按QDataStream的大端序格式)
            int suit = ntohl(*reinterpret_cast<const int*>(data + offset));
            int point = ntohl(*reinterpret_cast<const int*>(data + offset + sizeof(int)));

            // 构造卡牌字符串格式："suit-point"
            std::string cardStr = std::to_string(suit) + "-" + std::to_string(point);
            m_redis->removeCardFromPlayer(roomName, userName, cardStr);

            std::cout << "移除玩家 " << userName << " 的牌：" << cardStr << std::endl;
        }
    }

    // 检查玩家手牌是否为空（游戏结束）
    std::string remainingCards = m_redis->getPlayerCards(roomName, userName);
    if (remainingCards.empty() || remainingCards == "#") {
        std::cout << "玩家 " << userName << " 手牌出完，游戏结束！" << std::endl;
        // 这里可以触发游戏结束逻辑
    }
}

std::string Communication::getNextPlayer(const std::string& roomName, const std::string& currentPlayer) {
    if (!m_redis) {
        return "";
    }

    // 获取Redis中按分数排序的玩家顺序（这个数据在游戏开始时就固定了，不会因为断线而丢失）
    std::string orderData = m_redis->getGamePlayerOrder(roomName);
    if (orderData.empty()) {
        std::cout << "警告：无法获取房间(communication) " << roomName << " 的玩家顺序数据" << std::endl;
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
        }
    }

    std::cout << "警告：在玩家顺序中未找到当前玩家：" << currentPlayer << std::endl;
    return orderedPlayers[0]; // 默认返回第一个
}

void Communication::generateConnectionId() {
    // 生成唯一连接ID
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    m_connectionId = "conn_" + std::to_string(timestamp) + "_" + std::to_string(rand() % 1000);
}

void Communication::handleHeartbeat(Message* reqMsg, Message& resMsg) {
    try {
        // 更新心跳时间
        ConnectionManager::getInstance()->updateHeartbeat(m_connectionId);

        // 设置心跳响应
        resMsg.resCode = ResponseCode::HeartbeatReply;
        resMsg.userName = reqMsg->userName;
        resMsg.roomName = reqMsg->roomName;
        resMsg.data1 = "pong";

        // 可选：日志记录（调试用）
        // std::cout << "心跳响应已设置：" << reqMsg->userName << std::endl;

    } catch (const std::exception& e) {
        std::cout << "处理心跳时异常：" << e.what() << std::endl;
    }
}
