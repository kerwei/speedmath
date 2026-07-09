#include <vector>
#include "arithmetic.h"
#include "manager.h"
#include <fstream>
#include <iomanip>
#include <algorithm>


Manager::Manager(const int diff, const int intense, const vector<Op>& ops, const vector<AiLevel>& ai_levels):
    _seed(time(NULL)), _diff(diff), _intense(intense), _ops(ops), _op(Op::ADD),
    _qtotal(10), _elapsed(0),
    x(1), y(1), score(0), _start_time(time(NULL)),
    __row({-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}),
    __lrow({-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}),
    _elapsed_board({__lrow,__lrow,__lrow,__lrow,__lrow,__lrow,__lrow,__lrow,__lrow,__lrow,__lrow}),
    _correct_board({__row,__row,__row,__row,__row,__row,__row,__row,__row,__row,__row}),
    fcnPtr(nullptr)
{
    std::srand(_seed);
    _qtotal = _intense * 10;

    if (diff == 1) {
        fcnPtr = &random_one_digit;
    }
    else {
        fcnPtr = &random_ge_two_digit;
    }

    // Initialize player arrays: index 0 = human, 1+ = AI
    _player_times.push_back(0);  // human
    for (AiLevel lvl : ai_levels) {
        _ais.push_back(new AiOpponent(lvl));
        _ai_answers.push_back("");
        _ai_delays.push_back(0);
        _player_times.push_back(0);   // one per AI
        _ai_correct_counts.push_back(0);
    }
}

string Manager::qnext() {
    if (_qtotal <= 0) throw out_of_range("");

    _op = _ops[std::rand() % _ops.size()];
    if (_op == Op::DIV) {
        int divisor = fcnPtr(_diff);
        while (divisor == 0) { divisor = fcnPtr(_diff); }
        int q = fcnPtr(_diff);
        int r = std::rand() % divisor;
        x = divisor * q + r;
        y = divisor;
    } else {
        x = fcnPtr(_diff);
        y = fcnPtr(_diff);
    }
    _qtotal -= 1;

    // AI opponents decide
    int correct = 0, r = 0;
    switch (_op) {
        case Op::ADD: correct = x + y; break;
        case Op::SUB: correct = x - y; break;
        case Op::MUL: correct = x * y; break;
        case Op::DIV: correct = x / y; r = x % y; break;
    }
    for (size_t i = 0; i < _ais.size(); i++) {
        _ai_answers[i] = _ais[i]->decide_answer(x, y, op_symbol(_op), correct, r);
        _ai_delays[i] = _ais[i]->delay_ms();
    }

    _start_time = time(NULL);
    return to_string(x) + " " + op_symbol(_op) + " " + to_string(y);
}

void Manager::_grade_ai_answers() {
    int correct = 0, r = 0;
    switch (_op) {
        case Op::ADD: correct = x + y; break;
        case Op::SUB: correct = x - y; break;
        case Op::MUL: correct = x * y; break;
        case Op::DIV: correct = x / y; r = x % y; break;
    }
    for (size_t i = 0; i < _ais.size(); i++) {
        bool ai_correct = false;
        if (_op == Op::DIV) {
            size_t space = _ai_answers[i].find(' ');
            if (space != string::npos) {
                int q = stoi(_ai_answers[i].substr(0, space));
                int rem = stoi(_ai_answers[i].substr(space + 1));
                ai_correct = (q == correct && rem == r);
            }
        } else {
            ai_correct = (stoi(_ai_answers[i]) == correct);
        }
        _ais[i]->record(ai_correct, _ai_delays[i]);
        if (ai_correct) { _ai_correct_counts[i]++; }
    }
}

void Manager::_apply_racing_scores(long human_question_time_ms, bool human_correct) {
    int total = 1 + _ais.size();  // human + AIs

    // Collect times and correctness for this question
    vector<long> times(total);
    vector<bool> corrects(total);
    times[0] = human_question_time_ms;
    corrects[0] = human_correct;

    for (int i = 0; i < (int)_ais.size(); i++) {
        times[1 + i] = _ai_delays[i];
        corrects[1 + i] = (_ai_correct_counts[i] > 0);
    }

    // Find slowest correct time
    long slowest_correct = 0;
    for (int i = 0; i < total; i++) {
        if (corrects[i] && times[i] > slowest_correct) {
            slowest_correct = times[i];
        }
    }

    // If nobody got it correct, all get slowest_correct (0) + penalty
    // If there's at least one correct, wrong answers get slowest_correct + penalty
    for (int i = 0; i < total; i++) {
        if (!corrects[i]) {
            times[i] = slowest_correct + PENALTY_MS;
        }
        _player_times[i] += times[i];
    }
}

string Manager::grade_answer(const int answer) {
    _elapsed += (time(NULL) - _start_time);
    bool correct = check_answer(_op, x, y, answer);
    if (correct) { score++; _human_correct_count++; }
    _grade_ai_answers();
    _apply_racing_scores((time(NULL) - _start_time) * 1000, correct);
    _questions_answered++;
    return correct ? "correct" : "wrong";
}

