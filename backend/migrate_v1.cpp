// 一次性 v1→v2 数据库迁移工具 (dāncì v1→v2 shùjùkù qiānyí gōngjù — one-time v1→v2 DB migration tool)
// 仅由 db_migrate 目标编译; 应用 (backend) 不包含此逻辑 — zero residue, 可整体退役
//
// 用法 (usage): db_migrate <db_path> --config <quickmath.conf>
// 行为 (behavior):
//   - 无 users 表 (全新安装) 或已是 v2 → 无操作, exit 0
//   - v1 表 → 事务内重建: 邮箱加密 (AES-GCM) + 索引 (HMAC) + 随机用户名
// 退出码 (exit codes): 0 = 完成/跳过, 1 = 迁移失败, 2 = 用法/配置错误

#include "email_crypto.h"
#include "crypto.h"

#include <sqlite3.h>
#include <iostream>
#include <fstream>
#include <random>
#include <string>

using namespace std;

namespace {

string trim(const string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// 从 quickmath.conf 读取 enc_key (cóng pèizhì dúqǔ — read enc_key from config)
string read_enc_key(const string& conf_path) {
    ifstream f(conf_path);
    if (!f) return "";
    string line;
    while (getline(f, line)) {
        size_t hash = line.find('#');
        if (hash != string::npos) line = line.substr(0, hash);
        size_t eq = line.find('=');
        if (eq == string::npos) continue;
        if (trim(line.substr(0, eq)) == "enc_key") return trim(line.substr(eq + 1));
    }
    return "";
}

struct Sql {
    sqlite3* db = nullptr;
    ~Sql() { if (db) sqlite3_close(db); }

    bool open(const string& path) {
        return sqlite3_open(path.c_str(), &db) == SQLITE_OK && db != nullptr;
    }

    bool exec(const string& sql) {
        char* err = nullptr;
        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            cerr << "[migrate] exec error: " << err << endl;
            sqlite3_free(err);
            return false;
        }
        return true;
    }

    bool table_exists(const string& name) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", -1, &stmt, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        bool ok = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool has_column(const string& table, const string& col) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, ("PRAGMA table_info(" + table + ")").c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            return false;
        bool found = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* t = sqlite3_column_text(stmt, 1);
            if (t && string(reinterpret_cast<const char*>(t)) == col) { found = true; break; }
        }
        sqlite3_finalize(stmt);
        return found;
    }

    bool username_exists(const string& username) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM users_new WHERE username = ? LIMIT 1", -1, &stmt, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        bool ok = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        return ok;
    }

    bool step_done(sqlite3_stmt* stmt) {
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            cerr << "[migrate] step error: " << sqlite3_errmsg(db) << endl;
            return false;
        }
        return true;
    }
};

