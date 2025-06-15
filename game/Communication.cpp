//
// Created by fb on 2025/6/11.
//

#include "Communication.h"

#include <RsaCrypto.h>
#include <netinet/in.h>
#include <memory>

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
    cout << "数据头长度（大端）：" << length << endl;
    length = ntohl(length);
    cout << "数据头长度（小端）：" << length << endl;
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
        cout << "AesFenFa success" << endl;
    }

}

void Communication::handleRegister(Message *reqMsg, Message &resMsg)
{
    //查询数据库中是否有该用户
    char sql[1024];
    sprintf(sql, "select name from user where name = '%s';", reqMsg->userName.data());

    bool flag = m_mysql->query(sql);

    if (flag && !m_mysql->next())           //查询操作成功但是没有数据
    {
        //将注册信息写到数据库中
        m_mysql->transaction();
        sprintf(sql, "insert into user (name, passwd, phone, date) values('%s', '%s', '%s', now());",
                                                                                        reqMsg->userName.data(),
                                                                                        reqMsg->data1.data(),
                                                                                        reqMsg->data2.data());
        bool fl1 = m_mysql->update(sql);

        sprintf(sql, "insert into information (name, score, status) values('%s', 0, 0);", reqMsg->userName.data());
        bool fl2 = m_mysql->update(sql);

        if (fl1 && fl2)
        {
            m_mysql->commit();
            resMsg.resCode = ResponseCode::RegisterOk;
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
    char sql[1024];
    sprintf(sql, "select name from user where name = '%s' and passwd = '%s' and (select count(*) from information where name = '%s' and status = 0);",
                                                                            reqMsg->userName.data(), reqMsg->data1.data(), reqMsg->userName.data());


    bool flag = m_mysql->query(sql);

    if (flag && m_mysql->next())
    {
        m_mysql->transaction();
        sprintf(sql, "update information set status = 1 where name = '%s';", reqMsg->userName.data());
        bool flag1 = m_mysql->update(sql);

        if (flag1)
        {
            m_mysql->commit();
            resMsg.resCode = ResponseCode::LoginOk;
            return;
        }
        m_mysql->rollback();
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
        roomName = m_redis->joinRoom(reqMsg->userName);
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
            //查询mysql, 并将其存储到redis中
            std::string sql = "select score from information where name = '" + reqMsg->userName + "';";

            bool flag1 = m_mysql->query(sql);
            assert(flag1);
            m_mysql->next();
            score = std::stoi(m_mysql->value(0));
        }
        m_redis->UpdatePlayerScore(roomName, reqMsg->userName, score);

        //将房间和玩家的关系保存到单例对象中
        RoomList* roomList = RoomList::getInstance();
        roomList->addUser(roomName, reqMsg->userName, m_sendCallback);

        //给客户端回复数据
        resMsg.resCode = ResponseCode::JoinRoomOK;
        resMsg.data1 = to_string(m_redis->getPlayerCount(roomName));
        resMsg.roomName = roomName;
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
}

void Communication::handleGoodBye(Message *reqMsg)
{
    //修改玩家的登录状态
    char sql[1024] = {0};
    sprintf(sql, "update information set status = 0 where name = '%s';", reqMsg->userName.data());
    cout << sql << endl;
    m_mysql->update(sql);
    //和客户端断开连接
    m_deleteCallback();
}

void Communication::handleGameOver(Message *reqMsg)
{
    int score = std::stoi(reqMsg->data1);
    //redis
    m_redis->UpdatePlayerScore(reqMsg->roomName, reqMsg->userName, score);
    //mysql
    char sql[1024];
    sprintf(sql, "update information set score = %d where name = '%s';", score, reqMsg->userName.data());
    m_mysql->update(sql);
}

void Communication::handleSearchRoom(Message *reqMsg, Message &resMsg)
{
    bool flag = m_redis->searchRoom(reqMsg->roomName);
    resMsg.resCode = ResponseCode::SearchRoomOK;
    resMsg.data1 = flag ? "true" : "false";
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
            m_cards.insert(make_pair(i, j));
        }
    }
    m_cards.insert(make_pair(0, 14));
    m_cards.insert(make_pair(0, 15));
}

std::pair<int, int> Communication::takeOneCard()
{
    //创建随机设备对象
    std::random_device rd;
    //创建随机数生成对象
    std::mt19937 gen(rd());
    //创建随机数分布对象， 均匀分布
    std::uniform_int_distribution<int> distrib(0, m_cards.size() - 1);
    int randNum = distrib(gen);

    auto it = m_cards.begin();
    for (int i = 0; i < randNum; ++i, ++it);

    m_cards.erase(it);

    return *it;

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
    //得到房间里的玩家
    auto players = RoomList::getInstance()->getPlayers(reqMsg->roomName);
    //判断房间人数
    if (players.size() == 3)
    {
        RoomList::getInstance()->removeRoom(reqMsg->roomName);
    }
    //将玩家添加到单例对象中
    RoomList::getInstance()->addUser(reqMsg->roomName, reqMsg->userName, m_sendCallback);

    cout << " continue game userName: " << reqMsg->userName << " roomName: " << reqMsg->roomName << endl;

    players = RoomList::getInstance()->getPlayers(reqMsg->roomName);
    if (players.size() == 3)
    {
        //发牌并开始游戏
        startGame(reqMsg->roomName, players);
    }
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

