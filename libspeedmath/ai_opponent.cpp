#include "ai_opponent.h"
#include <cstdlib>
#include <sstream>
#include <algorithm>

using namespace std;

AiOpponent::AiOpponent(AiLevel level) : _level(level) {
    switch (level) {
        case AiLevel::EASY:
            _accuracy = 0.60;
            _min_delay = 5000;
            _max_delay = 8000;
            break;
        case AiLevel::MEDIUM:
            _accuracy = 0.85;
            _min_delay = 2000;
            _max_delay = 4000;
            break;
        case AiLevel::HARD:
            _accuracy = 1.0;
            _min_delay = 500;
            _max_delay = 1000;
            break;
    }
}

int AiOpponent::delay_ms() const {
    if (_min_delay == _max_delay) {
        return _min_delay;
    }
    return _min_delay + std::rand() % (_max_delay - _min_delay);
}

string AiOpponent::_wrong_answer(int x, int y, char op, int correct_answer, int correct_remainder) {
    if (op == '/') {
        // For division, tweak quotient and/or remainder
        int q = correct_answer;
        int r = correct_remainder;
        int tweak = (std::rand() % 3) + 1;  // 1-3
        if (std::rand() % 2 == 0) {
            q += tweak;
        } else {
            r = (r + tweak) % y;  // wrap remainder
        }
        // Ensure quotient is non-negative
        if (q < 0) q = 0;
        return to_string(q) + " " + to_string(r);
    }

    // For +, -, *: off by ±1 to ±5
    int offset = (std::rand() % 5) + 1;
    if (std::rand() % 2 == 1) {
        offset = -offset;
    }
    // For multiplication, keep offset smaller relative to answer
    if (op == '*' && correct_answer > 0) {
        offset = (std::rand() % std::max(1, correct_answer / 4)) + 1;
        if (std::rand() % 2 == 1) {
            offset = -offset;
        }
    }
    return to_string(correct_answer + offset);
}

string AiOpponent::decide_answer(int x, int y, char op, int correct_answer, int correct_remainder) {
    _total++;

    // Decide if correct based on accuracy
    int roll = std::rand() % 100;
    bool correct = (roll < static_cast<int>(_accuracy * 100));

    if (correct) {
        if (op == '/') {
            return to_string(correct_answer) + " " + to_string(correct_remainder);
        }
        return to_string(correct_answer);
    }

    return _wrong_answer(x, y, op, correct_answer, correct_remainder);
}

void AiOpponent::record(bool correct, int time_ms) {
    _elapsed_ms += time_ms;
    if (correct) {
        _score++;
    }
}