string Manager::grade_answer(const string& answer) {
    long elapsed_ms = (time(NULL) - _start_time) * 1000;
    _elapsed += elapsed_ms / 1000;
    bool human_correct = false;

    if (_op == Op::DIV) {
        size_t space = answer.find(' ');
        if (space != string::npos) {
            if (isNumber(answer.substr(0, space)) && isNumber(answer.substr(space + 1))) {
                int q = stoi(answer.substr(0, space));
                int r = stoi(answer.substr(space + 1));
                human_correct = check_answer(_op, x, y, q, r);
            }
        } else {
            // Concatenated format
            if (isNumber(answer) && answer[0] != '-') {
                bool matched = false;
                for (size_t split = 1; split < answer.size(); split++) {
                    int q = stoi(answer.substr(0, split));
                    int rem = stoi(answer.substr(split));
                    if (check_answer(_op, x, y, q, rem)) {
                        human_correct = true;
                        matched = true;
                        break;
                    }
                }
                // No-remainder: accept plain quotient (e.g. "4" for 12÷3)
                if (!matched && check_answer(_op, x, y, stoi(answer), 0)) {
                    human_correct = true;
                }
            }
        }
    } else {
        if (isNumber(answer)) {
            human_correct = check_answer(_op, x, y, stoi(answer));
        }
    }

    if (human_correct) { score++; _human_correct_count++; }
    _grade_ai_answers();
    _apply_racing_scores(elapsed_ms, human_correct);
    _questions_answered++;
    return human_correct ? "correct" : "wrong";
}

int Manager::player_correct_count(int idx) const {
    if (idx == 0) return _human_correct_count;
    int ai_idx = idx - 1;
    if (ai_idx < (int)_ai_correct_counts.size()) return _ai_correct_counts[ai_idx];
    return 0;
}

int Manager::ai_total_score() const {
    int s = 0;
    for (auto* ai : _ais) s += ai->score();
    return s;
}

long Manager::ai_total_elapsed_ms() const {
    long e = 0;
    for (auto* ai : _ais) e += ai->elapsed_ms();
    return e;
}

string Manager::print_results() {
    _ss << "=== RACE RESULTS (赛车结果) ===" << endl << endl;
    int total = player_count();
    int q_count = _intense * 10;

    // Print each player's stats
    string level_names[] = {"Easy", "Medium", "Hard"};
    for (int i = 0; i < total; i++) {
        string name;
        if (i == 0) {
            name = "Human";
        } else {
            int ai_idx = i - 1;
            string lvl = level_names[static_cast<int>(_ais[ai_idx]->level())];
            name = "AI" + to_string(ai_idx + 1) + " (" + lvl + ")";
        }
        double t_sec = _player_times[i] / 1000.0;
        _ss << name << ": " << player_correct_count(i) << "/" << q_count
            << ", Time: " << fixed << setprecision(1) << t_sec << "s" << endl;
    }

    // Sort by cumulative time to determine leaderboard
    vector<int> order(total);
    for (int i = 0; i < total; i++) order[i] = i;
    sort(order.begin(), order.end(), [this](int a, int b) {
        return _player_times[a] < _player_times[b];
    });

    _ss << endl << "--- LEADERBOARD (排行榜) ---" << endl;
    double leader_time = _player_times[order[0]] / 1000.0;
    for (int i = 0; i < total; i++) {
        int pi = order[i];
        double t_sec = _player_times[pi] / 1000.0;
        double gap = (pi == order[0]) ? 0.0 : (t_sec - leader_time);
        string pos;
        if (i == 0) pos = "🥇";
        else if (i == 1) pos = "🥈";
        else if (i == 2) pos = "🥉";
        else pos = to_string(i + 1) + ".";

        string name;
        if (pi == 0) {
            name = "Human";
        } else {
            int ai_idx = pi - 1;
            string lvl = level_names[static_cast<int>(_ais[ai_idx]->level())];
            name = "AI" + to_string(ai_idx + 1) + "(" + lvl + ")";
        }
        _ss << pos << " " << name << " — " << fixed << setprecision(1) << t_sec << "s";
        if (gap > 0.0) _ss << " (+" << fixed << setprecision(1) << gap << "s)";
        _ss << endl;
    }

    return _ss.str();
}

void Manager::updatescore() { update_hit(); update_elapsed(); }

void Manager::update_hit() {
    int rows = _correct_board.size();
    int cols = _correct_board[0].size();
    if (_intense >= rows) {
        _correct_board.insert(_correct_board.end(), _intense - rows + 1, vector<int>(cols, -1));
    }
    if (_diff >= cols) {
        for (auto& row : _correct_board) row.insert(row.end(), _diff - cols + 1, -1);
    }
    _correct_board[_intense][_diff] = score;
}

void Manager::update_elapsed() {
    int rows = _elapsed_board.size();
    int cols = _elapsed_board[0].size();
    if (_intense >= rows) {
        _elapsed_board.insert(_elapsed_board.end(), _intense - rows + 1, vector<long>(cols, -1));
    }
    if (_diff >= cols) {
        for (auto& row : _elapsed_board) row.insert(row.end(), _diff - cols + 1, -1);
    }
    _elapsed_board[_intense][_diff] = _elapsed;
}

void Manager::savehighscore() { save_correct_count(); save_elapsed(); }

void Manager::save_correct_count() {
    ofstream f("correct.txt");
    for (auto& row : _correct_board) {
        for (size_t j = 0; j < row.size(); j++) {
            f << row[j] << (j + 1 < row.size() ? " " : "");
        }
        f << endl;
    }
}

void Manager::save_elapsed() {
    ofstream f("elapsed.txt");
    for (auto& row : _elapsed_board) {
        for (size_t j = 0; j < row.size(); j++) {
            f << row[j] << (j + 1 < row.size() ? " " : "");
        }
        f << endl;
    }
}
