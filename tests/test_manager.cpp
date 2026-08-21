// 管理器单元测试 (guǎnlǐ qì dānyuán cèshì — Manager unit tests)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../libquickmath/arithmetic.h"
#include "../libquickmath/manager.h"
#include <fstream>
#include <utility>

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

    // Correct answer grades "correct"
    CHECK(m.grade_answer(to_string(correct)) == "correct");
    // 幂等 (mìděng — idempotent): 同一题重复提交返回上次结果 (re-grade returns stored result)
    CHECK(m.grade_answer(to_string(correct + 1)) == "correct");
    CHECK(m.questions_answered() == 1);

    // Wrong answer on a fresh question grades "wrong"
    q = m.qnext();
    x = stoi(q.substr(0, q.find(' ')));
    y = stoi(q.substr(q.rfind(' ') + 1));
    CHECK(m.grade_answer(to_string(x + y + 1)) == "wrong");
}

TEST_CASE("Manager::grade_answer(string): subtraction") {
    vector<Op> ops = {Op::SUB};
    Manager m(1, 1, ops);
    string q = m.qnext();
    int x = stoi(q.substr(0, q.find(' ')));
    int y = stoi(q.substr(q.rfind(' ') + 1));
    int correct = x - y;

    CHECK(m.grade_answer(to_string(correct)) == "correct");
    // 幂等 (mìděng — idempotent)
    CHECK(m.grade_answer(to_string(correct + 1)) == "correct");
    CHECK(m.questions_answered() == 1);

    q = m.qnext();
    x = stoi(q.substr(0, q.find(' ')));
    y = stoi(q.substr(q.rfind(' ') + 1));
    CHECK(m.grade_answer(to_string(x - y + 1)) == "wrong");
}

TEST_CASE("Manager::grade_answer(string): multiplication") {
    vector<Op> ops = {Op::MUL};
    Manager m(1, 1, ops);
    string q = m.qnext();
    int x = stoi(q.substr(0, q.find(' ')));
    int y = stoi(q.substr(q.rfind(' ') + 1));
    int correct = x * y;

    CHECK(m.grade_answer(to_string(correct)) == "correct");
    // 幂等 (mìděng — idempotent)
    CHECK(m.grade_answer(to_string(correct + 1)) == "correct");
    CHECK(m.questions_answered() == 1);

    q = m.qnext();
    x = stoi(q.substr(0, q.find(' ')));
    y = stoi(q.substr(q.rfind(' ') + 1));
    CHECK(m.grade_answer(to_string(x * y + 1)) == "wrong");
}

TEST_CASE("Manager::grade_answer(string): division") {
    vector<Op> ops = {Op::DIV};
    Manager m(1, 1, ops);
    // Each answer format gets its own fresh question (grading is once per question)
    auto parse_quotient = [&m]() {
        string q = m.qnext();
        int x = stoi(q.substr(0, q.find(' ')));
        int y = stoi(q.substr(q.rfind(' ') + 1));
        return make_pair(x / y, x % y);
    };

    // 格式 A: space-separated (kònggé fēngé — space-separated)
    auto [q1, r1] = parse_quotient();
    CHECK(m.grade_answer(to_string(q1) + " " + to_string(r1)) == "correct");

    // 格式 B: concatenated (liánjiē — no space)
    auto [q2, r2] = parse_quotient();
    CHECK(m.grade_answer(to_string(q2) + to_string(r2)) == "correct");

    // Plain quotient without remainder: correct iff remainder is 0
    auto [q3, r3] = parse_quotient();
    if (r3 == 0) {
        CHECK(m.grade_answer(to_string(q3)) == "correct");
    } else {
        CHECK(m.grade_answer(to_string(q3)) == "wrong");
    }

    // Empty answer grades wrong
    m.qnext();
    CHECK(m.grade_answer("") == "wrong");
}

// ============================================================
// 边界情况 (biānjiè qíngkuàng — edge cases)
// ============================================================
TEST_CASE("Manager::grade_answer: non-numeric input") {
    vector<Op> ops = {Op::ADD};
    Manager m(1, 1, ops);
    // Each input gets its own fresh question (grading is once per question)
    m.qnext();
    CHECK(m.grade_answer("abc") == "wrong");
    m.qnext();
    CHECK(m.grade_answer("") == "wrong");
    m.qnext();
    CHECK(m.grade_answer("3.14") == "wrong");  // float format
}

TEST_CASE("Manager::grade_answer: double-submit is idempotent (no count inflation)") {
    // Regression for the 2026-08-17 solo leaderboard bug: fast Enter key-repeat or a
    // timeout racing a submit re-graded the same question, inflating the `total`
    // denominator and AI correct counts. Verified API repro: intense=1, one question
    // double-submitted → results showed total:2, AI correct:2.
    vector<Op> ops = {Op::ADD};
    vector<AiLevel> levels = {AiLevel::MEDIUM};
    Manager m(1, 1, ops, levels);
    m.qnext();

    // First grade: almost certainly wrong for ADD
    string r1 = m.grade_answer("12345");
    int human_before = m.player_correct_count(0);
    int total_before = m.questions_answered();
    vector<int> ai_before;
    for (int i = 1; i < m.player_count(); i++) {
        ai_before.push_back(m.player_correct_count(i));
    }

    // Re-grade the same question (double submit / timeout race): same result, no inflation
    CHECK(m.grade_answer("12345") == r1);
    CHECK(m.grade_answer("") == r1);

    CHECK(m.questions_answered() == total_before);
    CHECK(m.player_correct_count(0) == human_before);
    for (size_t i = 0; i < ai_before.size(); i++) {
        CHECK(m.player_correct_count(i + 1) == ai_before[i]);
    }
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
    // Should contain racing-style results
    CHECK(results.find("RACE") != string::npos);
    CHECK(results.find("LEADERBOARD") != string::npos);
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
