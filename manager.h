#include <functional>
#include <stdexcept>
#include <string>

using namespace std;


class Manager {
    public:
        Manager(const int diff, const int intense);
        std::string qnext();
        std::string grade_answer(const int answer);
        std::string grade_answer(const string answer);

    private:
        const int _seed, _diff, _intense;
        long int _start_time;

        int _qtotal, x, y;
        std::function<int(const int)> fcnPtr;
};  // class Manager