#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "wordle/word_list.hpp"

namespace wordle {

struct SolveInfo {
    std::string answer;
    std::vector<std::string> guesses;   // guesses actually made
    std::vector<std::string> patterns;  // feedback pattern of each guess
    std::vector<int> remaining;         // candidates left after each guess
};

struct BenchmarkResult {
    int total = 0;
    std::vector<int> distribution;  // distribution[score] = number of answers
    double average = 0.0;
    int maxScore = 0;
    std::string worstAnswer;
};

// Called after each answer is solved: (solve info, answers done, total).
using ProgressFn = std::function<void(const SolveInfo&, int done, int total)>;

// Best opening guess: the word with the highest entropy over all answers.
size_t bestOpeningGuess(const std::vector<uint8_t>& matrix,
                        const WordLists& lists);

// Auto-play a single answer, narrowing the candidate pool after every guess.
SolveInfo solveOne(const std::vector<uint8_t>& matrix, const WordLists& lists,
                   size_t answerIndex, size_t openingGuess);

// Auto-play every possible answer and collect the score distribution.
BenchmarkResult solveAll(const std::vector<uint8_t>& matrix,
                         const WordLists& lists, size_t openingGuess,
                         ProgressFn progress = {});

}  // namespace wordle