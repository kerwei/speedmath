#include "arithmetic.h"
#include "manager.h"

using namespace std;


int main(int argc, char* argv[]) {
    // Set difficulty
    std::string sdiff{""};
    std::cout << "Enter the difficulty: (1) 1-digit (2) 2-digits ..." << std::endl;
    std::cin >> sdiff;

    // Set intensity (number of questions)
    std::string sintense{""};
    std::cout << "Enter the intensity: (1) 10 questions (2) 20 questions ..." << std::endl;
    std::cin >> sintense;

    int diff{stoi(sdiff)};
    int intense{stoi(sintense)};

    // Create a new game
    Manager gameManager = Manager(diff, intense);

    while (true) {
        try {
            std::cout << gameManager.qnext() << std::endl;
        }
        catch (const std::out_of_range& e) {
            break;
        }
    }

    return 0;
}