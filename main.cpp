#include "arithmetic.h"
#include "manager.h"

using namespace std;


int main(int argc, char* argv[]) {
    // Create a new game manager with default values
    Manager gameManager = Manager(1, 1);

    // Load existing highscore from file
    gameManager.load_score();

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

    gameManager.set_params(diff, intense);

    // Print the highscore for this mode and count down to start

    while (true) {
        try {
            std::cout << gameManager.qnext() << std::endl;

            std::string sanswer{""};
            std::cin >> sanswer;

            std::cout << gameManager.grade_answer(sanswer) << std::endl;
        }
        catch (const std::out_of_range& e) {
            break;
        }
    }

    gameManager.update_score();
    std::cout << gameManager.print_results() << std::endl;
    gameManager.save_score();

    return 0;
}