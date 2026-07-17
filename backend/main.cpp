// 后端服务器 (hòuduān fúwùqì — backend server)
// REST API 驱动速算游戏 (qūdòng sùsuàn yóuxì — powering the speed math game)

#include "httplib.h"
#include "../libspeedmath/manager.h"
#include "db.h"
#include "game_session.h"
#include <signal.h>
#include <thread>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include <ctime>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <random>

using namespace std;

// 简单日志 (jiǎndān rìzhì — simple logger)
static void log(const string& method, const string& path, const string& info = "") {
    time_t now = time(nullptr);
    char buf[20];
    strftime(buf, sizeof buf, "%H:%M:%S", localtime(&now));
    cerr << "[" << buf << "] " << method << " " << path;
    if (!info.empty()) cerr << " " << info;
    cerr << endl;
}

// 会话存储 (huìhuà chǔncún — session storage)
// key = session ID, value = Manager*
static unordered_map<string, Manager*> sessions;
static httplib::Server* g_svr = nullptr;

static string generate_id() {
    stringstream ss;
    ss << std::hex << std::rand();
    return ss.str();
}

// ── Database singleton ── 数据库单例 (shùjùkù dānlì)
static Db db("speedmath.db");  // 速算数据库 (sùsuàn shùjùkù — speedmath database)

// ── Password & auth helpers ── 密码哈希和令牌工具 (mìmǎ hāxī hé lìngpái gōngjù)
// Generates a random alphanumeric salt string
static string random_salt(size_t len = 16) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    string s;
    srand(time(nullptr) ^ (rand() << 16));
    for (size_t i = 0; i < len; i++) s += chars[rand() % (sizeof(chars) - 1)];
    return s;
}

// FNV-1a 哈希 — 跨调用/跨平台结果一致 (kuà diàoyòng/kuà píngtái jiéguǒ yīzhì)
// Deterministic hash: same input always gives the same result
static size_t fnv1a(const string& s) {
    size_t h = 14695981039346656037ULL;
    for (char c : s) { h ^= static_cast<size_t>(c); h *= 1099511628211ULL; }
    return h;
}

// 加盐哈希密码 (jiā yán hāxī mìmǎ — salted password hash)
static string hash_password(const string& pw, const string& salt) {
    stringstream ss;
    ss << salt << "$" << hex << fnv1a(salt + ":" + pw);
    return ss.str();
}

// 验证密码 (yànzhèng mìmǎ — verify password against stored hash)
static bool verify_password(const string& pw, const string& stored) {
    size_t dollar = stored.find('$');
    if (dollar == string::npos) return false;
    return hash_password(pw, stored.substr(0, dollar)) == stored;
}

// 生成64字符令牌 (shēngchéng 64 zìfú lìngpái — generate 64-char auth token)
static string generate_token() {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static random_device rd;
    string s;
    for (int i = 0; i < 64; i++) s += chars[rd() % (sizeof(chars) - 1)];
    return s;
}

// 认证中间件 (rènzhèng zhōngjiānjiàn — auth middleware)
// Extracts user_id from Bearer token in Authorization header
static int require_auth(const httplib::Request& req, httplib::Response& res) {
    string auth = req.get_header_value("Authorization", "");
    if (auth.substr(0, 7) != "Bearer ") {
        res.status = 401;
        res.set_content("{\"error\":\"missing auth\"}", "application/json");
        return -1;
    }
    int uid = db.find_user_by_token(auth.substr(7));
    if (uid < 0) {
        res.status = 401;
        res.set_content("{\"error\":\"invalid token\"}", "application/json");
        return -1;
    }
    return uid;
}

// ── 房间码生成器 (fángjiān mǎ shēngchéng qì — room code generator) ──
// 4 chars, uppercase + digits, excludes confusable I/O/0/1
static string make_room_code() {
    static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    string code;
    for (int i = 0; i < 4; i++) code += chars[rand() % (sizeof(chars) - 1)];
    return code;
}

