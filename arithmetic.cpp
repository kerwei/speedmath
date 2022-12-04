#include <bits/stdc++.h>
#include <iostream>
#include <limits>
#include <math.h>
#include <time.h>

using namespace std;


bool isNumber(string& str) {
    /*
        Function to check the string character by character
        Extracted from https://www.geeksforgeeks.org/continue-statement-cpp/
        on 12 Nov 2022
    */
    for (char const &c : str) {
       
        // using the std::isdigit() function
        if (std::isdigit(c) == 0)
          return false;
    }
    return true;
}


int random_one_digit(int phony) {
    return std::rand() % 10;
}


int random_ge_two_digit(int n) {
    int low{(int) pow(10, n-1)};
    return low + std::rand() % (int) (pow(10, n) - low);
}


bool scoreit(const int x, const int y, const int answer) {
    return x + y == answer;
}

// int main(int argc, char* argv[]) {
//     srand(time(NULL));

//     // Set difficulty
//     std::string sdiff{""};
//     std::cout << "Enter the difficulty: (1) 1-digit (2) 2-digits ..." << std::endl;
//     std::cin >> sdiff;

//     // Set intensity (number of questions)
//     std::string sintense{""};
//     std::cout << "Enter the intensity: (1) 10 questions (2) 20 questions ..." << std::endl;
//     std::cin >> sintense;

//     // Create the record matrix
//     // TODO: Read the records from file
//     std::vector<int> rows(11, -1);
//     std::vector<std::vector<int>> records(11, rows);

//     int x{0}; 
//     int y{0};

//     string sanswer{""};
//     int answer{0};
//     int diff{stoi(sdiff)};
//     int intense{stoi(sintense)};

//     int (*fcnPtr)(int);
//     if (diff == 1) {
//         fcnPtr = &random_one_digit;
//     }
//     else {
//         fcnPtr = &random_ge_two_digit;
//     }

//     int i{0};
//     int res{0};

//     time_t t_start = time(NULL);
//     while (i < intense * 10) {
//         x = (*fcnPtr)(diff);
//         y = (*fcnPtr)(diff);

//         std::cout << x << " + " << y << std::endl;
//         std::cin >> sanswer;

//         if (!isNumber(sanswer)) {
//             std::cout << "wrong" << std::endl;
//             i++;
//             continue;
//         }

//         answer = stoi(sanswer);

//         if (x + y == answer) {
//             std::cout << "correct" << std::endl;
//             res++;
//         }
//         else {
//             std::cout << "wrong" << std::endl;
//         }

//         i++;
//     }

//     long elapsed = time(NULL) - t_start;
//     double speed = std::numeric_limits<double>::infinity();

//     if (res > 0) {
//         speed = (double) elapsed/res;
//     }

//     std::cout << "Elapsed: " << elapsed << " seconds" << std::endl;
//     std::cout << "Score: " << res << "/" << intense * 10 << std::endl;
//     std::cout << "Speed: " << fixed << setprecision(5) << speed << " seconds per hit" << std::endl;

//     updaterecords(&records, diff, intense, (int) elapsed);
//     std::cout << "Updated records. Row: " << intense << " Col: " << diff << " Elapsed: " << records[intense][diff] << std::endl;

//     return 0;
// }