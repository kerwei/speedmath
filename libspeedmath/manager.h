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
        // 构造函数: diff=难度, intense=题量(1=10题), ops=算子, ai_levels=每个AI的难度(空=无AI)
        Manager(const int diff, const int intense, const vector<Op>& ops, const vector<AiLevel>& ai_levels = {});

        std::string qnext();
        std::string grade_answer(const int answer);
        std::string grade_answer(const string& answer);

        // AI 对手群信息 (duìshǒu qún xìnxī — multi-AI info)
        int ai_count() const { return _ais.size(); }
        bool ai_enabled() const { return !_ais.empty(); }
        std::string ai_answer(int idx) const { return _ai_answers[idx]; }
        int ai_delay_ms(int idx) const { return _ai_delays[idx]; }
        int ai_score(int idx) const { return _ais[idx]->score(); }
        int ai_total(int idx) const { return _ais[idx]->total(); }
        long ai_elapsed_ms(int idx) const { return _ais[idx]->elapsed_ms(); }

        // 所有AI的总分/总时间 (for simplicity in frontend)
        int ai_total_score() const;
        long ai_total_elapsed_ms() const;

        std::string print_results();

        void updatescore();
        void savehighscore();

    private:
        const int _seed, _diff, _intense;
        const vector<Op> _ops;
        Op _op;
        int _qtotal, x, y;
        int score; // the number of questions answered correctly
        long int _elapsed;

        time_t _start_time;
        std::stringstream _ss;
        std::vector<int> __row;
        std::vector<long> __lrow;
        std::vector<std::vector<int>> _correct_board;
        std::vector<std::vector<long>> _elapsed_board;
        std::function<int(const int)> fcnPtr;

        // AI 对手群 (duìshǒu qún — opponents)
        vector<AiOpponent*> _ais;     // AI instances (0-3)
        vector<string> _ai_answers;    // per-AI answer for current question
        vector<int> _ai_delays;        // per-AI thinking delay (ms)

        void update_elapsed();
        void update_hit();
        void save_elapsed();
        void save_correct_count();
        void _grade_ai_answers();  // 评分全部 AI 答案
};  // class Manager
