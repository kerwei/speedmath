// 数据库助手 (shùjùkù zhùshǒu — database helper)
// Thin C++ wrapper around SQLite for user/room/token storage
// v2 users schema: email stored encrypted (email_enc) with a deterministic index (email_hash)

#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <random>
#include <cctype>
#include "crypto.h"
#include "email_crypto.h"

using namespace std;

struct UserRow {
    int id;
    string email_enc;
    string email_hash;
    string password_hash;
    string username;
    string username_updated_at;  // "" = 从未自行设置过 (cóngwèi — never self-chosen)
    string created_at;
};

struct RoomRow {
    int id;
    string code;
    int host_id;
    string status;     // "waiting" | "playing" | "finished"
    string created_at;
};

struct RoomPlayerRow {
    int room_id;
    int user_id;
    bool ready;
    string username;
};

class Db {
public:
    Db(const string& path) {
        int rc = sqlite3_open(path.c_str(), &_db);
        if (rc != SQLITE_OK) {
            cerr << "[DB] Failed to open: " << sqlite3_errmsg(_db) << endl;
            _db = nullptr;
            return;
        }
        cerr << "[DB] Opened " << path << endl;
        _exec("PRAGMA journal_mode=WAL");
        _init_schema();
    }

    ~Db() {
        if (_db) sqlite3_close(_db);
    }

    bool ok() const { return _db != nullptr; }

    // 启动时校验 (qǐdòng shí jiàoyàn — startup check): 仅处理 v2 与全新数据库
    // v1 数据库必须先用 scripts/migrate-db.sh 迁移 — 应用不含一次性迁移逻辑 (zero residue)
    void migrate() {
        if (_table_exists("users")) {
            if (!_has_column("users", "email_hash")) {
                cerr << "[DB][FATAL] users table is at v1 (pre email-encryption). "
                     << "Run scripts/migrate-db.sh before starting this backend." << endl;
                exit(1);
            }
            cerr << "[DB] users table already at v2" << endl;
        } else {
            _create_users_table("users");
        }
        _seed_users();
        cerr << "[DB] Migration complete" << endl;
    }

    // ── Users ──

    bool create_user(const string& email_enc, const string& email_hash,
                     const string& password_hash, const string& username) {
        sqlite3_stmt* stmt = _prepare(
            "INSERT INTO users (email_enc, email_hash, password_hash, username) VALUES (?, ?, ?, ?)"
        );
        if (!stmt) return false;
        sqlite3_bind_text(stmt, 1, email_enc.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, email_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, password_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, username.c_str(), -1, SQLITE_TRANSIENT);
        bool ok = _step_done(stmt);
        sqlite3_finalize(stmt);
        return ok;
    }

