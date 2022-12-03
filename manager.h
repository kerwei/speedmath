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
        std::string grade_answer(const string answer);
        std::string print_results();

    private:
        const int _seed, _diff, _intense;
        int _qtotal, x, y, score;
        long int _elapsed;

        std::stringstream _ss;
        time_t _start_time;
        std::function<int(const int)> fcnPtr;
};  // class Manager