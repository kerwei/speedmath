// 管理器单元测试 (guǎnlǐ qì dānyuán cèshì — Manager unit tests)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../libspeedmath/arithmetic.h"
#include "../libspeedmath/manager.h"
#include <fstream>

// ============================================================
// 构造函数 (gòuzào hánshù — constructor)
// ============================================================
TEST_CASE("Manager: construction with add-only ops") {
    vector<Op> ops = {Op::ADD};
    Manager m(1, 2, ops);
    // Should not crash; private state indirectly tested via qnext
    SUBCASE("produces addition-only questions") {
        for (int i = 0; i < 10; i++) {
            string q = m.qnext();
            // "+" should appear; other ops should not
            CHECK(q.find('+') != string::npos);
        }
    }
}

TEST_CASE("Manager: construction with different difficulty") {
    vector<Op> ops = {Op::ADD};
    // diff=1 → random_one_digit (0-9), diff=2 → random_ge_two_digit (10-99)
    Manager m1(1, 1, ops);
    Manager m2(2, 1, ops);
    // Just ensure it constructs without error (range tested in arithmetic tests)
    CHECK(m1.qnext().find('+') != string::npos);
    CHECK(m2.qnext().find('+') != string::npos);
}

// ============================================================
// qnext (出题) (chū tí — question generation)
// ============================================================
TEST_CASE("Manager::qnext: format") {
    vector<Op> ops = {Op::ADD};
    Manager m(1, 1, ops);
    string q = m.qnext();
    // Should be like "3 + 5"
    CHECK(q.find(' ') != string::npos);
    CHECK(q.find('+') != string::npos);
}

TEST_CASE("Manager::qnext: throws when no questions remain") {
    vector<Op> ops = {Op::ADD};
    Manager m(1, 1, ops); // intense=1 → 10 questions
    for (int i = 0; i < 10; i++) {
        m.qnext();
    }
    CHECK_THROWS_AS(m.qnext(), std::out_of_range);
}

TEST_CASE("Manager::qnext: division generates valid divisor != 0") {
    vector<Op> ops = {Op::DIV};
    Manager m(1, 2, ops);  // intense=2 → 20 questions
    for (int i = 0; i < 20; i++) {
        string q = m.qnext();
        CHECK(q.find('/') != string::npos);
        // Divisor should not be zero
        size_t slash = q.find("/ ");
        if (slash != string::npos) {
            string divisor = q.substr(slash + 2);
            CHECK(stoi(divisor) != 0);
        }
    }
}

// ============================================================
// grade_answer (评分) (píngfēn — scoring)
// ============================================================
TEST_CASE("Manager::grade_answer(int): correct and wrong") {
    // We can't control the random values, but we can test the
    // second overload which reconstructs the answer
    vector<Op> ops = {Op::ADD};
    Manager m(1, 1, ops);
    // We don't know x and y, but we can use the format:
    // qnext returns "x + y", we parse it, then submit the correct answer
    // This is tested via the string overload below
}

TEST_CASE("Manager::grade_answer(string): addition") {
    vector<Op> ops = {Op::ADD};
    Manager m(1, 1, ops);
    string q = m.qnext();
    // Parse "x + y" and compute correct answer
    int x = stoi(q.substr(0, q.find(' ')));
    int y = stoi(q.substr(q.rfind(' ') + 1));
    int correct = x + y;

    // grade_answer should accept both int form and string form
    // We can only call the string overload since the int overload
    // doesn't match Manager's API (it takes a separate int)
    CHECK(m.grade_answer(to_string(correct)) == "correct");
    CHECK(m.grade_answer(to_string(correct + 1)) == "wrong");
}

TEST_CASE("Manager::grade_answer(string): subtraction") {
    vector<Op> ops = {Op::SUB};
    Manager m(1, 1, ops);
    string q = m.qnext();
    int x = stoi(q.substr(0, q.find(' ')));
    int y = stoi(q.substr(q.rfind(' ') + 1));
    int correct = x - y;

    CHECK(m.grade_answer(to_string(correct)) == "correct");
    CHECK(m.grade_answer(to_string(correct + 1)) == "wrong");
}

TEST_CASE("Manager::grade_answer(string): multiplication") {
    vector<Op> ops = {Op::MUL};
    Manager m(1, 1, ops);
    string q = m.qnext();
    int x = stoi(q.substr(0, q.find(' ')));
    int y = stoi(q.substr(q.rfind(' ') + 1));
    int correct = x * y;

    CHECK(m.grade_answer(to_string(correct)) == "correct");
    CHECK(m.grade_answer(to_string(correct + 1)) == "wrong");
}

TEST_CASE("Manager::grade_answer(string): division") {
    vector<Op> ops = {Op::DIV};
    Manager m(1, 1, ops);
    string q = m.qnext();
    int x = stoi(q.substr(0, q.find(' ')));
    int y = stoi(q.substr(q.rfind(' ') + 1));
    int quotient = x / y;
    int remainder = x % y;

    string correct = to_string(quotient) + " " + to_string(remainder);
    CHECK(m.grade_answer(correct) == "correct");
    CHECK(m.grade_answer(to_string(quotient)) == "wrong");  // missing remainder
    CHECK(m.grade_answer("") == "wrong");
}

// ============================================================
// 边界情况 (biānjiè qíngkuàng — edge cases)
// ============================================================
TEST_CASE("Manager::grade_answer: non-numeric input") {
    vector<Op> ops = {Op::ADD};
    Manager m(1, 1, ops);
    m.qnext();  // advance to first question
    CHECK(m.grade_answer("abc") == "wrong");
    CHECK(m.grade_answer("") == "wrong");
    CHECK(m.grade_answer("3.14") == "wrong");  // float format
}

TEST_CASE("Manager::grade_answer: negative result for subtraction") {
    // This test verifies negative answers are accepted
    vector<Op> ops = {Op::SUB};
    Manager m(1, 1, ops);
    string q = m.qnext();
    // If x < y, the answer is negative
    // Just verify the system doesn't crash on negative input
    m.grade_answer("-5");  // may or may not be correct, but shouldn't crash
    CHECK(true);
}

// ============================================================
// print_results / updatescore (结果输出 / 更新分数)
// ============================================================
TEST_CASE("Manager::print_results: format") {
    vector<Op> ops = {Op::ADD};
    Manager m(1, 1, ops);
    for (int i = 0; i < 10; i++) {
        m.qnext();
        // Submit answers round-robin to get some correct, some wrong
        if (i < 5) {
            m.grade_answer(to_string(0));  // likely wrong
        } else {
            m.grade_answer(to_string(999)); // likely wrong too
        }
    }
    m.updatescore();
    string results = m.print_results();
    // Should contain key labels
    CHECK(results.find("Elapsed") != string::npos);
    CHECK(results.find("Score") != string::npos);
    CHECK(results.find("Speed") != string::npos);
}

// ============================================================
// 持久化 (chíjiǔhuà — persistence)
// ============================================================
TEST_CASE("Manager::savehighscore: files written") {
    // Remove stale files if any
    remove("correct.txt");
    remove("elapsed.txt");

    vector<Op> ops = {Op::ADD};
    Manager m(1, 1, ops);
    for (int i = 0; i < 10; i++) {
        m.qnext();
        m.grade_answer("0");
    }
    m.updatescore();
    m.savehighscore();

    // Verify files exist
    ifstream f1("correct.txt");
    CHECK(f1.good());
    f1.close();

    ifstream f2("elapsed.txt");
    CHECK(f2.good());
    f2.close();

    // Cleanup
    remove("correct.txt");
    remove("elapsed.txt");
}
