#include <vector>
#include "arithmetic.h"
#include "manager.h"
#include <fstream>
#include <iomanip>


Manager::Manager(const int diff, const int intense, const vector<Op>& ops):
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
}

std::string Manager::qnext() {
    if (_qtotal <= 0) {
        throw std::out_of_range("");
    }

    // Pick a random operator from the chosen set
    _op = _ops[std::rand() % _ops.size()];

    if (_op == Op::DIV) {
        // Generate divisor (non-zero), quotient, and remainder
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

    // Start the timer
    _start_time = time(NULL);

    return std::to_string(x) + " " + op_symbol(_op) + " " + std::to_string(y);
}

std::string Manager::grade_answer(const int answer) {
    _elapsed += (time(NULL) - _start_time);

    if (check_answer(_op, x, y, answer)) {
        score += 1;
        return "correct";
    }
    else {
        return "wrong";
    }
}

std::string Manager::grade_answer(const string& answer) {
    _elapsed += (time(NULL) - _start_time);

    if (_op == Op::DIV) {
        size_t space = answer.find(' ');

        if (space != string::npos) {
            // 格式 A: "商 余数" (shāng yúshù — quotient remainder), space-separated
            if (isNumber(answer.substr(0, space)) && isNumber(answer.substr(space + 1))) {
                int q = stoi(answer.substr(0, space));
                int r = stoi(answer.substr(space + 1));
                if (check_answer(_op, x, y, q, r)) {
                    score += 1;
                    return "correct";
                }
            }
            return "wrong";
        }

        // 格式 B: "商余数" concatenated, no space — try every split
        if (!isNumber(answer) || answer[0] == '-') {
            return "wrong";
        }
        for (size_t split = 1; split < answer.size(); split++) {
            int q = stoi(answer.substr(0, split));
            int r = stoi(answer.substr(split));
            if (check_answer(_op, x, y, q, r)) {
                score += 1;
                return "correct";
            }
        }
        return "wrong";
    }
    else {
        if (!isNumber(answer)) {
            return "wrong";
        }
        if (check_answer(_op, x, y, stoi(answer))) {
            score += 1;
            return "correct";
        }
    }

    return "wrong";
}

std::string Manager::print_results() {
    _ss << "Elapsed: " << _elapsed << " seconds" << std::endl;
    _ss << "Score: " << score << "/" << _intense * 10 << std::endl;
    _ss << "Speed: " << fixed << setprecision(5) << (double) _elapsed/score << " seconds per hit" << std::endl;

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
