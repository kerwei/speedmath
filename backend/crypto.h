// 密码学工具 (mìmǎxué gōngjù — crypto helpers)
// PBKDF2 password hashing, AES-256-GCM encryption, HMAC-SHA256, CSPRNG — via OpenSSL

#pragma once

#include <string>
#include <vector>

using namespace std;

namespace crypto {

    // 设置加密密钥 (shèzhì jiāmì mìyuè — set the AES/HMAC key; 64 hex chars = 32 bytes)
    void set_enc_key(const string& key_hex);
    bool has_enc_key();

    // PBKDF2-HMAC-SHA256 (300k 次迭代 — iterations)
    // 格式 (géshì — format): pbkdf2$iterations$salt_hex$hash_hex
    string pbkdf2_hash(const string& password);
    bool pbkdf2_verify(const string& password, const string& stored);

    // AES-256-GCM: 输入明文, 输出 hex(iv||ciphertext||tag); 解密失败返回空串 (jiě mì shībài — empty on failure)
    string aes_encrypt(const string& plaintext);
    string aes_decrypt(const string& hex_payload);

    // HMAC-SHA256 → hex
    string hmac_hex(const string& key, const string& data);

    // HMAC-SHA256 使用配置的 enc_key (未设置密钥时返回空串 — empty if no key set)
    string hmac_enc(const string& data);

    // CSPRNG 随机字节 → hex (suíjī zìjié — random bytes)
    string random_hex(size_t bytes);

    // UTF-8 码点数量 (mǎdiǎn shùliàng — count of unicode code points)
    size_t utf8_len(const string& s);
}
