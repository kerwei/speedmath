// 多人游戏会话 (duōrén yóuxì huìhuà — multiplayer game session)
// Manages the game loop: questions → answers → leaderboard → next question

#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include "../libspeedmath/arithmetic.h"
#include "../libspeedmath/manager.h"

using namespace std;

struct MqPlayerState {
    int user_id;
    string username;
    int correct = 0;
    long cumulative_ms = 0;
    long q_time_ms = 0;    // time for current question (ms)
    bool answered = false;
    string answer;
};

struct MqQuestion {
    string text;
    Op op;
    int x, y;
    int correct_div_q = 0;  // for DIV: quotient
    int correct_div_r = 0;  // for DIV: remainder
};

// 游戏阶段 (yóuxì jiēduàn — game phase)
enum class GamePhase { Playing, Evaluating, Finished };

class GameSession {
public:
    GameSession(int room_id, int diff, int intense, const vector<Op>& ops)
        : _room_id(room_id), _diff(diff), _intense(intense), _ops(ops),
          _total_q(intense * 10), _current_q(-1), _phase(GamePhase::Playing)
    {
        std::srand(time(nullptr));
        _generate_questions();
    }

    int room_id() const { return _room_id; }
    int total_q() const { return _total_q; }
    int current_q() const { return _current_q; }
    GamePhase phase() const { return _phase; }
    const vector<MqPlayerState>& players() const { return _players; }
    const MqQuestion& question(int idx) const { return _questions[idx]; }

    // Add a player to the game (called when game starts)
    void add_player(int uid, const string& username) {
        _players.push_back({uid, username, 0, 0, 0, false, ""});
    }

    // Returns the next question JSON, or empty if game is finished
    string next_question_json() {
        _current_q++;
        if (_current_q >= _total_q) {
            _phase = GamePhase::Finished;
            return "";
        }

        // Reset player states for new question
        for (auto& p : _players) { p.answered = false; p.answer = ""; p.q_time_ms = 0; }
        _question_start = time(nullptr);
        _phase = GamePhase::Playing;

        auto& q = _questions[_current_q];
        stringstream ss;
        ss << "{\"type\":\"question\""
           << ",\"q_num\":" << (_current_q + 1)
           << ",\"total\":" << _total_q
           << ",\"question\":\"" << q.text << "\""
           << "}";
        return ss.str();
    }

    // Record a player's answer and return whether all have answered
    bool submit_answer(int uid, const string& answer) {
        for (auto& p : _players) {
            if (p.user_id == uid && !p.answered) {
                p.answered = true;
                p.answer = answer;
                long elapsed = (time(nullptr) - _question_start) * 1000;
                p.q_time_ms = elapsed;
                break;
            }
        }
        // Check if all answered
        for (auto& p : _players) {
            if (!p.answered) return false;
        }
        return true;  // All have answered
    }

    // 超时处理 (chāoshí chǔlǐ — mark unanswered players as timed out)
    void timeout_unanswered() {
        for (auto& p : _players) {
            if (!p.answered) {
                p.answered = true;
                p.answer = "";
            }
        }
    }

    // Grade all answers and return leaderboard JSON
    string grade_and_leaderboard_json() {
        auto& q = _questions[_current_q];
        for (auto& p : _players) {
            bool correct = false;
            if (q.op == Op::DIV) {
                size_t space = p.answer.find(' ');
                if (space != string::npos) {
                    int aq = stoi(p.answer.substr(0, space));
                    int ar = stoi(p.answer.substr(space + 1));
                    correct = (aq == q.correct_div_q && ar == q.correct_div_r);
                } else {
                    // Accept plain quotient when remainder is 0
                    if (!p.answer.empty() && isNumber(p.answer)) {
                        correct = (stoi(p.answer) == q.correct_div_q && q.correct_div_r == 0);
                    }
                }
            } else {
                correct = (!p.answer.empty() && isNumber(p.answer) && stoi(p.answer) == _correct_answer(q));
            }

            if (correct) {
                p.correct++;
                p.cumulative_ms += p.q_time_ms;
            } else {
                // Wrong answer: add penalty (slowest correct time + PENALTY_MS)
                long slowest_correct = 0;
                for (auto& op : _players) {
                    // This is a simplification — in the real game we compute per-question
                    // For now, just add the max correct time
                }
                p.cumulative_ms += p.q_time_ms + Manager::PENALTY_MS;
            }
        }

        _phase = GamePhase::Evaluating;
        return _build_leaderboard_json();
    }

