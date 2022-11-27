#include "arithmetic.h";
#include "manager.h";

using namespace std;


int main() {
    // Create a new game
    Manager gameManager = Manager(1, 1);

    try {
        std::cout << gameManager.qnext() << std::endl;
    }
    catch (const std::out_of_range& e) {}

    return 0;
}