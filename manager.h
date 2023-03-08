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
        std::string grade_answer(const string& answer);
        std::string print_results();

        void set_params(const int diff, const int intense);
        void update_score();
        // TODO: Refactor to a file read/write manager
        void save_score();
        void load_score();

    private:
        const std::string _savefile{"highscore.txt"};
        const int _seed; 
        int _diff, _intense;
        int _qtotal, x, y;
        int score; // the number of questions answered correctly
        long int _elapsed;

        time_t _start_time;
        std::stringstream _ss;
        std::vector<int> __row;
        std::vector<std::vector<int>> _records;
        std::function<int(const int)> fcnPtr;

        void ensure_record_shape(const int row, const int col);
};  // class Manager