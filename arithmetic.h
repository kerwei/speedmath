#include <bits/stdc++.h>
#include <iostream>
#include <limits>
#include <math.h>
#include <time.h>

using namespace std;


std::string scoreit(const int x, const int y, const int answer);

bool isNumber(string& str);

int random_one_digit(int phony);

int random_ge_two_digit(int n);

void updaterecords(std::vector<std::vector<int>> *records, int diff, int intense, int score);