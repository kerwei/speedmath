// 后端服务器 (hòuduān fúwùqì — backend server)
// REST API 驱动速算游戏 (qūdòng sùsuàn yóuxì — powering the speed math game)

#include "httplib.h"
#include "../libspeedmath/manager.h"
#include <unordered_map>
#include <sstream>

using namespace std;

// 会话存储 (huìhuà chǔncún — session storage)
// key = session ID, value = Manager*
static unordered_map<string, Manager*> sessions;

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

int main() {
    httplib::Server svr;

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
        Manager* game = new Manager(diff, intense, ops);
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
            res.set_content("{\"question\":\"" + q + "\"}", "application/json");
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

        // 手动构建 JSON: escape newlines and quotes
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

    std::cout << "服务器已启动 (fúwùqì yǐ qǐdòng — Server started): http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