// ── WebSocket 房间状态 (fángjiān zhuàngtài — in-memory room state) ──
// These hold the live WebSocket connections for each room.
struct WsPlayer {
    int user_id;
    string username;
    httplib::ws::WebSocket* ws;  // non-owning pointer
    bool ready = false;
};

struct WsRoom {
    int room_id;
    string code;
    int host_id;
    string status = "waiting";
    vector<shared_ptr<WsPlayer>> players;
    mutex mtx;
};

static unordered_map<int, shared_ptr<WsRoom>> ws_rooms;
static mutex g_ws_mtx;

// 游戏会话 (yóuxì huìhuà — live game sessions per room)
static unordered_map<int, shared_ptr<GameSession>> game_sessions;

// Broadcast a JSON message to all players in a WsRoom (caller must hold r->mtx)
static void ws_broadcast(shared_ptr<WsRoom>& r, const string& json) {
    for (auto& p : r->players) {
        if (p->ws && p->ws->is_open()) p->ws->send(json);
    }
}

// Build a JSON string describing the room state (players + ready states)
static string ws_room_json(shared_ptr<WsRoom>& r) {
    string j = "{\"type\":\"room_state\",\"room_id\":" + to_string(r->room_id);
    j += ",\"code\":\"" + r->code + "\",\"status\":\"" + r->status + "\"";
    j += ",\"host_id\":" + to_string(r->host_id) + ",\"players\":[";
    for (size_t i = 0; i < r->players.size(); i++) {
        if (i > 0) j += ",";
        auto& p = r->players[i];
        j += "{\"user_id\":" + to_string(p->user_id);
        j += ",\"username\":\"" + p->username + "\"";
        j += ",\"ready\":" + string(p->ready ? "true" : "false") + "}";
    }
    j += "]}";
    return j;
}

// Find which room a user is in (searches all rooms — call with g_ws_mtx held)
static shared_ptr<WsRoom> find_room_by_user(int uid) {
    for (auto& [id, r] : ws_rooms) {
        for (auto& p : r->players) {
            if (p->user_id == uid) return r;
        }
    }
    return nullptr;
}

// 辅助函数 (fǔzhù hánshù — helper): get param with fallback
static string param_or(const httplib::Request& req, const string& name, const string& fallback) {
    if (req.has_param(name)) {
        return req.get_param_value(name);
    }
    return fallback;
}

// 信号等待线程 (xìnhào děngdài xiànchéng — signal wait thread)
// Reliable approach: signals are delivered via sigwait() instead of a handler
static void signal_thread() {
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTERM);

    int sig = 0;
    sigwait(&sigs, &sig);
    std::cout << "\n收到关闭信号 (shōudào guānbì xìnhào — Shutdown received)" << std::endl;
    if (g_svr) {
        g_svr->stop();
    }
}

