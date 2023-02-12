#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;


class Manager {
    public:
        Manager(const int diff, const int intense);
        std::string qnext();
        std::string grade_answer(const int answer);
        std::string grade_answer(string answer);
        std::string print_results();

    private:
        const int _seed, _diff, _intense;
        int _qtotal, x, y, score;
        long int _elapsed;

        time_t _start_time;
        std::stringstream _ss;
        std::vector<int> __row;
        std::vector<std::vector<int>> _records;
        std::function<int(const int)> fcnPtr;
        
        void updaterecords(std::vector<std::vector<int>> *records, const int diff, const int intense, const int score);
};  // class Manager