// AI 对手单元测试 (AI duìshǒu dānyuán cèshì — AI opponent unit tests)

#include "doctest.h"
#include "../libspeedmath/ai_opponent.h"
#include <cstdlib>

using namespace std;

TEST_CASE("AiOpponent: construction and level settings") {
    // Easy: ~60% accuracy, 5-8s delay
    AiOpponent easy(AiLevel::EASY);
    CHECK(easy.score() == 0);
    CHECK(easy.total() == 0);
    CHECK(easy.elapsed_ms() == 0);

    // Medium: ~85% accuracy
    AiOpponent medium(AiLevel::MEDIUM);
    CHECK(medium.score() == 0);

    // Hard: 100% accuracy, always correct
    AiOpponent hard(AiLevel::HARD);
    CHECK(hard.score() == 0);
}

TEST_CASE("AiOpponent: delay range") {
    AiOpponent easy(AiLevel::EASY);
    for (int i = 0; i < 20; i++) {
        int d = easy.delay_ms();
        CHECK(d >= 5000);
        CHECK(d <= 8000);
    }

    AiOpponent hard(AiLevel::HARD);
    for (int i = 0; i < 20; i++) {
        int d = hard.delay_ms();
        CHECK(d >= 500);
        CHECK(d <= 1000);
    }
}

TEST_CASE("AiOpponent: hard always correct") {
    AiOpponent hard(AiLevel::HARD);
    int correct_count = 0;
    int trials = 100;
    for (int i = 0; i < trials; i++) {
        string ans = hard.decide_answer(3, 5, '+', 8);
        if (ans == "8") correct_count++;
        hard.record(ans == "8", 500);
    }
    CHECK(correct_count == trials);  // Hard level should always be correct
    CHECK(hard.score() == trials);
    CHECK(hard.total() == trials);
}

TEST_CASE("AiOpponent: division answer format") {
    AiOpponent hard(AiLevel::HARD);
    // 7 / 3 = 2 r 1
    string ans = hard.decide_answer(7, 3, '/', 2, 1);
    CHECK(ans == "2 1");
}

TEST_CASE("AiOpponent: easy accuracy below 100%") {
    AiOpponent easy(AiLevel::EASY);
    int correct_count = 0;
    int trials = 500;  // large sample for statistical test
    for (int i = 0; i < trials; i++) {
        string ans = easy.decide_answer(3, 5, '+', 8);
        if (ans == "8") correct_count++;
    }
    // Easy has 60% accuracy, so should be less than 100%
    // With 500 trials, it's extremely unlikely to hit 500/500
    CHECK(correct_count < trials);
    // But should get at least some correct
    CHECK(correct_count > 0);
}

TEST_CASE("AiOpponent: wrong answers are plausible") {
    AiOpponent easy(AiLevel::EASY);
    for (int i = 0; i < 50; i++) {
        string ans = easy.decide_answer(10, 3, '+', 13);
        if (ans != "13") {
            // Wrong answer should be a number close to the correct one
            int val = stoi(ans);
            // For addition of 10+3, wrong answer should be in reasonable range
            CHECK(val >= 8);
            CHECK(val <= 18);
        }
    }
}

TEST_CASE("AiOpponent: record accumulates stats") {
    AiOpponent opp(AiLevel::MEDIUM);
    for (int i = 0; i < 10; i++) {
        opp.decide_answer(1, 1, '+', 2);  // increments _total
        opp.record(true, 2000);
    }
    CHECK(opp.score() == 10);
    CHECK(opp.total() == 10);
    CHECK(opp.elapsed_ms() == 20000);  // 10 * 2000
}
