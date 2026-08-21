// 密码学工具实现 (mìmǎxué gōngjù shíxiàn — crypto helpers implementation)
#include "crypto.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include <sstream>

using namespace std;

namespace crypto {

static string g_enc_key_hex;

void set_enc_key(const string& key_hex) { g_enc_key_hex = key_hex; }
bool has_enc_key() { return g_enc_key_hex.size() == 64; }

static string hex_encode(const unsigned char* d, size_t n) {
    static const char* h = "0123456789abcdef";
    string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { s += h[d[i] >> 4]; s += h[d[i] & 0xF]; }
    return s;
}

static vector<unsigned char> hex_decode(const string& hex) {
    vector<unsigned char> out;
    if (hex.size() % 2 != 0) return out;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = nib(hex[i]), lo = nib(hex[i + 1]);
        if (hi < 0 || lo < 0) { out.clear(); return out; }
        out.push_back(static_cast<unsigned char>((hi << 4) | lo));
    }
    return out;
}

string pbkdf2_hash(const string& password) {
    const int ITERS = 300000;
    unsigned char salt[16];
    unsigned char dk[32];
    if (RAND_bytes(salt, sizeof salt) != 1) return "";
    if (PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.size(), salt, sizeof salt, ITERS, EVP_sha256(), sizeof dk, dk) != 1) return "";
    stringstream ss;
    ss << "pbkdf2$" << ITERS << "$" << hex_encode(salt, sizeof salt) << "$" << hex_encode(dk, sizeof dk);
    return ss.str();
}

bool pbkdf2_verify(const string& password, const string& stored) {
    size_t p1 = stored.find('$');
    if (p1 == string::npos) return false;
    size_t p2 = stored.find('$', p1 + 1);
    if (p2 == string::npos) return false;
    size_t p3 = stored.find('$', p2 + 1);
    if (p3 == string::npos) return false;
    if (stored.substr(0, p1) != "pbkdf2") return false;
    int iters = atoi(stored.substr(p1 + 1, p2 - p1 - 1).c_str());
    auto salt = hex_decode(stored.substr(p2 + 1, p3 - p2 - 1));
    auto want = hex_decode(stored.substr(p3 + 1));
    if (iters <= 0 || salt.empty() || want.empty() || want.size() > 64) return false;
    unsigned char dk[64];
    if (PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.size(), salt.data(), (int)salt.size(), iters, EVP_sha256(), (int)want.size(), dk) != 1) return false;
    return CRYPTO_memcmp(dk, want.data(), want.size()) == 0;
}

string aes_encrypt(const string& plaintext) {
    if (!has_enc_key()) return "";
    auto key = hex_decode(g_enc_key_hex);
    if (key.size() != 32) return "";
    unsigned char iv[12];
    if (RAND_bytes(iv, sizeof iv) != 1) return "";
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";
    vector<unsigned char> out(plaintext.size() + 16);
    int len1 = 0, len2 = 0;
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, sizeof iv, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv);
    EVP_EncryptUpdate(ctx, out.data(), &len1, (const unsigned char*)plaintext.data(), (int)plaintext.size());
    EVP_EncryptFinal_ex(ctx, out.data() + len1, &len2);
    unsigned char tag[16];
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, sizeof tag, tag);
    EVP_CIPHER_CTX_free(ctx);
    string payload;
    payload.append((const char*)iv, sizeof iv);
    payload.append((const char*)out.data(), len1 + len2);
    payload.append((const char*)tag, sizeof tag);
    return hex_encode((const unsigned char*)payload.data(), payload.size());
}

string aes_decrypt(const string& hex_payload) {
    if (!has_enc_key()) return "";
    auto key = hex_decode(g_enc_key_hex);
    auto blob = hex_decode(hex_payload);
    if (key.size() != 32 || blob.size() < 12 + 16) return "";
    const unsigned char* p = blob.data();
    size_t ctlen = blob.size() - 12 - 16;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";
    vector<unsigned char> out(ctlen + 16);
    int len1 = 0, len2 = 0;
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), p);
    EVP_DecryptUpdate(ctx, out.data(), &len1, p + 12, (int)ctlen);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)(p + blob.size() - 16));
    int ok = EVP_DecryptFinal_ex(ctx, out.data() + len1, &len2);
    EVP_CIPHER_CTX_free(ctx);
    if (ok != 1) return "";
    return string((const char*)out.data(), len1 + len2);
}

string hmac_hex(const string& key, const string& data) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int outlen = 0;
    HMAC(EVP_sha256(), key.data(), (int)key.size(), (const unsigned char*)data.data(), data.size(), out, &outlen);
    return hex_encode(out, outlen);
}

string hmac_enc(const string& data) {
    if (!has_enc_key()) return "";
    auto key = hex_decode(g_enc_key_hex);   // 与 AES 一致: 使用解码后的 32 字节密钥
    if (key.size() != 32) return "";
    return hmac_hex(string((const char*)key.data(), key.size()), data);
}

string random_hex(size_t bytes) {
    vector<unsigned char> buf(bytes);
    if (RAND_bytes(buf.data(), (int)bytes) != 1) return "";
    return hex_encode(buf.data(), bytes);
}

size_t utf8_len(const string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++;
    return n;
}

} // namespace crypto
