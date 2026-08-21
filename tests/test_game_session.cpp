// 多人游戏会话单元测试 (duōrén yóuxì huìhuà dānyuán cèshì — multiplayer GameSession unit tests)
// Covers the racing-score penalty rule in grade_and_leaderboard_json:
// wrong answers must get slowest_correct + PENALTY_MS (solo parity with
// Manager::_apply_racing_scores) — regression for the 2026-08-20 fix.

#include "doctest.h"
#include <algorithm>
#include <string>
#include <vector>
#include "../backend/game_session.h"

using namespace std;

// Build a session with two players and one question in play
static GameSession make_two_player_game() {
    vector<Op> ops = {Op::ADD};
    GameSession gs(1, 1, 1, ops);
    gs.add_player(101, "alice");
    gs.add_player(102, "bob");
    string q = gs.next_question_json();
    REQUIRE(q.find("question") != string::npos);
    return gs;
}

TEST_CASE("GameSession: correct answer keeps own time; wrong answer gets slowest correct + penalty") {
    GameSession gs = make_two_player_game();
    auto& mq = gs.question(0);
    int correct = mq.x + mq.y;

    gs.submit_answer(101, to_string(correct));       // alice correct
    gs.submit_answer(102, to_string(correct + 1));   // bob wrong

    gs.grade_and_leaderboard_json();
    const auto& players = gs.players();

    // alice: correct count 1, cumulative = her own question time
    CHECK(players[0].correct == 1);
    CHECK(players[0].cumulative_ms == players[0].q_time_ms);

    // bob: wrong → slowest correct (alice, the only correct player) + penalty
    CHECK(players[1].correct == 0);
    CHECK(players[1].cumulative_ms == players[0].cumulative_ms + Manager::PENALTY_MS);

    CHECK(gs.phase() == GamePhase::Evaluating);
}

TEST_CASE("GameSession: nobody correct → every wrong answer gets 0 + penalty") {
    GameSession gs = make_two_player_game();
    auto& mq = gs.question(0);
    int wrong = mq.x + mq.y + 100;   // definitely wrong

    gs.submit_answer(101, to_string(wrong));
    gs.submit_answer(102, to_string(wrong + 1));

    gs.grade_and_leaderboard_json();
    const auto& players = gs.players();

    CHECK(players[0].correct == 0);
    CHECK(players[1].correct == 0);
    // slowest_correct stays 0 → penalty only
    CHECK(players[0].cumulative_ms == Manager::PENALTY_MS);
    CHECK(players[1].cumulative_ms == Manager::PENALTY_MS);
}

TEST_CASE("GameSession: both correct → both keep their own times") {
    GameSession gs = make_two_player_game();
    auto& mq = gs.question(0);
    int correct = mq.x + mq.y;

    gs.submit_answer(101, to_string(correct));
    gs.submit_answer(102, to_string(correct));

    gs.grade_and_leaderboard_json();
    const auto& players = gs.players();

    CHECK(players[0].correct == 1);
    CHECK(players[1].correct == 1);
    CHECK(players[0].cumulative_ms == players[0].q_time_ms);
    CHECK(players[1].cumulative_ms == players[1].q_time_ms);
}

TEST_CASE("GameSession: timeout (empty answer) grades as wrong with penalty") {
    GameSession gs = make_two_player_game();
    auto& mq = gs.question(0);
    int correct = mq.x + mq.y;

    gs.submit_answer(101, to_string(correct));  // alice correct
    gs.timeout_unanswered();                    // bob timed out (empty answer)

    gs.grade_and_leaderboard_json();
    const auto& players = gs.players();

    CHECK(players[0].correct == 1);
    CHECK(players[1].correct == 0);
    CHECK(players[1].cumulative_ms == players[0].cumulative_ms + Manager::PENALTY_MS);
}
