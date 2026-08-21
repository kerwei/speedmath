// 邮箱加密辅助 (yóuxiāng jiāmì fǔzhù — email crypto helpers)
// normalize + AES-GCM encrypt (存储/可还原) + HMAC index (查找索引/不可逆)
// 共享给 db.h (应用) 与 db_migrate (一次性迁移工具)

#pragma once

#include <string>
#include <cctype>
#include "crypto.h"

using namespace std;

// 规范化邮箱: 小写 + 去空格 (guīfàn huà — lowercase + strip spaces)
static string normalize_email(const string& email) {
    string e;
    e.reserve(email.size());
    for (unsigned char c : email) {
        if (c == ' ') continue;
        e += static_cast<char>(tolower(c));
    }
    return e;
}

// 确定性索引: HMAC-SHA256(enc_key, email) (quèdìngxìng suǒyǐn — deterministic lookup index)
static string email_hash_of(const string& email) { return crypto::hmac_enc(normalize_email(email)); }

// 加密存储: AES-256-GCM(enc_key, email)
static string email_enc_of(const string& email) { return crypto::aes_encrypt(normalize_email(email)); }
