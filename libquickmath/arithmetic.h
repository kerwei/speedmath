#pragma once

#include <iostream>
#include <limits>
#include <math.h>
#include <time.h>

using namespace std;


enum class Op { ADD, SUB, MUL, DIV };

bool scoreit(const int x, const int y, const int answer);

bool check_answer(const Op op, const int x, const int y, const int answer, const int remainder = 0);

char op_symbol(const Op op);

bool isNumber(const string& str);

int random_one_digit(const int phony);

int random_ge_two_digit(const int n);
