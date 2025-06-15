//
// Created by fb on 2025/6/9.
//

#include "Codec.h"

Codec::Codec(Message *msg)
{
    reload(msg);
}

Codec::Codec(string msg)
{
    reload(msg);
}

//将Message序列化为字符串
string Codec::enCodeMsg()
{
    m_obj.SerializeToString(&m_msg);
    return m_msg;
}

//将字符串反序列化为Message
shared_ptr<Message> Codec::deCodeMsg()
{
    m_obj.ParseFromString(m_msg);
    Message *msg = new Message;
    msg->userName = m_obj.username();
    msg->roomName = m_obj.roomname();
    msg->data1 = m_obj.data1();
    msg->data2 = m_obj.data2();
    msg->data3 = m_obj.data3();
    msg->reqCode = m_obj.reqcode();
    msg->resCode = m_obj.rescode();

    shared_ptr<Message> ptr(msg, [this](Message* pt)
    {
        delete pt;
    });
    return ptr;
}

void Codec::reload(Message *msg)
{
    m_obj.set_username(msg->userName);
    m_obj.set_roomname(msg->roomName);
    m_obj.set_data1(msg->data1);
    m_obj.set_data2(msg->data2);
    m_obj.set_data3(msg->data3);
    m_obj.set_reqcode(msg->reqCode);
    m_obj.set_rescode(msg->resCode);
}

void Codec::reload(string msg)
{
    m_msg = msg;
}


