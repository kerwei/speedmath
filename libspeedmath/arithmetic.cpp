#include "arithmetic.h"

#include <iostream>
#include <limits>
#include <math.h>
#include <time.h>

using namespace std;


bool isNumber(const string& str) {
    if (str.empty()) {
        return false;
    }
    size_t start = 0;
    if (str[0] == '-') {
        if (str.size() == 1) {
            return false;
        }
        start = 1;
    }
    for (size_t i = start; i < str.size(); i++) {
        if (std::isdigit(str[i]) == 0) {
          return false;
        }
    }
    return true;
}


int random_one_digit(const int phony) {
    return std::rand() % 10;
}


int random_ge_two_digit(const int n) {
    int low{(int) pow(10, n-1)};
    return low + std::rand() % (int) (pow(10, n) - low);
}


bool scoreit(const int x, const int y, const int answer) {
    return x + y == answer;
}

bool check_answer(const Op op, const int x, const int y, const int answer, const int remainder) {
    switch (op) {
        case Op::ADD: return x + y == answer;
        case Op::SUB: return x - y == answer;
        case Op::MUL: return x * y == answer;
        case Op::DIV: return answer == x / y && remainder == x % y;
    }
    return false;
}

char op_symbol(const Op op) {
    switch (op) {
        case Op::ADD: return '+';
        case Op::SUB: return '-';
        case Op::MUL: return '*';
        case Op::DIV: return '/';
    }
    return '?';
}