int main() {
    httplib::Server svr;
    g_svr = &svr;

    // CORS 支持 (zhīchí — support) for frontend dev server
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });

    svr.set_websocket_ping_interval(10);  // 10s ping for WebSocket keepalive

    svr.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 204;
    });

    // ── Auth endpoints ──

    // POST /api/auth/register — 注册 (zhùcè — register)
    svr.Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        string email = param_or(req, "email", "");
        string password = param_or(req, "password", "");
        log("POST", "/api/auth/register", "email=" + email);

        if (email.empty() || password.size() < 4) {
            res.status = 400;
            res.set_content("{\"error\":\"invalid input\"}", "application/json");
            return;
        }

        if (db.find_user_by_email(email)) {
            res.status = 409;
            res.set_content("{\"error\":\"email taken\"}", "application/json");
            return;
        }

        if (!db.create_user(email, hash_password(password, random_salt()))) {
            res.status = 500;
            res.set_content("{\"error\":\"create failed\"}", "application/json");
            return;
        }

        UserRow* u = db.find_user_by_email(email);
        string token = generate_token();
        int uid = u->id;
        db.save_token(token, uid);
        delete u;

        res.set_content("{\"token\":\"" + token + "\",\"user_id\":" + to_string(uid) + "}", "application/json");
        log("POST", "/api/auth/register", "email=" + email + " uid=" + to_string(uid));
    });

    // POST /api/auth/login — 登录 (dēnglù — login)
    svr.Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        string email = param_or(req, "email", "");
        string password = param_or(req, "password", "");
        log("POST", "/api/auth/login", "email=" + email);

        if (email.empty() || password.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"invalid input\"}", "application/json");
            return;
        }

        UserRow* u = db.find_user_by_email(email);
        if (!u || !verify_password(password, u->password_hash)) {
            res.status = 401;
            res.set_content("{\"error\":\"invalid credentials\"}", "application/json");
            delete u;
            return;
        }

        string token = generate_token();
        int uid = u->id;
        db.save_token(token, uid);
        delete u;

        res.set_content("{\"token\":\"" + token + "\",\"user_id\":" + to_string(uid) + "}", "application/json");
    });

    // ── Room endpoints ──

    // POST /api/room/create — 创建房间 (chuàngjiàn fángjiān — create room)
    svr.Post("/api/room/create", [](const httplib::Request& req, httplib::Response& res) {
        int uid = require_auth(req, res);
        if (uid < 0) return;

        // Generate unique code
        string code;
        do { code = make_room_code(); } while (db.find_room_by_code(code));

        int room_id = db.create_room(code, uid);
        if (room_id < 0) {
            res.status = 500;
            res.set_content("{\"error\":\"create failed\"}", "application/json");
            return;
        }

        db.add_room_player(room_id, uid);
        // Create in-memory WsRoom for WebSocket state
        {
            lock_guard<mutex> lock(g_ws_mtx);
            auto room = make_shared<WsRoom>();
            room->room_id = room_id;
            room->code = code;
            room->host_id = uid;
            ws_rooms[room_id] = room;
        }
        log("POST", "/api/room/create", "code=" + code + " host=" + to_string(uid));
        res.set_content("{\"room_id\":" + to_string(room_id) + ",\"code\":\"" + code + "\"}", "application/json");
    });

    // POST /api/room/join — 加入房间 (jiārù fángjiān — join room)
    svr.Post("/api/room/join", [](const httplib::Request& req, httplib::Response& res) {
        int uid = require_auth(req, res);
        if (uid < 0) return;

        string code = param_or(req, "code", "");
        if (code.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"missing code\"}", "application/json");
            return;
        }

        RoomRow* room = db.find_room_by_code(code);
        if (!room) {
            res.status = 404;
            res.set_content("{\"error\":\"room not found\"}", "application/json");
            return;
        }

        if (room->status != "waiting") {
            res.status = 400;
            res.set_content("{\"error\":\"room not joinable\"}", "application/json");
            delete room;
            return;
        }

        db.add_room_player(room->id, uid);
        log("POST", "/api/room/join", "code=" + code + " user=" + to_string(uid));
        res.set_content("{\"room_id\":" + to_string(room->id) + ",\"code\":\"" + code + "\"}", "application/json");
        delete room;
    });

    // POST /api/room/leave — 离开房间 (líkāi fángjiān — leave room)
    svr.Post("/api/room/leave", [](const httplib::Request& req, httplib::Response& res) {
        int uid = require_auth(req, res);
        if (uid < 0) return;

        int room_id = stoi(param_or(req, "room_id", "-1"));
        if (room_id < 0) { res.status = 400; res.set_content("{\"error\":\"invalid room\"}", "application/json"); return; }

        db.remove_room_player(room_id, uid);
        // If no players left, delete the room
        auto remaining = db.get_room_players(room_id);
        if (remaining.empty()) db.delete_room(room_id);

        log("POST", "/api/room/leave", "room=" + to_string(room_id) + " user=" + to_string(uid));
        res.set_content("{\"ok\":true}", "application/json");
    });

    // POST /api/room/ready — 准备/取消准备 (zhǔnbèi — toggle ready)
    svr.Post("/api/room/ready", [](const httplib::Request& req, httplib::Response& res) {
        int uid = require_auth(req, res);
        if (uid < 0) return;

        int room_id = stoi(param_or(req, "room_id", "-1"));
        bool ready = param_or(req, "ready", "true") == "true";
        if (room_id < 0) { res.status = 400; res.set_content("{\"error\":\"invalid room\"}", "application/json"); return; }

        db.set_player_ready(room_id, uid, ready);
        log("POST", "/api/room/ready", "room=" + to_string(room_id) + " user=" + to_string(uid) + " ready=" + (ready ? "1" : "0"));
        res.set_content("{\"ok\":true}", "application/json");
    });

    // GET /api/room/status — 房间状态 (fángjiān zhuàngtài — room status)
    svr.Get("/api/room/status", [](const httplib::Request& req, httplib::Response& res) {
        int uid = require_auth(req, res);
        if (uid < 0) return;

        int room_id = stoi(param_or(req, "room_id", "-1"));
        if (room_id < 0) { res.status = 400; res.set_content("{\"error\":\"invalid room\"}", "application/json"); return; }

        RoomRow* room = nullptr;
        for (int attempt = 0; attempt < 2; attempt++) {
            room = db.find_room_by_id(room_id);
            if (room) break;
            // Maybe room_id is actually a code
            room = db.find_room_by_code(to_string(room_id));
            if (room) break;
        }
        if (!room) {
            res.status = 404;
            res.set_content("{\"error\":\"room not found\"}", "application/json");
            return;
        }

        auto players = db.get_room_players(room->id);
        string players_json = "[";
        for (size_t i = 0; i < players.size(); i++) {
            if (i > 0) players_json += ",";
            players_json += "{\"user_id\":" + to_string(players[i].user_id);
            players_json += ",\"username\":\"" + players[i].username + "\"";
            players_json += ",\"ready\":" + string(players[i].ready ? "true" : "false") + "}";
        }
        players_json += "]";

        res.set_content(
            "{\"room_id\":" + to_string(room->id)
            + ",\"code\":\"" + room->code + "\""
            + ",\"status\":\"" + room->status + "\""
            + ",\"host_id\":" + to_string(room->host_id)
            + ",\"players\":" + players_json + "}",
            "application/json"
        );
        delete room;
    });

    // ── WebSocket 端点 (WebSocket duān diǎn — real-time room communication) ──
    svr.WebSocket("/api/ws", [](const httplib::Request& req, httplib::ws::WebSocket& ws) {
        string token = req.get_param_value("token");
        int uid = db.find_user_by_token(token);
        if (uid < 0) { ws.close(); return; }

        string rid_str = req.get_param_value("room_id");
        int room_id = rid_str.empty() ? -1 : stoi(rid_str);
        if (room_id < 0) { ws.close(); return; }

        // Verify user is in this room via DB
        auto db_players = db.get_room_players(room_id);
        bool authorized = false;
        string username;
        for (auto& p : db_players) {
            if (p.user_id == uid) { authorized = true; username = p.username; break; }
        }
        if (!authorized) { ws.close(); return; }

        // Get or create WsRoom
        shared_ptr<WsRoom> wsr;
        {
            lock_guard<mutex> lock(g_ws_mtx);
            auto it = ws_rooms.find(room_id);
            if (it == ws_rooms.end()) { ws.close(); return; }
            wsr = it->second;
        }

        // Add player to room
        shared_ptr<WsPlayer> my_player;
        {
            lock_guard<mutex> lock(wsr->mtx);
            // Reuse if reconnecting
            for (auto& p : wsr->players) {
                if (p->user_id == uid) { my_player = p; break; }
            }
            if (!my_player) {
                my_player = make_shared<WsPlayer>();
                my_player->user_id = uid;
                my_player->username = username;
                wsr->players.push_back(my_player);
            }
            my_player->ws = &ws;
            ws_broadcast(wsr, ws_room_json(wsr));
        }

        // Read loop
        string msg;
        while (ws.read(msg) != httplib::ws::ReadResult::Fail) {
            if (msg.find("\"type\":\"ready\"") != string::npos) {
                lock_guard<mutex> lock(wsr->mtx);
                my_player->ready = (msg.find("\"ready\":true") != string::npos);
                ws_broadcast(wsr, ws_room_json(wsr));
            } else if (msg.find("\"type\":\"leave\"") != string::npos) {
                break;
            } else if (msg.find("\"type\":\"start\"") != string::npos) {
                if (uid != wsr->host_id) continue;

                // Parse game config from JSON
                int diff = 1, intense = 1;
                string ops_str = "1234";
                auto get_val = [&](const string& key) -> string {
                    auto p = msg.find("\"" + key + "\":");
                    if (p == string::npos) return "";
                    p = msg.find(':', p) + 1;
                    while (p < msg.size() && (msg[p] == ' ' || msg[p] == '"')) p++;
                    string v;
                    while (p < msg.size() && msg[p] != '"' && msg[p] != ',' && msg[p] != '}') v += msg[p++];
                    return v;
                };
                string ds = get_val("diff");
                if (!ds.empty()) diff = stoi(ds);
                string is = get_val("intense");
                if (!is.empty()) intense = stoi(is);
                string os = get_val("ops");
                if (!os.empty()) ops_str = os;

                vector<Op> ops;
                for (char c : ops_str) {
                    switch (c) {
                        case '1': ops.push_back(Op::ADD); break;
                        case '2': ops.push_back(Op::SUB); break;
                        case '3': ops.push_back(Op::MUL); break;
                        case '4': ops.push_back(Op::DIV); break;
                    }
                }
                if (ops.empty()) ops.push_back(Op::ADD);

                // Create game session
                auto gs = make_shared<GameSession>(room_id, diff, intense, ops);
                {
                    lock_guard<mutex> lock(wsr->mtx);
                    for (auto& p : wsr->players) {
                        gs->add_player(p->user_id, p->username);
                    }
                    game_sessions[room_id] = gs;
                    wsr->status = "playing";
                    ws_broadcast(wsr, "{\"type\":\"game_start\"}");
                }

                // Game loop in a separate thread
                 thread([wsr, gs, room_id]() mutable {
                    string qjson;
                    while ((qjson = gs->next_question_json()) != "") {
                        // Broadcast question
                        {
                            lock_guard<mutex> lock(wsr->mtx);
                            ws_broadcast(wsr, qjson);
                        }
                        // Wait for all players to answer (poll every 100ms, timeout 15s)
                        int total = gs->players().size();
                        int answered = 0, wait_loops = 0;
                        const int MAX_WAIT = 150;  // 150 * 100ms = 15 seconds
                        do {
                            this_thread::sleep_for(100ms);
                            answered = 0;
                            for (auto& p : gs->players()) {
                                if (p.answered) answered++;
                            }
                            wait_loops++;
                            // Timeout: mark remaining players as answered (no answer → wrong)
                            if (wait_loops >= MAX_WAIT && answered < total) {
                                gs->timeout_unanswered();
                                break;
                            }
                        } while (answered < total);

                        // All answered — grade and broadcast leaderboard
                        string lb = gs->grade_and_leaderboard_json();
                        {
                            lock_guard<mutex> lock(wsr->mtx);
                            // Include each player's answer in the broadcast
                            string full_lb = lb.substr(0, lb.size() - 1); // remove trailing }
                            full_lb += ",\"answers\":[";
                            for (size_t i = 0; i < gs->players().size(); i++) {
                                if (i > 0) full_lb += ",";
                                full_lb += "{\"user_id\":" + to_string(gs->players()[i].user_id);
                                full_lb += ",\"answer\":\"" + gs->players()[i].answer + "\"}";
                            }
                            full_lb += "]}";
                            ws_broadcast(wsr, full_lb);
                        }

                        // Pause between questions
                        this_thread::sleep_for(2s);
                    }

                    // Game over
                    {
                        lock_guard<mutex> lock(wsr->mtx);
                        ws_broadcast(wsr, gs->results_json());
                        wsr->status = "finished";
                    }
                }).detach();

            } else if (msg.find("\"type\":\"answer\"") != string::npos) {
                // Parse answer from JSON
                auto ap = msg.find("\"answer\":\"");
                if (ap != string::npos) {
                    ap += 10; // skip past "answer":"
                    string ans;
                    while (ap < msg.size() && msg[ap] != '"') ans += msg[ap++];
                    auto gs_it = game_sessions.find(room_id);
                    if (gs_it != game_sessions.end()) {
                        gs_it->second->submit_answer(uid, ans);
                    }
                }
            }
        }

        // Disconnect — remove player from room
        {
            lock_guard<mutex> lock(wsr->mtx);
            for (auto it = wsr->players.begin(); it != wsr->players.end(); ++it) {
                if ((*it)->user_id == uid) { wsr->players.erase(it); break; }
            }
            if (!wsr->players.empty()) {
                ws_broadcast(wsr, ws_room_json(wsr));
            }
        }
    });

    // POST /api/game/new — 创建新游戏 (chuàngjiàn xīn yóuxì — create new game)
    svr.Post("/api/game/new", [](const httplib::Request& req, httplib::Response& res) {
        log("POST", "/api/game/new", "diff=" + param_or(req, "diff", "1") + " intense=" + param_or(req, "intense", "1") + " ai_levels=" + param_or(req, "ai_levels", "0"));
        int diff = stoi(param_or(req, "diff", "1"));
        int intense = stoi(param_or(req, "intense", "1"));
        string ops_str = param_or(req, "ops", "1234");

        // Parse AI levels: support both "ai_level" (single) and "ai_levels" (comma-separated)
        vector<AiLevel> ai_levels;
        string ai_levels_str = param_or(req, "ai_levels", "");
        if (ai_levels_str.empty()) {
            // Fallback to single ai_level param
            int ai_level_val = stoi(param_or(req, "ai_level", "0"));
            if (ai_level_val > 0) {
                AiLevel lvl = AiLevel::EASY;
                if (ai_level_val == 2) lvl = AiLevel::MEDIUM;
                else if (ai_level_val >= 3) lvl = AiLevel::HARD;
                ai_levels.push_back(lvl);
            }
        } else {
            // Comma-separated: "1,2,3" = Easy, Medium, Hard
            size_t start = 0, end;
            while ((end = ai_levels_str.find(',', start)) != string::npos) {
                int v = stoi(ai_levels_str.substr(start, end - start));
                if (v >= 1 && v <= 3) {
                    ai_levels.push_back(static_cast<AiLevel>(v - 1));
                }
                start = end + 1;
            }
            if (start < ai_levels_str.size()) {
                int v = stoi(ai_levels_str.substr(start));
                if (v >= 1 && v <= 3) {
                    ai_levels.push_back(static_cast<AiLevel>(v - 1));
                }
            }
        }

        vector<Op> ops;
        for (char c : ops_str) {
            switch (c) {
                case '1': ops.push_back(Op::ADD); break;
                case '2': ops.push_back(Op::SUB); break;
                case '3': ops.push_back(Op::MUL); break;
                case '4': ops.push_back(Op::DIV); break;
            }
        }
        if (ops.empty()) {
            ops.push_back(Op::ADD);
        }

        string sid = generate_id();
        Manager* game = new Manager(diff, intense, ops, ai_levels);
        sessions[sid] = game;

        res.set_content(
            "{\"session_id\":\"" + sid + "\"}",
            "application/json"
        );
    });

    // POST /api/game/question — 下一题 (xià yī tí — next question)
    svr.Post("/api/game/question", [](const httplib::Request& req, httplib::Response& res) {
        string sid = param_or(req, "session_id", "");
        log("POST", "/api/game/question", "sid=" + sid.substr(0, 8) + "...");
        auto it = sessions.find(sid);
        if (it == sessions.end()) {
            log("WARN", "session not found", "sid=" + sid.substr(0, 8) + "...");
            res.status = 404;
            res.set_content("{\"error\":\"session not found\"}", "application/json");
            return;
        }

        Manager* game = it->second;
        try {
            string q = game->qnext();
            string json = "{\"question\":\"" + q + "\"";
            if (game->ai_enabled()) {
                json += ",\"ai_answers\":[";
                for (int i = 0; i < game->ai_count(); i++) {
                    if (i > 0) json += ",";
                    json += "\"" + game->ai_answer(i) + "\"";
                }
                json += "],\"ai_delays\":[";
                for (int i = 0; i < game->ai_count(); i++) {
                    if (i > 0) json += ",";
                    json += to_string(game->ai_delay_ms(i));
                }
                json += "]";
            }
            // Include current standings (updated from previous answer)
            json += ",\"standings\":[";
            for (int i = 0; i < game->player_count(); i++) {
                if (i > 0) json += ",";
                json += "{\"t\":" + to_string(game->player_cumulative_time(i));
                json += ",\"c\":" + to_string(game->player_correct_count(i));
                json += "}";
            }
            json += "],\"questions\": " + to_string(game->questions_answered());
            json += "}";
            res.set_content(json, "application/json");
        }
        catch (const out_of_range&) {
            res.set_content("{\"question\":\"\",\"finished\":true}", "application/json");
        }
    });

    // POST /api/game/answer — 提交答案 (tíjiāo dá'àn — submit answer)
    svr.Post("/api/game/answer", [](const httplib::Request& req, httplib::Response& res) {
        string sid = param_or(req, "session_id", "");
        log("POST", "/api/game/answer", "sid=" + sid.substr(0, 8) + "... answer=" + param_or(req, "answer", ""));
        auto it = sessions.find(sid);
        if (it == sessions.end()) {
            log("WARN", "session not found (answer)", "sid=" + sid.substr(0, 8) + "...");
            res.status = 404;
            res.set_content("{\"error\":\"session not found\"}", "application/json");
            return;
        }

        Manager* game = it->second;
        string answer = param_or(req, "answer", "");
        string result = game->grade_answer(answer);

        log("POST", "/api/game/answer", "result=" + result);
        res.set_content("{\"result\":\"" + result + "\"}", "application/json");
    });

    // GET /api/game/results
    svr.Get("/api/game/results", [](const httplib::Request& req, httplib::Response& res) {
        string sid = param_or(req, "session_id", "");
        log("GET", "/api/game/results", "sid=" + sid.substr(0, 8) + "...");
        auto it = sessions.find(sid);
        if (it == sessions.end()) {
            log("WARN", "session not found (results)", "sid=" + sid.substr(0, 8) + "...");
            res.status = 404;
            res.set_content("{\"error\":\"session not found\"}", "application/json");
            return;
        }

        Manager* game = it->second;
        game->updatescore();
        game->savehighscore();

        int total_q = game->questions_answered();
        int n = game->player_count();

        // Build players JSON array
        string players_json = "[";
        for (int i = 0; i < n; i++) {
            if (i > 0) players_json += ",";
            string type = (i == 0) ? "\"human\"" : "\"ai\"";
            string level_str = "null";
            if (i > 0) {
                level_str = to_string(static_cast<int>(game->ai_level(i - 1)));
            }
            players_json += "{";
            players_json += "\"idx\":" + to_string(i) + ",";
            players_json += "\"type\":" + type + ",";
            players_json += "\"level\":" + level_str + ",";
            players_json += "\"correct\":" + to_string(game->player_correct_count(i)) + ",";
            players_json += "\"total\":" + to_string(total_q) + ",";
            players_json += "\"time_ms\":" + to_string(game->player_cumulative_time(i));
            players_json += "}";
        }
        players_json += "]";

        delete game;
        sessions.erase(sid);

        res.set_content(
            "{\"players\":" + players_json + "}",
            "application/json"
        );
    });

    // 静态文件服务 (jìngtài wénjiàn fúwù — serve static frontend files)
    svr.set_mount_point("/", "frontend/dist");

    // Block signals in the main thread — they'll be handled by sigwait thread
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sigs, nullptr);

    // Start the signal waiting thread
    thread st(signal_thread);

    std::cout << "服务器已启动 (fúwùqì yǐ qǐdòng — Server started): http://localhost:8080" << std::endl;
    std::cout << "按 Ctrl+C 停止 (àn Ctrl+C tíngzhǐ — press Ctrl+C to stop)" << std::endl;
    svr.listen("0.0.0.0", 8080);

    st.join();
    return 0;
}
