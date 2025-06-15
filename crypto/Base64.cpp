//
// Created by fb on 2025/6/10.
//

#include "Base64.h"



string Base64::encode(string data)
{
    return encode(data.data(), data.size());
}

string Base64::encode(const char *data, int size)
{
    // 数据要进程base64编码-> 得到了新的字符串, 需要存到内存中(需要一个内存类型的BIO对象)
    // 创建base64对象
    BIO* b64 = BIO_new(BIO_f_base64());
    // 创建内存对象
    BIO* mem = BIO_new(BIO_s_mem());
    // 组织BIO链  b64->mem
    BIO_push(b64, mem);
    // 往bio链中添加数据 -> 编码
    BIO_write(b64, data, size);
    // 强制刷新数据-> 数据在内存BIO中
    BIO_flush(b64);
    // 需要将BIO内存对象中的数据取出
    BUF_MEM* ptr;
    // 这个地方传递bio链的头结点就可以了, 通过链结构可以自动找到内存类型的bio对象
    BIO_get_mem_ptr(b64, &ptr);
    string str(ptr->data, ptr->length);
    // 释放整个bio链
    BIO_free_all(b64);

    return str;
}

string Base64::decode(string data)
{
    return decode(data.data(), data.size());
}

string Base64::decode(const char *data, int size)
{
    // 创建base64对象
    BIO* b64 = BIO_new(BIO_f_base64());
    // 创建内存对象
    BIO* mem = BIO_new(BIO_s_mem());
    // 组织BIO链 b64->mem, 读数据, 数据流动的方向尾节点 -> 头结点
    BIO_push(b64, mem);
    // 给尾节点初始化数据
    BIO_write(mem, data, size);


    // 解码, buf中存储了b64节点解码之后的数据
    char* buf = new char[size];
    int ret = BIO_read(b64, buf, size);
    string str(buf, ret);
    // 释放整个bio链
    delete[]buf;
    BIO_free_all(b64);

    return str;
}

