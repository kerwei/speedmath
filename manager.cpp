#include "arithmetic.h"
#include "manager.h"
#include <fstream>


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

std::string Manager::grade_answer(const string& answer) {
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

void Manager::update_score() {
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
    int rows = _records.size();
    int columns = _records[0].size();

    // Add more inner vectors to record if intense is greater than the rows
    if (_intense >= rows) {
        std::vector<int> new_row(columns, -1);
        std::vector<std::vector<int>> new_block(_intense - rows + 1, new_row);
        _records.insert(_records.end(), new_block.begin(), new_block.end());
    }

    // If diff is greater than the number of columns,
    // append a -1 to the end of each nested vector
    if (_diff >= columns) {
        for (int j = 0; j <= _intense; j++) {
            std::vector<int>::iterator it = _records[j].end();
            _records[j].insert(it, _diff - columns + 1, -1);
        }
    }

    _records[_intense][_diff] = score;
}

void Manager::save_score() {
    ofstream savefile;
    savefile.open(_savefile);

    const int cols = _records[0].size();
    // Loop through the scores and persist them to file
    for (int i = 0; i < _records.size(); i++) {
        for (int j = 0; j < cols - 1; j++) {
            savefile << _records[i][j] << " ";
        }

        savefile << _records[i][cols -1] << std::endl;
    }

    savefile.close();
}

void Manager::load_score() {
    ifstream savefile;
    savefile.open(_savefile);

    std:: stringstream _ifss;
    std::string line;
    std::vector<string> _frecords;
    /*
        Convert input file stream to game record object
        _records is instantiated as a default (10, 10) matrix
        file input is not guaranteed to be (10, 10)
        1. Read row count
        2. Read column count
        3. Instantiate _record container
        4. Convert file input to _record
    */
    int r{0};   // row count
    while (getline(savefile, line)) {
        std::cout << line << std::endl;
        _frecords.push_back(line);
        r++;
    }

    int c{0};   // column count
    for (int j = 0; j < _frecords[0].length(); j++) {
        // count non-numeric chars because numbers can be more than 1 digit
        if (!isNumber(string(1, _frecords[0][j]))) {
            c++;
        }
    }

    // the count of numbers would be non-numeric chars (gaps) + 1
    if (c > 0) {
        c++;
    }

    ensure_record_shape(r, c);

    // TODO: iterate and update each cell in the record

    savefile.close();
}

void Manager::set_params(const int diff, const int intense) {
    _diff = diff;
    _intense = intense;
}

void Manager::ensure_record_shape(const int row, const int col) {
    int _row{(int) _records.size()};
    int _col{(int) _records[0].size()};
    // existing records already has the required rows and cols
    if (_row >= row && _col >= col) {
        return;
    }

    if (row > _row) {
        std::vector<int> new_row(_col, -1);
        std::vector<std::vector<int>> new_block(row - _row, new_row);
        _records.insert(_records.end(), new_block.begin(), new_block.end());
    }

    if (_diff > _col) {
        for (int j = 0; j <= _intense; j++) {
            std::vector<int>::iterator it = _records[j].end();
            _records[j].insert(it, col - _col, -1);
        }
    }
}