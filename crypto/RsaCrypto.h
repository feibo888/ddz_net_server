//
// Created by fb on 2025/6/10.
//

#ifndef RSACRYPTO_H
#define RSACRYPTO_H


#include <string>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <map>
#include <cassert>
#include "Base64.h"
#include "Hash.h"

using namespace std;

class RsaCrypto
{
public:

    enum KeyLength
    {
        Bits_1K = 1024,
        Bits_2K = 2048,
        Bits_3K = 3072,
        Bits_4K = 4096
    };
    enum KeyType{PublicKey, PrivateKey};

    explicit RsaCrypto() = default;

    //构造对象并加载密钥文件中的数据到内存中
    explicit RsaCrypto(string fileName, KeyType type);
    ~RsaCrypto();

    //将密钥字符串解析为密钥类型
    void parseStringToKey(string data, KeyType type);
    //生成密钥对
    void generateRsaKey(KeyLength bits, string pub = "public.pem", string pri = "private.pem");

    //通过公钥进行加密
    string pubKeyEncrypt(string data);
    //通过私钥进行解密
    string priKeyDecrypt(string data);

    //数据签名
    string sign(string data, HashType hash = HashType::Sha256);
    //签名校验
    bool verify(string sign, string data, HashType hash = HashType::Sha256);

private:
    EVP_PKEY *m_pubKey = NULL;
    EVP_PKEY *m_priKey = NULL;


};



#endif //RSACRYPTO_H
