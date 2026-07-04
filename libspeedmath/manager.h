#pragma once

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "arithmetic.h"
#include "ai_opponent.h"

using namespace std;


class Manager {
    public:
        Manager(const int diff, const int intense, const vector<Op>& ops, const vector<AiLevel>& ai_levels = {});

        std::string qnext();
        std::string grade_answer(const int answer);
        std::string grade_answer(const string& answer);

        // AI 对手群信息
        int ai_count() const { return _ais.size(); }
        bool ai_enabled() const { return !_ais.empty(); }
        std::string ai_answer(int idx) const { return _ai_answers[idx]; }
        int ai_delay_ms(int idx) const { return _ai_delays[idx]; }
        int ai_score(int idx) const { return _ais[idx]->score(); }
        int ai_total(int idx) const { return _ais[idx]->total(); }
        long ai_elapsed_ms(int idx) const { return _ais[idx]->elapsed_ms(); }
        int ai_total_score() const;
        long ai_total_elapsed_ms() const;

        // 赛车积分: 累计时间 (ms), 0 = human, 1+ = AI
        int player_count() const { return 1 + _ais.size(); }
        long player_cumulative_time(int idx) const { return _player_times[idx]; }
        int player_correct_count(int idx) const;
        int questions_answered() const { return _questions_answered; }

        std::string print_results();

        void updatescore();
        void savehighscore();

        static const long PENALTY_MS = 3000;  // 错题罚时: +3s

    private:
        const int _seed, _diff, _intense;
        const vector<Op> _ops;
        Op _op;
        int _qtotal, x, y;
        int score;
        long int _elapsed;

        time_t _start_time;
        std::stringstream _ss;
        std::vector<int> __row;
        std::vector<long> __lrow;
        std::vector<std::vector<int>> _correct_board;
        std::vector<std::vector<long>> _elapsed_board;
        std::function<int(const int)> fcnPtr;

        // AI 对手群
        vector<AiOpponent*> _ais;
        vector<string> _ai_answers;
        vector<int> _ai_delays;

        // 赛车积分: [human, ai1, ai2, ...]
        vector<long> _player_times;
        int _human_correct_count = 0;
        vector<int> _ai_correct_counts;
        int _questions_answered = 0;

        void update_elapsed();
        void update_hit();
        void save_elapsed();
        void save_correct_count();
        void _grade_ai_answers();
        void _apply_racing_scores(long human_question_time_ms, bool human_correct);  // 赛车计分
};  // class Manager
