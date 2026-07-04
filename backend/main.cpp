// 后端服务器 (hòuduān fúwùqì — backend server)
// REST API 驱动速算游戏 (qūdòng sùsuàn yóuxì — powering the speed math game)

#include "httplib.h"
#include "../libspeedmath/manager.h"
#include <signal.h>
#include <thread>
#include <unordered_map>
#include <sstream>

using namespace std;

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
        int diff = stoi(param_or(req, "diff", "1"));
        int intense = stoi(param_or(req, "intense", "1"));
        string ops_str = param_or(req, "ops", "1234");
        int ai_level_val = stoi(param_or(req, "ai_level", "0"));

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

        bool ai_enabled = ai_level_val > 0;
        AiLevel ai_level = AiLevel::EASY;
        if (ai_level_val == 2) ai_level = AiLevel::MEDIUM;
        else if (ai_level_val >= 3) ai_level = AiLevel::HARD;

        string sid = generate_id();
        Manager* game = new Manager(diff, intense, ops, ai_level, ai_enabled);
        sessions[sid] = game;

        res.set_content(
            "{\"session_id\":\"" + sid + "\"}",
            "application/json"
        );
    });

    // POST /api/game/question — 下一题 (xià yī tí — next question)
    svr.Post("/api/game/question", [](const httplib::Request& req, httplib::Response& res) {
        string sid = param_or(req, "session_id", "");
        auto it = sessions.find(sid);
        if (it == sessions.end()) {
            res.status = 404;
            res.set_content("{\"error\":\"session not found\"}", "application/json");
            return;
        }

        Manager* game = it->second;
        try {
            string q = game->qnext();
            string json = "{\"question\":\"" + q + "\"";
            if (game->ai_enabled()) {
                json += ",\"ai_answer\":\"" + game->ai_answer() + "\"";
                json += ",\"ai_delay_ms\":" + to_string(game->ai_delay_ms());
            }
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
        auto it = sessions.find(sid);
        if (it == sessions.end()) {
            res.status = 404;
            res.set_content("{\"error\":\"session not found\"}", "application/json");
            return;
        }

        Manager* game = it->second;
        string answer = param_or(req, "answer", "");
        string result = game->grade_answer(answer);

        res.set_content("{\"result\":\"" + result + "\"}", "application/json");
    });

    // GET /api/game/results — 最终结果 (zuìzhōng jiéguǒ — final results)
    svr.Get("/api/game/results", [](const httplib::Request& req, httplib::Response& res) {
        string sid = param_or(req, "session_id", "");
        auto it = sessions.find(sid);
        if (it == sessions.end()) {
            res.status = 404;
            res.set_content("{\"error\":\"session not found\"}", "application/json");
            return;
        }

        Manager* game = it->second;
        game->updatescore();
        string output = game->print_results();
        game->savehighscore();

        delete game;
        sessions.erase(sid);

        string escaped;
        for (char c : output) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\n') escaped += "\\n";
            else escaped += c;
        }

        res.set_content(
            "{\"results\":\"" + escaped + "\"}",
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
