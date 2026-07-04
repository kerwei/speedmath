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
        Manager(const int diff, const int intense, const vector<Op>& ops, AiLevel ai_level = AiLevel::EASY, bool ai_enabled = true);
        std::string qnext();
        std::string grade_answer(const int answer);
        std::string grade_answer(const string& answer);

        // AI 对手信息 (duìshǒu xìnxī — AI opponent info)
        bool ai_enabled() const { return _ai_enabled; }
        std::string ai_answer() const { return _ai_last_answer; }
        int ai_delay_ms() const { return _ai_last_delay; }
        int ai_score() const { return _ai ? _ai->score() : 0; }
        int ai_total() const { return _ai ? _ai->total() : 0; }
        long ai_elapsed_ms() const { return _ai ? _ai->elapsed_ms() : 0; }

        std::string print_results();

        void updatescore();
        void savehighscore();

    private:
        const int _seed, _diff, _intense;
        const vector<Op> _ops;
        const bool _ai_enabled;
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

        // AI 对手 (duìshǒu — opponent)
        AiOpponent* _ai = nullptr;
        std::string _ai_last_answer;  // AI's answer for current question
        int _ai_last_delay = 0;       // AI's thinking delay for current question

        void update_elapsed();
        void update_hit();
        void save_elapsed();
        void save_correct_count();
        void _grade_ai_answer();  // 评分 AI 答案 (píngfēn dá'àn — grade AI's answer)
};  // class Manager
