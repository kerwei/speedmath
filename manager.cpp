#include "arithmetic.h"
#include "manager.h"


Manager::Manager(const int diff, const int intense):
    _seed(time(NULL)), _diff(diff), _intense(intense), _qtotal(10), _elapsed(0), 
    x(1), y(1), score(0), _start_time(time(NULL)), 
    __row({-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}), 
    _records({__row,__row,__row,__row,__row,__row,__row,__row,__row,__row,__row}),
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

    x = fcnPtr(_diff);
    y = fcnPtr(_diff);
    _qtotal -= 1;

    // Start the timer
    _start_time = time(NULL);

    return std::to_string(x) + " + " + std::to_string(y);
}

std::string Manager::grade_answer(const int answer) {
    _elapsed += (time(NULL) - _start_time);

    if (scoreit(x, y, answer)) {
        score += 1;
        return "correct";
    }
    else {
        return "wrong";
    }
}

std::string Manager::grade_answer(string answer) {
    if (!isNumber(answer)) {
        // if the input answer is non-numeric
        // ensure that the answer is always wrong
        return Manager::grade_answer(x + y + 1);
    }

    return Manager::grade_answer(stoi(answer));
}

std::string Manager::print_results() {
    _ss << "Elapsed: " << _elapsed << " seconds" << std::endl;
    _ss << "Score: " << score << "/" << _intense * 10 << std::endl;
    _ss << "Speed: " << fixed << setprecision(5) << (double) _elapsed/score << " seconds per hit" << std::endl;

    return _ss.str();
}

void Manager::updaterecords(std::vector<std::vector<int>> *records, int diff, int intense, int score) {
    /* records:
            0    1     2     3    4 +--> diff
        0   0    0     0     0    0
        1   0    10    -1    -1   -1
        2   0   -1     27    -1   -1
        3   0   -1     -1    -1   -1
        4   0   -1     -1    -1   -1
        +
        |
        v
        intense (x10)

        Constraints:
            diff > 0; intense > 0; (diff, intense) has a default value of -1

        update (diff, intense) if 1/score > 1/(diff, intense)

        TODO: Refactor this into a 1D array,
        where column indices for the second row = col1 + (n x row) + coln
        0, 1, 2, ... col, col1-0, col1-1, col1-2, ... col1-n, col2-0 
    */
    int rows = records->size();
    int columns = records[0].size();

    // Add more inner vectors to record if intense is greater than the rows
    if (intense >= rows) {
        std::vector<int> new_row(columns, -1);
        std::vector<std::vector<int>> new_block(intense - rows + 1, new_row);
        records->insert(records->end(), new_block.begin(), new_block.end());
    }

    // If diff is greater than the number of columns,
    // append a -1 to the end of each nested vector
    if (diff >= columns) {
        for (int j = 0; j <= intense; j++) {
            std::vector<int>::iterator it = (*records)[j].end();
            (*records)[j].insert(it, diff - columns + 1, -1);
        }
    }

    (*records)[intense][diff] = score;
}