// 随机唯一用户名 (suíjī wéiyī yònghùmíng — random unique username against users_new)
string random_username(Sql& sql) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static random_device rd;
    for (int attempt = 0; attempt < 20; attempt++) {
        string name = "player";
        for (int i = 0; i < 6; i++) name += chars[rd() % 36];
        if (!sql.username_exists(name)) return name;
    }
    return "player" + to_string(rd() % 100000000);
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 4 || string(argv[2]) != "--config") {
        cerr << "usage: db_migrate <db_path> --config <quickmath.conf>" << endl;
        return 2;
    }
    string db_path = argv[1];
    string conf_path = argv[3];

    crypto::set_enc_key(read_enc_key(conf_path));
    if (!crypto::has_enc_key()) {
        cerr << "[migrate][FATAL] enc_key missing or invalid in " << conf_path
             << " (need 64 hex chars — generate with: openssl rand -hex 32)" << endl;
        return 2;
    }

    Sql sql;
    if (!sql.open(db_path)) {
        cerr << "[migrate][FATAL] cannot open database " << db_path << endl;
        return 1;
    }
    if (!sql.table_exists("users")) {
        cout << "[migrate] no users table — nothing to migrate (fresh install)" << endl;
        return 0;
    }
    if (sql.has_column("users", "email_hash")) {
        cout << "[migrate] users table already at v2 — nothing to do" << endl;
        return 0;
    }
    if (!sql.has_column("users", "email")) {
        cerr << "[migrate][FATAL] users table has neither v1 (email) nor v2 (email_hash) schema — aborting" << endl;
        return 1;
    }

    cout << "[migrate] migrating v1 → v2 (encrypting emails, assigning random usernames)..." << endl;
    if (!sql.exec("BEGIN TRANSACTION")) return 1;
    if (!sql.exec(
            "CREATE TABLE users_new ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "email_enc TEXT NOT NULL,"
            "email_hash TEXT UNIQUE NOT NULL,"
            "password_hash TEXT NOT NULL,"
            "username TEXT UNIQUE NOT NULL,"
            "username_updated_at TEXT,"
            "created_at TEXT DEFAULT (datetime('now'))"
            ")")) { sql.exec("ROLLBACK"); return 1; }

    sqlite3_stmt* sel = nullptr;
    if (sqlite3_prepare_v2(sql.db, "SELECT id, email, password_hash, created_at FROM users", -1, &sel, nullptr) != SQLITE_OK) {
        cerr << "[migrate] prepare select failed: " << sqlite3_errmsg(sql.db) << endl;
        sql.exec("ROLLBACK");
        return 1;
    }
    int migrated = 0;
    while (sqlite3_step(sel) == SQLITE_ROW) {
        int id = sqlite3_column_int(sel, 0);
        const unsigned char* email = sqlite3_column_text(sel, 1);
        const unsigned char* pw = sqlite3_column_text(sel, 2);
        const unsigned char* created = sqlite3_column_text(sel, 3);
        if (!email || !pw) {
            cerr << "[migrate][FATAL] NULL column in users row " << id << endl;
            sqlite3_finalize(sel);
            sql.exec("ROLLBACK");
            return 1;
        }
        string enc = email_enc_of(reinterpret_cast<const char*>(email));
        string hash = email_hash_of(reinterpret_cast<const char*>(email));
        string username = random_username(sql);
        if (enc.empty() || hash.empty() || username.empty()) {
            cerr << "[migrate][FATAL] email encryption failed for user " << id << endl;
            sqlite3_finalize(sel);
            sql.exec("ROLLBACK");
            return 1;
        }

        sqlite3_stmt* ins = nullptr;
        if (sqlite3_prepare_v2(sql.db,
                "INSERT INTO users_new (id, email_enc, email_hash, password_hash, username, username_updated_at, created_at) "
                "VALUES (?,?,?,?,?,NULL,?)", -1, &ins, nullptr) != SQLITE_OK) {
            cerr << "[migrate] prepare insert failed: " << sqlite3_errmsg(sql.db) << endl;
            sqlite3_finalize(sel);
            sql.exec("ROLLBACK");
            return 1;
        }
        sqlite3_bind_int(ins, 1, id);
        sqlite3_bind_text(ins, 2, enc.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 3, hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 4, reinterpret_cast<const char*>(pw), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 5, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 6, created ? reinterpret_cast<const char*>(created) : "", -1, SQLITE_TRANSIENT);
        bool ok = sql.step_done(ins);
        sqlite3_finalize(ins);
        if (!ok) { sqlite3_finalize(sel); sql.exec("ROLLBACK"); return 1; }
        migrated++;
    }
    sqlite3_finalize(sel);

    if (!sql.exec("DROP TABLE users")) { sql.exec("ROLLBACK"); return 1; }
    if (!sql.exec("ALTER TABLE users_new RENAME TO users")) { sql.exec("ROLLBACK"); return 1; }
    if (!sql.exec("COMMIT")) { sql.exec("ROLLBACK"); return 1; }

    cout << "[migrate] migrated " << migrated << " user(s) — emails encrypted, random usernames assigned" << endl;
    cout << "[migrate] done — this tool is a no-op if run again" << endl;
    return 0;
}
