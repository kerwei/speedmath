#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;


class Manager {
    public:
        Manager(const int diff, const int intense);
        std::string qnext();
        std::string grade_answer(const int answer);
        std::string grade_answer(const string& answer);
        // TODO: Persist file to text file so that all-time high score can be tracked
        std::string print_results();

        void updatescore();
        void savehighscore();

    private:
        const int _seed, _diff, _intense;
        int _qtotal, x, y;
        int score; // the number of questions answered correctly
        long int _elapsed;

        time_t _start_time;
        std::stringstream _ss;
        std::vector<int> __row;
        std::vector<std::vector<int>> _records;
        std::function<int(const int)> fcnPtr;
};  // class Manager