//
// Created by fb on 2025/6/9.
//

#ifndef AESCRYPTO_H
#define AESCRYPTO_H


#include <string>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <map>
#include <cassert>
#include "Hash.h"

using namespace std;

class AesCrypto
{
public:
    enum Algorithm
    {
        //16bytes
        AES_ECB_128,
        AES_CBC_128,
        AES_CFB_128,
        AES_OFB_128,
        AES_CTR_128,
        //24bytes
        AES_ECB_192,
        AES_CBC_192,
        AES_CFB_192,
        AES_OFB_192,
        AES_CTR_192,
        //32bytes
        AES_ECB_256,
        AES_CBC_256,
        AES_CFB_256,
        AES_OFB_256,
        AES_CTR_256,
    };

    enum CryptoType{DECRYPTO, ENCRYPTO};

    using algorithmFunc = const EVP_CIPHER *(*)(void);

    AesCrypto(Algorithm algorithm, string key);
    ~AesCrypto();

    //加密
    string enctypt(string text);
    //解密
    string dectypt(string text);

private:
    string aesCrypto(string text, CryptoType type);
    void generateIvec(unsigned char* ivec);
    const map<Algorithm, algorithmFunc> m_algorithm
    {
            {AES_ECB_128, EVP_aes_128_ecb},
            {AES_CBC_128, EVP_aes_128_cbc},
            {AES_CFB_128, EVP_aes_128_cfb128},
            {AES_OFB_128, EVP_aes_128_ofb},
            {AES_CTR_128, EVP_aes_128_ctr},
            {AES_ECB_192, EVP_aes_192_ecb},
            {AES_CBC_192, EVP_aes_192_cbc},
            {AES_CFB_192, EVP_aes_192_cfb128},
            {AES_OFB_192, EVP_aes_192_ofb},
            {AES_CTR_192, EVP_aes_192_ctr},
            {AES_ECB_256, EVP_aes_256_ecb},
            {AES_CBC_256, EVP_aes_256_cbc},
            {AES_CFB_256, EVP_aes_256_cfb128},
            {AES_OFB_256, EVP_aes_256_ofb},
            {AES_CTR_256, EVP_aes_256_ctr}
    };

private:
    Algorithm m_type;
    string m_key;

};



#endif //AESCRYPTO_H
