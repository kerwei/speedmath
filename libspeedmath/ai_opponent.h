#pragma once

#include <string>

// AI 难度 (nándù — difficulty level)
enum class AiLevel { EASY, MEDIUM, HARD };

// AI 对手 (duìshǒu — opponent) that simulates a player
class AiOpponent {
public:
    AiOpponent(AiLevel level);

    // 决定答案: correct or wrong? returns the chosen answer string
    // 使用当前题目参数 (tímù cānshù — question params)
    std::string decide_answer(int x, int y, char op, int correct_answer, int correct_remainder = 0);

    // AI 的反应延迟, 毫秒 (fǎnyìng yánchí — response delay in ms)
    int delay_ms() const;

    // 获取该局的统计 (tǒngjì — stats)
    int score() const { return _score; }
    int total() const { return _total; }
    long elapsed_ms() const { return _elapsed_ms; }
    AiLevel level() const { return _level; }

    void record(bool correct, int time_ms);

private:
    AiLevel _level;
    double _accuracy;    // 正确率 (zhèngquè lǜ)
    int _min_delay;      // 最小延迟, ms
    int _max_delay;      // 最大延迟, ms
    int _score = 0;
    int _total = 0;
    long _elapsed_ms = 0;

    // 生成一个"看起来合理"的错误答案 (cuòwù dá'àn — plausible wrong answer)
    std::string _wrong_answer(int x, int y, char op, int correct_answer, int correct_remainder);
};
