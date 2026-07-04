#include "libspeedmath/arithmetic.h"
#include "libspeedmath/manager.h"

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

    // Choose operators
    std::string sops{""};
    std::cout << "Choose operators (e.g. 1234 for all):" << std::endl;
    std::cout << "  (1) +  (2) -  (3) *  (4) /" << std::endl;
    std::cin >> sops;

    // Choose AI difficulty
    std::string sai{""};
    std::cout << "AI difficulty: (0) None  (1) Easy  (2) Medium  (3) Hard" << std::endl;
    std::cin >> sai;

    vector<Op> ops;
    for (const char& c : sops) {
        switch (c) {
            case '1': ops.push_back(Op::ADD); break;
            case '2': ops.push_back(Op::SUB); break;
            case '3': ops.push_back(Op::MUL); break;
            case '4': ops.push_back(Op::DIV); break;
        }
    }
    if (ops.empty()) {
        ops.push_back(Op::ADD);
    }

    int diff{stoi(sdiff)};
    int intense{stoi(sintense)};
    int ai_val{stoi(sai)};

    vector<AiLevel> ai_levels;
    if (ai_val > 0) {
        AiLevel lvl = AiLevel::EASY;
        if (ai_val == 2) lvl = AiLevel::MEDIUM;
        else if (ai_val >= 3) lvl = AiLevel::HARD;
        ai_levels.push_back(lvl);
    }

    // Create a new game
    Manager gameManager = Manager(diff, intense, ops, ai_levels);

    while (true) {
        try {
            std::cout << gameManager.qnext() << std::endl;

            std::string sanswer{""};
            // Clear previous input and get full line
            std::cin >> ws;
            std::getline(std::cin, sanswer);

            std::cout << gameManager.grade_answer(sanswer) << std::endl;
        }
        catch (const std::out_of_range& e) {
            break;
        }
    }

    // Create another matrix for the elapsed time
    gameManager.updatescore();
    std::cout << gameManager.print_results() << std::endl;
    // Save highscore needs to save both the number of correct answers and the time taken
    gameManager.savehighscore();

    return 0;
}
