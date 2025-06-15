//
// Created by fb on 2025/6/10.
//

#ifndef BASE64_H
#define BASE64_H


#include <string>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

using namespace std;

class Base64
{
public:
    explicit Base64() = default;

    //编码
    string encode(string data);
    string encode(const char* data, int size);
    //解码
    string decode(string data);
    string decode(const char* data, int size);


};



#endif //BASE64_H
