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
}

Communication::~Communication()
{
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

    Message resMsg;
    switch (msg->reqCode)
    {
        case RequestCode::AesFenFa:
            handleAesFenFa(msg.get(), resMsg);
            break;
        case RequestCode::UserLogin:
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
        case RequestCode::GrabLord:
            resMsg.data1 = msg->data1;
            resMsg.resCode = ResponseCode::OtherGrabLord;
            realFunc = bind(&Communication::notifyOtherPlayers, this, placeholders::_1, msg->roomName, msg->userName);
            break;
        case RequestCode::PlayAHand:
            resMsg.data1 = msg->data1;
            resMsg.data2 = msg->data2;
            resMsg.resCode = ResponseCode::OtherPlayHand;
            realFunc = bind(&Communication::notifyOtherPlayers, this, placeholders::_1, msg->roomName, msg->userName);
            break;
        case RequestCode::GameOver:
            handleGameOver(msg.get());
            realFunc = nullptr;
            break;
        case RequestCode::Continue:
            restartGame(msg.get());
            realFunc = nullptr;
            break;
        case RequestCode::LeaveRoom:
            handleLeaveRoom(msg.get(), resMsg);
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
        cout << "AesFenFa failed" << endl;
        resMsg.resCode = ResponseCode::Failed;
        resMsg.data1 = "Aes密钥哈希校验失败...";
    }
    else
    {
        m_aes = new AesCrypto(AesCrypto::AES_CBC_256, aesKey);
        LOG(INFO) << "AesFenFa success";
    }

}

void Communication::handleRegister(Message *reqMsg, Message &resMsg)
{
    // //查询数据库中是否有该用户
    // char sql[1024];
    // sprintf(sql, "select name from user where name = '%s';", reqMsg->userName.data());

    // bool flag = m_mysql->query(sql);

    // if (flag && !m_mysql->next())           //查询操作成功但是没有数据
    // {
    //     //将注册信息写到数据库中
    //     m_mysql->transaction();
    //     sprintf(sql, "insert into user (name, passwd, phone, date) values('%s', '%s', '%s', now());",
    //                                                                                     reqMsg->userName.data(),
    //                                                                                     reqMsg->data1.data(),
    //                                                                                     reqMsg->data2.data());
    //     bool fl1 = m_mysql->update(sql);

    //     sprintf(sql, "insert into information (name, score, status) values('%s', 0, 0);", reqMsg->userName.data());
    //     bool fl2 = m_mysql->update(sql);

    //     if (fl1 && fl2)
    //     {
    //         m_mysql->commit();
    //         resMsg.resCode = ResponseCode::RegisterOk;
    //     }
    //     else
    //     {
    //         m_mysql->rollback();
    //         resMsg.resCode = ResponseCode::Failed;
    //         resMsg.data1 = "数据库插入数据失败";
    //     }
    // }
    // else
    // {
    //     resMsg.resCode = ResponseCode::Failed;
    //     resMsg.data1 = "用户名已存在，无法注册";
    // }


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
    // char sql[1024];
    // sprintf(sql, "select name from user where name = '%s' and passwd = '%s' and (select count(*) from information where name = '%s' and status = 0);",
    //                                                                         reqMsg->userName.data(), reqMsg->data1.data(), reqMsg->userName.data());


    // bool flag = m_mysql->query(sql);

    // if (flag && m_mysql->next())
    // {
    //     m_mysql->transaction();
    //     sprintf(sql, "update information set status = 1 where name = '%s';", reqMsg->userName.data());
    //     bool flag1 = m_mysql->update(sql);

    //     if (flag1)
    //     {
    //         m_mysql->commit();
    //         resMsg.resCode = ResponseCode::LoginOk;
    //         return;
    //     }
    //     m_mysql->rollback();
    // }
    // resMsg.resCode = ResponseCode::Failed;
    // resMsg.data1 = "用户名或密码错误，或者当前玩家已经成功登录...";



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
            // 参数化查询mysql, 并将其存储到redis中
            // //查询mysql, 并将其存储到redis中
            // std::string sql = "select score from information where name = '" + reqMsg->userName + "';";

            // bool flag1 = m_mysql->query(sql);
            // assert(flag1);
            // m_mysql->next();
            // score = std::stoi(m_mysql->value(0));

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
                    }
                }
                m_mysql->closeStmt();
            }
        }
        m_redis->UpdatePlayerScore(roomName, reqMsg->userName, score);

        //将房间和玩家的关系保存到单例对象中
        RoomList* roomList = RoomList::getInstance();
        roomList->addUser(roomName, reqMsg->userName, m_sendCallback);

        //给客户端回复数据
        resMsg.resCode = ResponseCode::JoinRoomOK;
        resMsg.data1 = to_string(m_redis->getPlayerCount(roomName));
        resMsg.roomName = roomName;

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
    // int score = std::stoi(reqMsg->data1);
    // //redis
    // m_redis->UpdatePlayerScore(reqMsg->roomName, reqMsg->userName, score);
    // //mysql
    // char sql[1024];
    // sprintf(sql, "update information set score = %d where name = '%s';", score, reqMsg->userName.data());
    // m_mysql->update(sql);

    int score = std::stoi(reqMsg->data1);
    // redis
    m_redis->UpdatePlayerScore(reqMsg->roomName, reqMsg->userName, score);
    cout << "更新玩家分数: " << reqMsg->userName << " 分数: " << score << endl;
    // mysql 参数化更新分数
    std::string sql = "update information set score = ? where name = ?";
    if (m_mysql->prepare(sql)) {
        std::string name = reqMsg->userName;
        MYSQL_BIND bind[2] = {0};
        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &score;
        bind[0].is_unsigned = 0;
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (void*)name.c_str();
        bind[1].buffer_length = name.size();
        m_mysql->bindParam(bind);
        m_mysql->execute();
        m_mysql->closeStmt();
    }
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
    std::string& all = msg.data1;
    for (int i = 0; i < 51; ++i)
    {
        auto card = takeOneCard();
        std::string sub = std::to_string(card.first) + "-" + std::to_string(card.second) + "#";
        all += sub;
    }

    //剩余的3张底牌
    std::string& lastCard = msg.data2;
    for (const auto& it : m_cards)
    {
        std::string sub = std::to_string(it.first) + "-" + std::to_string(it.second) + "#";
        lastCard += sub;
    }

    msg.resCode = ResponseCode::DealCards;
    Codec codec(&msg);

    //遍历当前房间中的所有玩家
    for (const auto& it : players)
    {
        it.second(codec.enCodeMsg());
    }

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
