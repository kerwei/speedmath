// 密码学与 Unicode 工具测试 (mìmǎxué yǔ Unicode gōngjù cèshì — crypto & unicode helper tests)

#include "doctest.h"
#include "crypto.h"
#include "unicode.h"

TEST_CASE("pbkdf2 hash/verify roundtrip") {
    string h = crypto::pbkdf2_hash("secret123");
    CHECK(h.rfind("pbkdf2$", 0) == 0);
    CHECK(crypto::pbkdf2_verify("secret123", h));
    CHECK_FALSE(crypto::pbkdf2_verify("wrong", h));
    // 随机盐 (suíjī yán — random salt) → same password yields different hashes
    CHECK(crypto::pbkdf2_hash("secret123") != crypto::pbkdf2_hash("secret123"));
}

TEST_CASE("pbkdf2 rejects malformed stored") {
    CHECK_FALSE(crypto::pbkdf2_verify("x", ""));
    CHECK_FALSE(crypto::pbkdf2_verify("x", "not-a-hash"));
    CHECK_FALSE(crypto::pbkdf2_verify("x", "bcrypt$10$abc"));
}

TEST_CASE("aes gcm roundtrip") {
    crypto::set_enc_key(string(64, 'a'));  // 32 bytes of 0xaa
    string ct = crypto::aes_encrypt("alice@example.com");
    CHECK_FALSE(ct.empty());
    CHECK(crypto::aes_decrypt(ct) == "alice@example.com");
    // 篡改标签 (cuàngǎi biāoqiān — tamper with the tag) → decryption must fail
    string bad = ct;
    bad[bad.size() - 1] = (bad.back() == '0') ? '1' : '0';
    CHECK(crypto::aes_decrypt(bad).empty());
    // 未设置密钥 (wèi shèzhì mìyuè — no key set) → empty
    crypto::set_enc_key("");
    CHECK(crypto::aes_encrypt("x").empty());
    CHECK(crypto::aes_decrypt(ct).empty());
    crypto::set_enc_key(string(64, 'a'));
}

TEST_CASE("hmac known vector") {
    // RFC 4231 test vector: HMAC-SHA256(key="key", msg="The quick brown fox...")
    string h = crypto::hmac_hex("key", "The quick brown fox jumps over the lazy dog");
    CHECK(h == "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
}

TEST_CASE("hmac_enc uses configured key") {
    crypto::set_enc_key(string(64, 'a'));
    string h = crypto::hmac_enc("alice@example.com");
    CHECK(h.size() == 64);
    CHECK(crypto::hmac_enc("alice@example.com") == h);   // 确定性 (quèdìngxìng — deterministic)
    CHECK(crypto::hmac_enc("bob@example.com") != h);
    crypto::set_enc_key("");
    CHECK(crypto::hmac_enc("x").empty());                // 无密钥 → 空 (wú mìyuè — no key → empty)
    crypto::set_enc_key(string(64, 'a'));
}

TEST_CASE("random_hex length") {
    CHECK(crypto::random_hex(16).size() == 32);
    CHECK(crypto::random_hex(32).size() == 64);
}

TEST_CASE("utf8 code point count") {
    CHECK(crypto::utf8_len("abc") == 3);
    CHECK(crypto::utf8_len("") == 0);
    CHECK(crypto::utf8_len("你好") == 2);              // CJK (3 bytes each)
    CHECK(crypto::utf8_len("a\xF0\x9F\x91\x8D") == 2); // 'a' + 👍 (4 bytes)
}

TEST_CASE("unicode nfc normalization") {
    CHECK(uni::nfc("e\xCC\x81") == "\xC3\xA9");   // e + combining acute → é
    CHECK(uni::nfc("\xC3\xA9") == "\xC3\xA9");    // already composed stays
}

TEST_CASE("unicode letter-or-digit only") {
    CHECK(uni::is_letter_or_digit("Alice123"));
    CHECK(uni::is_letter_or_digit("你好"));
    CHECK(uni::is_letter_or_digit("héllo"));
    CHECK_FALSE(uni::is_letter_or_digit("hello!"));
    CHECK_FALSE(uni::is_letter_or_digit("a b"));
    CHECK_FALSE(uni::is_letter_or_digit("\xF0\x9F\x98\x80")); // 😀 emoji
    CHECK_FALSE(uni::is_letter_or_digit(""));
}
