#include <vector>
#include "arithmetic.h"
#include "manager.h"
#include <fstream>
#include <iomanip>


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

    // Create AI opponents for each level in the vector
    for (AiLevel lvl : ai_levels) {
        _ais.push_back(new AiOpponent(lvl));
        _ai_answers.push_back("");
        _ai_delays.push_back(0);
    }
}

std::string Manager::qnext() {
    if (_qtotal <= 0) {
        throw std::out_of_range("");
    }

    // Pick a random operator from the chosen set
    _op = _ops[std::rand() % _ops.size()];

    if (_op == Op::DIV) {
        int divisor = fcnPtr(_diff);
        while (divisor == 0) {
            divisor = fcnPtr(_diff);
        }
        int quotient = fcnPtr(_diff);
        int remainder = std::rand() % divisor;
        x = divisor * quotient + remainder;
        y = divisor;
    }
    else {
        x = fcnPtr(_diff);
        y = fcnPtr(_diff);
    }

    _qtotal -= 1;

    // 让所有 AI 对手决定答案 (let all AI opponents decide)
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

    // Start the timer
    _start_time = time(NULL);

    return std::to_string(x) + " " + op_symbol(_op) + " " + std::to_string(y);
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
    }
}

std::string Manager::grade_answer(const int answer) {
    _elapsed += (time(NULL) - _start_time);

    bool correct = check_answer(_op, x, y, answer);
    if (correct) {
        score += 1;
    }
    _grade_ai_answers();
    return correct ? "correct" : "wrong";
}

std::string Manager::grade_answer(const string& answer) {
    _elapsed += (time(NULL) - _start_time);

    if (_op == Op::DIV) {
        size_t space = answer.find(' ');

        if (space != string::npos) {
            if (isNumber(answer.substr(0, space)) && isNumber(answer.substr(space + 1))) {
                int q = stoi(answer.substr(0, space));
                int r = stoi(answer.substr(space + 1));
                if (check_answer(_op, x, y, q, r)) {
                    score += 1;
                    _grade_ai_answers();
                    return "correct";
                }
            }
            _grade_ai_answers();
            return "wrong";
        }

        // Concatenated format
        if (!isNumber(answer) || answer[0] == '-') {
            _grade_ai_answers();
            return "wrong";
        }
        for (size_t split = 1; split < answer.size(); split++) {
            int q = stoi(answer.substr(0, split));
            int r = stoi(answer.substr(split));
            if (check_answer(_op, x, y, q, r)) {
                score += 1;
                _grade_ai_answers();
                return "correct";
            }
        }
        _grade_ai_answers();
        return "wrong";
    }
    else {
        if (!isNumber(answer)) {
            _grade_ai_answers();
            return "wrong";
        }
        if (check_answer(_op, x, y, stoi(answer))) {
            score += 1;
            _grade_ai_answers();
            return "correct";
        }
    }

    _grade_ai_answers();
    return "wrong";
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

std::string Manager::print_results() {
    _ss << "Elapsed: " << _elapsed << " seconds" << std::endl;
    _ss << "Score: " << score << "/" << _intense * 10 << std::endl;
    if (score > 0) {
        _ss << "Speed: " << fixed << setprecision(5) << (double) _elapsed/score << " seconds per hit" << std::endl;
    }

    if (!_ais.empty()) {
        _ss << std::endl;
        _ss << "--- AI 对手们 (AI Opponents) ---" << std::endl;
        string level_names[] = {"Easy", "Medium", "Hard"};
        int best_ai_score = -1;
        for (size_t i = 0; i < _ais.size(); i++) {
            int ai_total_v = _ais[i]->total();
            int ai_score_v = _ais[i]->score();
            double ai_time_sec = _ais[i]->elapsed_ms() / 1000.0;
            string lvl_name = level_names[static_cast<int>(_ais[i]->level())];
            _ss << "AI" << (i+1) << " (" << lvl_name << "): "
                << ai_score_v << "/" << ai_total_v;
            if (ai_score_v > 0) {
                _ss << ", Speed: " << fixed << setprecision(3) << ai_time_sec / ai_score_v << "s/hit";
            }
            _ss << std::endl;
            if (ai_score_v > best_ai_score) best_ai_score = ai_score_v;
        }

        _ss << std::endl;
        if (score > best_ai_score) {
            _ss << "结果: 玩家胜! (Player wins!)" << std::endl;
        } else if (score < best_ai_score) {
            _ss << "结果: AI 胜! (AI wins!)" << std::endl;
        } else {
            _ss << "结果: 平局! (Draw!)" << std::endl;
        }
    }

    return _ss.str();
}

void Manager::updatescore() {
    update_hit();
    update_elapsed();
}

void Manager::update_hit() {
    int rows = _correct_board.size();
    int columns = _correct_board[0].size();

    if (_intense >= rows) {
        std::vector<int> new_row(columns, -1);
        std::vector<std::vector<int>> new_block(_intense - rows + 1, new_row);
        _correct_board.insert(_correct_board.end(), new_block.begin(), new_block.end());
    }

    if (_diff >= columns) {
        for (int j = 0; j <= _intense; j++) {
            std::vector<int>::iterator it = _correct_board[j].end();
            _correct_board[j].insert(it, _diff - columns + 1, -1);
        }
    }

    _correct_board[_intense][_diff] = score;
}

void Manager::update_elapsed() {
    int rows = _elapsed_board.size();
    int columns = _elapsed_board[0].size();

    if (_intense >= rows) {
        std::vector<long> new_row(columns, -1);
        std::vector<std::vector<long>> new_block(_intense - rows + 1, new_row);
        _elapsed_board.insert(_elapsed_board.end(), new_block.begin(), new_block.end());
    }

    if (_diff >= columns) {
        for (int j = 0; j <= _intense; j++) {
            std::vector<long>::iterator it = _elapsed_board[j].end();
            _elapsed_board[j].insert(it, _diff - columns + 1, -1);
        }
    }

    _elapsed_board[_intense][_diff] = _elapsed;
}

void Manager::savehighscore() {
    save_correct_count();
    save_elapsed();
}

void Manager::save_correct_count() {
    ofstream savefile;
    savefile.open("correct.txt");

    const int cols = _correct_board[0].size();
    for (int i = 0; i < _correct_board.size(); i++) {
        for (int j = 0; j < cols - 1; j++) {
            savefile << _correct_board[i][j] << " ";
        }

        savefile << _correct_board[i][cols -1] << std::endl;
    }

    savefile.close();
}

void Manager::save_elapsed() {
    ofstream savefile;
    savefile.open("elapsed.txt");

    const int cols = _elapsed_board[0].size();
    for (int i = 0; i < _elapsed_board.size(); i++) {
        for (int j = 0; j < cols - 1; j++) {
            savefile << _elapsed_board[i][j] << " ";
        }

        savefile << _elapsed_board[i][cols -1] << std::endl;
    }

    savefile.close();
}
