#include <bits/stdc++.h>
#include <iostream>
#include <limits>
#include <math.h>
#include <time.h>

using namespace std;


bool isNumber(const string str) {
    /*
        Function to check the string character by character
        Extracted from https://www.geeksforgeeks.org/continue-statement-cpp/
        on 12 Nov 2022
    */
    for (const char &c : str) {
       
        // using the std::isdigit() function
        if (std::isdigit(c) == 0)
          return false;
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
