// 后端服务器 (hòuduān fúwùqì — backend server)
// REST API 驱动速算游戏 (qūdòng sùsuàn yóuxì — powering the speed math game)

#include "httplib.h"
#include "../libspeedmath/manager.h"
#include <signal.h>
#include <thread>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include <ctime>

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
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    svr.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 204;
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
    svr.set_mount_point("/", "../frontend/dist");

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
