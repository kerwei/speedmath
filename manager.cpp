#include "arithmetic.h"
#include "manager.h"


Manager::Manager(const int diff, const int intense):
    _seed(time(NULL)), _diff(diff), _intense(intense), _qtotal(10), _start_time(0), x(1), y(1), fcnPtr(nullptr)
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

    x = fcnPtr(_diff);
    y = fcnPtr(_diff);
    _qtotal -= 1;

    return std::to_string(x) + " + " + std::to_string(y);
}

std::string Manager::grade_answer(const int answer) {
    return scoreit(x, y, answer);
}

std::string Manager::grade_answer(const string answer) {
    return Manager::grade_answer(stoi(answer));
}