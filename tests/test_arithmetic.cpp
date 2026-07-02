// 算术单元测试 (suànshù dānyuán cèshì — arithmetic unit tests)

#include "doctest.h"
#include "../libspeedmath/arithmetic.h"

// ============================================================
// isNumber 测试 (cèshì — tests)
// ============================================================
TEST_CASE("isNumber: valid positive integers") {
    CHECK(isNumber("0") == true);
    CHECK(isNumber("1") == true);
    CHECK(isNumber("42") == true);
    CHECK(isNumber("999") == true);
    CHECK(isNumber("1234567890") == true);
}

TEST_CASE("isNumber: negative integers") {
    CHECK(isNumber("-0") == true);   // C++ isdigit('-') is 0, but leading '-' is accepted
    CHECK(isNumber("-5") == true);
    CHECK(isNumber("-123") == true);
}

TEST_CASE("isNumber: empty and lone sign") {
    CHECK(isNumber("") == false);
    CHECK(isNumber("-") == false);
}

TEST_CASE("isNumber: non-numeric strings") {
    CHECK(isNumber("abc") == false);
    CHECK(isNumber("12a34") == false);
    CHECK(isNumber("3.14") == false);  // decimal point not a digit
    CHECK(isNumber(" 1") == false);    // leading space
    CHECK(isNumber("1 ") == false);    // trailing space
}

// ============================================================
// 随机数生成 (suíjī shù shēngchéng — random generation)
// ============================================================
TEST_CASE("random_one_digit: value in [0, 9]") {
    for (int i = 0; i < 100; i++) {
        int val = random_one_digit(1);
        CHECK(val >= 0);
        CHECK(val <= 9);
    }
}

TEST_CASE("random_ge_two_digit: 2-digit range [10, 99]") {
    for (int i = 0; i < 100; i++) {
        int val = random_ge_two_digit(2);
        CHECK(val >= 10);
        CHECK(val <= 99);
    }
}

TEST_CASE("random_ge_two_digit: 3-digit range [100, 999]") {
    for (int i = 0; i < 100; i++) {
        int val = random_ge_two_digit(3);
        CHECK(val >= 100);
        CHECK(val <= 999);
    }
}

// ============================================================
// scoreit (原始加法校验) (jiāfǎ xiàoyàn — addition validation)
// ============================================================
TEST_CASE("scoreit: addition") {
    CHECK(scoreit(3, 5, 8) == true);
    CHECK(scoreit(3, 5, 7) == false);
    CHECK(scoreit(0, 0, 0) == true);
    CHECK(scoreit(-3, 5, 2) == true);  // negative operands still work
}

// ============================================================
// check_answer (四则运算校验) (sìzé yùnsuàn xiàoyàn — 4-op validation)
// ============================================================
TEST_CASE("check_answer: addition") {
    CHECK(check_answer(Op::ADD, 3, 5, 8) == true);
    CHECK(check_answer(Op::ADD, 3, 5, 9) == false);
    CHECK(check_answer(Op::ADD, 0, 0, 0) == true);
}

TEST_CASE("check_answer: subtraction") {
    CHECK(check_answer(Op::SUB, 10, 3, 7) == true);
    CHECK(check_answer(Op::SUB, 10, 3, 6) == false);
    CHECK(check_answer(Op::SUB, 3, 10, -7) == true);   // negative result
    CHECK(check_answer(Op::SUB, 3, 10, -6) == false);
}

TEST_CASE("check_answer: multiplication") {
    CHECK(check_answer(Op::MUL, 6, 7, 42) == true);
    CHECK(check_answer(Op::MUL, 6, 7, 43) == false);
    CHECK(check_answer(Op::MUL, 0, 5, 0) == true);
}

TEST_CASE("check_answer: division with remainder") {
    // 7 / 3 = 2 r 1
    CHECK(check_answer(Op::DIV, 7, 3, 2, 1) == true);
    CHECK(check_answer(Op::DIV, 7, 3, 2, 0) == false);  // wrong remainder
    CHECK(check_answer(Op::DIV, 7, 3, 3, 1) == false);   // wrong quotient
    // 12 / 4 = 3 r 0
    CHECK(check_answer(Op::DIV, 12, 4, 3, 0) == true);
    CHECK(check_answer(Op::DIV, 12, 4, 3, 1) == false);
    // 0 / 5 = 0 r 0
    CHECK(check_answer(Op::DIV, 0, 5, 0, 0) == true);
}

// ============================================================
// op_symbol (运算符字符) (yùnsuànfú zìfú — operator character)
// ============================================================
TEST_CASE("op_symbol: returns correct character") {
    CHECK(op_symbol(Op::ADD) == '+');
    CHECK(op_symbol(Op::SUB) == '-');
    CHECK(op_symbol(Op::MUL) == '*');
    CHECK(op_symbol(Op::DIV) == '/');
}