    UserRow* find_user_by_email_hash(const string& email_hash) {
        sqlite3_stmt* stmt = _prepare(
            "SELECT id, email_enc, email_hash, password_hash, username, COALESCE(username_updated_at, ''), created_at "
            "FROM users WHERE email_hash = ?"
        );
        if (!stmt) return nullptr;
        sqlite3_bind_text(stmt, 1, email_hash.c_str(), -1, SQLITE_TRANSIENT);
        UserRow* row = nullptr;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            row = new UserRow{
                sqlite3_column_int(stmt, 0),
                _col_text(stmt, 1),
                _col_text(stmt, 2),
                _col_text(stmt, 3),
                _col_text(stmt, 4),
                _col_text(stmt, 5),
                _col_text(stmt, 6)
            };
        }
        sqlite3_finalize(stmt);
        return row;
    }

    UserRow* find_user_by_id(int id) {
        sqlite3_stmt* stmt = _prepare(
            "SELECT id, email_enc, email_hash, password_hash, username, COALESCE(username_updated_at, ''), created_at "
            "FROM users WHERE id = ?"
        );
        if (!stmt) return nullptr;
        sqlite3_bind_int(stmt, 1, id);
        UserRow* row = nullptr;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            row = new UserRow{
                sqlite3_column_int(stmt, 0),
                _col_text(stmt, 1),
                _col_text(stmt, 2),
                _col_text(stmt, 3),
                _col_text(stmt, 4),
                _col_text(stmt, 5),
                _col_text(stmt, 6)
            };
        }
        sqlite3_finalize(stmt);
        return row;
    }

    bool username_exists(const string& username) {
        sqlite3_stmt* stmt = _prepare("SELECT 1 FROM users WHERE username = ? LIMIT 1");
        if (!stmt) return false;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        bool exists = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        return exists;
    }

    // 随机用户名 (suíjī yòng hù míng — random username), 唯一性校验 (wéiyī xìng jiàoyàn — uniqueness-checked)
    string generate_random_username() {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        static random_device rd;
        for (int attempt = 0; attempt < 20; attempt++) {
            string name = "player";
            for (int i = 0; i < 6; i++) name += chars[rd() % 36];
            if (!username_exists(name)) return name;
        }
        return "player" + to_string(rd() % 100000000);
    }

    bool update_username(int user_id, const string& username, const string& updated_at) {
        sqlite3_stmt* stmt = _prepare(
            "UPDATE users SET username = ?, username_updated_at = ? WHERE id = ?"
        );
        if (!stmt) return false;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, updated_at.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, user_id);
        bool ok = _step_done(stmt);
        sqlite3_finalize(stmt);
        return ok;
    }

    bool update_password(int user_id, const string& password_hash) {
        sqlite3_stmt* stmt = _prepare("UPDATE users SET password_hash = ? WHERE id = ?");
        if (!stmt) return false;
        sqlite3_bind_text(stmt, 1, password_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, user_id);
        bool ok = _step_done(stmt);
        sqlite3_finalize(stmt);
        return ok;
    }

    // ── Tokens ──

    bool save_token(const string& token, int user_id) {
        sqlite3_stmt* stmt = _prepare(
            "INSERT OR REPLACE INTO tokens (token, user_id) VALUES (?, ?)"
        );
        if (!stmt) return false;
        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, user_id);
        bool ok = _step_done(stmt);
        sqlite3_finalize(stmt);
        return ok;
    }

    int find_user_by_token(const string& token) {
        sqlite3_stmt* stmt = _prepare(
            "SELECT user_id FROM tokens WHERE token = ?"
        );
        if (!stmt) return -1;
        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
        int uid = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            uid = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return uid;
    }

    void delete_token(const string& token) {
        sqlite3_stmt* stmt = _prepare("DELETE FROM tokens WHERE token = ?");
        if (!stmt) return;
        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // ── Rooms ──

    int create_room(const string& code, int host_id) {
        sqlite3_stmt* stmt = _prepare(
            "INSERT INTO rooms (code, host_id, status) VALUES (?, ?, 'waiting')"
        );
        if (!stmt) return -1;
        sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, host_id);
        if (!_step_done(stmt)) { sqlite3_finalize(stmt); return -1; }
        sqlite3_finalize(stmt);

        sqlite3_stmt* last = _prepare("SELECT last_insert_rowid()");
        int id = -1;
        if (sqlite3_step(last) == SQLITE_ROW) id = sqlite3_column_int(last, 0);
        sqlite3_finalize(last);
        return id;
    }

    RoomRow* find_room_by_code(const string& code) {
        sqlite3_stmt* stmt = _prepare(
            "SELECT id, code, host_id, status, created_at FROM rooms WHERE code = ?"
        );
        if (!stmt) return nullptr;
        sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
        RoomRow* row = nullptr;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            row = new RoomRow{
                sqlite3_column_int(stmt, 0),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                sqlite3_column_int(stmt, 2),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))
            };
        }
        sqlite3_finalize(stmt);
        return row;
    }

    RoomRow* find_room_by_id(int id) {
        sqlite3_stmt* stmt = _prepare(
            "SELECT id, code, host_id, status, created_at FROM rooms WHERE id = ?"
        );
        if (!stmt) return nullptr;
        sqlite3_bind_int(stmt, 1, id);
        RoomRow* row = nullptr;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            row = new RoomRow{
                sqlite3_column_int(stmt, 0),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                sqlite3_column_int(stmt, 2),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))
            };
        }
        sqlite3_finalize(stmt);
        return row;
    }

    bool update_room_status(int room_id, const string& status) {
        sqlite3_stmt* stmt = _prepare("UPDATE rooms SET status = ? WHERE id = ?");
        if (!stmt) return false;
        sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, room_id);
        bool ok = _step_done(stmt);
        sqlite3_finalize(stmt);
        return ok;
    }

    // ── Room Players ──

    bool add_room_player(int room_id, int user_id) {
        sqlite3_stmt* stmt = _prepare(
            "INSERT OR IGNORE INTO room_players (room_id, user_id, ready) VALUES (?, ?, 0)"
        );
        if (!stmt) return false;
        sqlite3_bind_int(stmt, 1, room_id);
        sqlite3_bind_int(stmt, 2, user_id);
        bool ok = _step_done(stmt);
        sqlite3_finalize(stmt);
        return ok;
    }

    bool set_player_ready(int room_id, int user_id, bool ready) {
        sqlite3_stmt* stmt = _prepare(
            "UPDATE room_players SET ready = ? WHERE room_id = ? AND user_id = ?"
        );
        if (!stmt) return false;
        sqlite3_bind_int(stmt, 1, ready ? 1 : 0);
        sqlite3_bind_int(stmt, 2, room_id);
        sqlite3_bind_int(stmt, 3, user_id);
        bool ok = _step_done(stmt);
        sqlite3_finalize(stmt);
        return ok;
    }

    vector<RoomPlayerRow> get_room_players(int room_id) {
        vector<RoomPlayerRow> rows;
        sqlite3_stmt* stmt = _prepare(
            "SELECT rp.room_id, rp.user_id, rp.ready, u.username "
            "FROM room_players rp JOIN users u ON rp.user_id = u.id "
            "WHERE rp.room_id = ?"
        );
        if (!stmt) return rows;
        sqlite3_bind_int(stmt, 1, room_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            rows.push_back({
                sqlite3_column_int(stmt, 0),
                sqlite3_column_int(stmt, 1),
                sqlite3_column_int(stmt, 2) != 0,
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))
            });
        }
        sqlite3_finalize(stmt);
        return rows;
    }

    void remove_room_player(int room_id, int user_id) {
        sqlite3_stmt* stmt = _prepare(
            "DELETE FROM room_players WHERE room_id = ? AND user_id = ?"
        );
        if (!stmt) return;
        sqlite3_bind_int(stmt, 1, room_id);
        sqlite3_bind_int(stmt, 2, user_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void delete_room(int room_id) {
        sqlite3_stmt* stmt = _prepare("DELETE FROM room_players WHERE room_id = ?");
        if (stmt) { sqlite3_bind_int(stmt, 1, room_id); sqlite3_step(stmt); sqlite3_finalize(stmt); }
        stmt = _prepare("DELETE FROM rooms WHERE id = ?");
        if (stmt) { sqlite3_bind_int(stmt, 1, room_id); sqlite3_step(stmt); sqlite3_finalize(stmt); }
    }

private:
    sqlite3* _db = nullptr;

    // 安全的列取值 (ānquán de liè qǔzhí — null-safe column text; NULL → "")
    static string _col_text(sqlite3_stmt* stmt, int col) {
        const unsigned char* t = sqlite3_column_text(stmt, col);
        return t ? string(reinterpret_cast<const char*>(t)) : string();
    }

    void _exec(const string& sql) {
        char* err = nullptr;
        if (sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            cerr << "[DB] exec error: " << err << endl;
            sqlite3_free(err);
        }
    }

    sqlite3_stmt* _prepare(const string& sql) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(_db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "[DB] prepare error: " << sqlite3_errmsg(_db) << endl;
            cerr << "[DB] SQL: " << sql << endl;
            return nullptr;
        }
        return stmt;
    }

    bool _step_done(sqlite3_stmt* stmt) {
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            cerr << "[DB] step error: " << sqlite3_errmsg(_db) << " (rc=" << rc << ")" << endl;
            return false;
        }
        return true;
    }

    // ── 迁移 (qiānyí — migration) ──

    bool _table_exists(const string& name) {
        sqlite3_stmt* stmt = _prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?");
        if (!stmt) return false;
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        bool exists = sqlite3_step(stmt) == SQLITE_ROW;
        sqlite3_finalize(stmt);
        return exists;
    }

    bool _has_column(const string& table, const string& col) {
        sqlite3_stmt* stmt = _prepare("PRAGMA table_info(" + table + ")");
        if (!stmt) return false;
        bool found = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (name == col) { found = true; break; }
        }
        sqlite3_finalize(stmt);
        return found;
    }

    void _create_users_table(const string& name) {
        // v2 结构: 邮箱加密存储 (yóuxiāng jiāmì cúnchǔ — email stored encrypted)
        _exec("CREATE TABLE IF NOT EXISTS " + name + " ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "email_enc TEXT NOT NULL,"
            "email_hash TEXT UNIQUE NOT NULL,"
            "password_hash TEXT NOT NULL,"
            "username TEXT UNIQUE NOT NULL,"
            "username_updated_at TEXT,"
            "created_at TEXT DEFAULT (datetime('now'))"
        ")");
    }

    // 种子测试用户 (zhǒngzǐ cèshì yònghù — seed test users, password: 000000)
    void _seed_users() {
        if (!crypto::has_enc_key()) {
            cerr << "[DB] enc_key not set — skipping seed users" << endl;
            return;
        }
        string pw = crypto::pbkdf2_hash("000000");
        const char* emails[] = {
            "test@quickmath.local", "test1@quickmath.local", "test2@quickmath.local",
            "test3@quickmath.local", "test4@quickmath.local"
        };
        const char* names[] = { "testplayer", "testplayer1", "testplayer2", "testplayer3", "testplayer4" };
        for (int i = 0; i < 5; i++) {
            sqlite3_stmt* stmt = _prepare(
                "INSERT OR IGNORE INTO users (email_enc, email_hash, password_hash, username) VALUES (?,?,?,?)"
            );
            if (!stmt) continue;
            string enc = email_enc_of(emails[i]);
            string hash = email_hash_of(emails[i]);
            sqlite3_bind_text(stmt, 1, enc.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, pw.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, names[i], -1, SQLITE_TRANSIENT);
            _step_done(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // 初始表结构 (chūshǐ biǎo jiégòu — initial schema; 无加密依赖 — no crypto needed)
    void _init_schema() {
        _exec(R"(
            CREATE TABLE IF NOT EXISTS tokens (
                token TEXT PRIMARY KEY,
                user_id INTEGER NOT NULL,
                created_at TEXT DEFAULT (datetime('now')),
                FOREIGN KEY (user_id) REFERENCES users(id)
            );
        )");
        _exec(R"(
            CREATE TABLE IF NOT EXISTS rooms (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                code TEXT UNIQUE NOT NULL,
                host_id INTEGER NOT NULL,
                status TEXT DEFAULT 'waiting',
                created_at TEXT DEFAULT (datetime('now')),
                FOREIGN KEY (host_id) REFERENCES users(id)
            );
        )");
        _exec(R"(
            CREATE TABLE IF NOT EXISTS room_players (
                room_id INTEGER NOT NULL,
                user_id INTEGER NOT NULL,
                ready INTEGER DEFAULT 0,
                PRIMARY KEY (room_id, user_id),
                FOREIGN KEY (room_id) REFERENCES rooms(id),
                FOREIGN KEY (user_id) REFERENCES users(id)
            );
        )");
        if (!_table_exists("users")) _create_users_table("users");
    }
};