    // Build leaderboard JSON (sorted by cumulative time)
    string leaderboard_json() const {
        return _build_leaderboard_json();
    }

    // Full results JSON for game over
    string results_json() const {
        stringstream ss;
        ss << "{\"type\":\"game_over\",\"players\":[";
        vector<int> order = _sorted_order();
        for (size_t i = 0; i < order.size(); i++) {
            if (i > 0) ss << ",";
            auto& p = _players[order[i]];
            ss << "{\"user_id\":" << p.user_id
               << ",\"username\":\"" << p.username << "\""
               << ",\"correct\":" << p.correct
               << ",\"total\":" << _total_q
               << ",\"time_ms\":" << p.cumulative_ms << "}";
        }
        ss << "]}";
        return ss.str();
    }

private:
    int _room_id, _diff, _intense, _total_q, _current_q;
    vector<Op> _ops;
    vector<MqQuestion> _questions;
    vector<MqPlayerState> _players;
    GamePhase _phase;
    time_t _question_start;

    void _generate_questions() {
        // Use function pointers from arithmetic.h
        auto fcnPtr = (_diff == 1) ? &random_one_digit : &random_ge_two_digit;

        for (int i = 0; i < _total_q; i++) {
            Op op = _ops[std::rand() % _ops.size()];
            int x, y;
            if (op == Op::DIV) {
                int divisor = fcnPtr(_diff);
                while (divisor == 0) divisor = fcnPtr(_diff);
                int q = fcnPtr(_diff);
                int r = std::rand() % divisor;
                x = divisor * q + r;
                y = divisor;
            } else {
                x = fcnPtr(_diff);
                y = fcnPtr(_diff);
            }

            stringstream ss;
            ss << x << " " << op_symbol(op) << " " << y;
            int cq = 0, cr = 0;
            if (op == Op::DIV) { cq = x / y; cr = x % y; }
            _questions.push_back({ss.str(), op, x, y, cq, cr});
        }
    }

    int _correct_answer(const MqQuestion& q) const {
        switch (q.op) {
            case Op::ADD: return q.x + q.y;
            case Op::SUB: return q.x - q.y;
            case Op::MUL: return q.x * q.y;
            default: return 0;
        }
    }

    vector<int> _sorted_order() const {
        vector<int> order(_players.size());
        for (size_t i = 0; i < order.size(); i++) order[i] = i;
        sort(order.begin(), order.end(), [this](int a, int b) {
            return _players[a].cumulative_ms < _players[b].cumulative_ms;
        });
        return order;
    }

    string _build_leaderboard_json() const {
        stringstream ss;
        ss << "{\"type\":\"leaderboard\",\"q_num\":" << (_current_q + 1);
        ss << ",\"total\":" << _total_q;
        ss << ",\"phase\":\"" << (_phase == GamePhase::Evaluating ? "evaluation" : "playing") << "\"";
        ss << ",\"players\":[";
        vector<int> order = _sorted_order();
        for (size_t i = 0; i < order.size(); i++) {
            if (i > 0) ss << ",";
            auto& p = _players[order[i]];
            ss << "{\"user_id\":" << p.user_id
               << ",\"username\":\"" << p.username << "\""
               << ",\"correct\":" << p.correct
               << ",\"time_ms\":" << p.cumulative_ms
               << ",\"answered\":" << (p.answered ? "true" : "false")
               << "}";
        }
        ss << "]}";
        return ss.str();
    }
};
