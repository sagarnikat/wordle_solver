#include "wordle/feedback.hpp"

namespace wordle {

Feedback feedbackCode(const std::string& guess, const std::string& answer) {
    int counts[26] = {};
    for (char c : answer) counts[c - 'a']++;

    int result[kWordLen] = {};
    for (int i = 0; i < kWordLen; ++i) {
        if (guess[i] == answer[i]) {
            result[i] = 2;
            counts[guess[i] - 'a']--;
        }
    }
    for (int i = 0; i < kWordLen; ++i) {
        if (result[i] == 0 && counts[guess[i] - 'a'] > 0) {
            result[i] = 1;
            counts[guess[i] - 'a']--;
        }
    }

    Feedback code = 0;
    int mul = 1;
    for (int i = 0; i < kWordLen; ++i) {
        code = static_cast<Feedback>(code + result[i] * mul);
        mul *= 3;
    }
    return code;
}

std::string codeToPattern(Feedback code) {
    std::string pattern(kWordLen, '.');
    for (int i = 0; i < kWordLen; ++i) {
        int digit = code % 3;
        pattern[i] = digit == 2 ? 'g' : (digit == 1 ? 'y' : '.');
        code /= 3;
    }
    return pattern;
}

Feedback patternCode(const std::string& pattern) {
    Feedback code = 0;
    int mul = 1;
    for (char c : pattern) {
        int digit = c == 'g' ? 2 : (c == 'y' ? 1 : 0);
        code = static_cast<Feedback>(code + digit * mul);
        mul *= 3;
    }
    return code;
}

std::vector<uint8_t> buildFeedbackMatrix(
    const std::vector<std::string>& guesses,
    const std::vector<std::string>& answers) {
    const size_t numGuesses = guesses.size();
    const size_t numAnswers = answers.size();
    std::vector<uint8_t> matrix(numGuesses * numAnswers);
    for (size_t g = 0; g < numGuesses; ++g)
        for (size_t a = 0; a < numAnswers; ++a)
            matrix[g * numAnswers + a] = feedbackCode(guesses[g], answers[a]);
    return matrix;
}

}  // namespace wordle