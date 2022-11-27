#include <functional>
#include <stdexcept>
#include <string>

using namespace std;


class Manager {
    public:
        Manager(const int diff, const int intense) : _seed(time(NULL)), _diff(diff), _intense(intense), _qtotal(10), _start_time(0), x(1), y(1), fcnPtr(nullptr) {}
        std::string qnext() {}
        bool scoreit(const int) {}

    private:
        const int _seed, _diff, _intense;
        long int _start_time;

        int _qtotal, x, y;
        std::function<int(const int)> fcnPtr;
};  // class Manager