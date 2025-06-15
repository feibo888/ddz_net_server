//
// Created by fb on 2025/6/9.
//

#ifndef CODEC_H
#define CODEC_H



#include "Information.pb.h"
#include <string>
#include <memory>

using namespace std;
struct Message
{
    string userName;
    string roomName;
    string data1;
    string data2;
    string data3;
    RequestCode reqCode;
    ResponseCode resCode;
};



class Codec
{
public:
    //序列化
    Codec(Message* msg);
    //反序列化
    Codec(string msg);
    //数据编码
    string enCodeMsg();
    //数据解码
    shared_ptr<Message> deCodeMsg();
    //重新加载数据
    void reload(Message* msg);
    void reload(string msg);

private:
    string m_msg;
    Information m_obj;

};



#endif //CODEC_H
