// 数据库助手 (shùjùkù zhùshǒu — database helper)
// Thin C++ wrapper around SQLite for user/room/token storage

#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>
#include <iostream>
#include <sstream>

using namespace std;

struct UserRow {
    int id;
    string email;
    string password_hash;
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
        _migrate();
    }

    ~Db() {
        if (_db) sqlite3_close(_db);
    }

    bool ok() const { return _db != nullptr; }

    // ── Users ──

    bool create_user(const string& email, const string& password_hash) {
        sqlite3_stmt* stmt = _prepare(
            "INSERT INTO users (email, password_hash) VALUES (?, ?)"
        );
        if (!stmt) return false;
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
        bool ok = _step_done(stmt);
        sqlite3_finalize(stmt);
        return ok;
    }

    UserRow* find_user_by_email(const string& email) {
        sqlite3_stmt* stmt = _prepare(
            "SELECT id, email, password_hash, created_at FROM users WHERE email = ?"
        );
        if (!stmt) return nullptr;
        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        UserRow* row = nullptr;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            row = new UserRow{
                sqlite3_column_int(stmt, 0),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))
            };
        }
        sqlite3_finalize(stmt);
        return row;
    }

    UserRow* find_user_by_id(int id) {
        sqlite3_stmt* stmt = _prepare(
            "SELECT id, email, password_hash, created_at FROM users WHERE id = ?"
        );
        if (!stmt) return nullptr;
        sqlite3_bind_int(stmt, 1, id);
        UserRow* row = nullptr;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            row = new UserRow{
                sqlite3_column_int(stmt, 0),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))
            };
        }
        sqlite3_finalize(stmt);
        return row;
    }

    // ── Tokens ──

    bool save_token(const string& token, int user_id) {
        sqlite3_stmt* stmt = _prepare(
            "INSERT INTO tokens (token, user_id) VALUES (?, ?)"
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
            "SELECT rp.room_id, rp.user_id, rp.ready, u.email "
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

    void _migrate() {
        _exec(R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                email TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                created_at TEXT DEFAULT (datetime('now'))
            );
        )");
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

        // Seed default test user (password: 000000)
        string test_salt = "speedmath_seed";
        stringstream test_hash;
        test_hash << test_salt << "$" << hex << _fnv1a(test_salt + ":000000");
        _exec("INSERT OR IGNORE INTO users (email, password_hash) VALUES ('test@speedmath.local', '" + test_hash.str() + "')");

        cerr << "[DB] Migration complete" << endl;
    }

private:
    // FNV-1a 哈希 — 用于种子密码 (yòng yú zhǒngzǐ mìmǎ — for seeding passwords)
    static size_t _fnv1a(const string& s) {
        size_t h = 14695981039346656037ULL;
        for (char c : s) { h ^= static_cast<size_t>(c); h *= 1099511628211ULL; }
        return h;
    }
};
