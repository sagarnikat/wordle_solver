#include "wordle/solver.hpp"

#include <algorithm>

#include "wordle/entropy.hpp"
#include "wordle/feedback.hpp"

namespace wordle {

namespace {

std::vector<int> allCandidates(size_t n) {
    std::vector<int> cands(n);
    for (size_t i = 0; i < n; ++i) cands[i] = static_cast<int>(i);
    return cands;
}

}  // namespace

size_t bestOpeningGuess(const std::vector<uint8_t>& matrix,
                        const WordLists& lists) {
    const auto cands = allCandidates(lists.answers.size());
    const auto top =
        rankTop(matrix, lists.answers.size(), lists.guesses.size(), cands, 1);
    return top.empty() ? 0 : top[0].second;
}

SolveInfo solveOne(const std::vector<uint8_t>& matrix, const WordLists& lists,
                   size_t answerIndex, size_t openingGuess) {
    const size_t numAnswers = lists.answers.size();
    std::vector<int> cands = allCandidates(numAnswers);

    SolveInfo info;
    info.answer = lists.answers[answerIndex];

    size_t g = openingGuess;
    while (cands.size() > 1) {
        const Feedback feedback = matrix[g * numAnswers + answerIndex];
        info.guesses.push_back(lists.guesses[g]);
        info.patterns.push_back(codeToPattern(feedback));

        cands = filterCandidates(matrix, numAnswers, cands, g, feedback);
        info.remaining.push_back(static_cast<int>(cands.size()));

        if (cands.size() > 1) {
            const auto top =
                rankTop(matrix, numAnswers, lists.guesses.size(), cands, 1);
            g = top[0].second;
        }
    }
    return info;
}

BenchmarkResult solveAll(const std::vector<uint8_t>& matrix,
                         const WordLists& lists, size_t openingGuess,
                         ProgressFn progress) {
    const size_t numAnswers = lists.answers.size();

    BenchmarkResult result;
    result.total = static_cast<int>(numAnswers);
    result.distribution.assign(7, 0);

    long long sumScores = 0;
    for (size_t ai = 0; ai < numAnswers; ++ai) {
        const SolveInfo info = solveOne(matrix, lists, ai, openingGuess);

        const int score = static_cast<int>(info.guesses.size());
        if (score >= static_cast<int>(result.distribution.size()))
            result.distribution.resize(score + 1, 0);
        result.distribution[score]++;
        sumScores += score;

        if (score > result.maxScore) {
            result.maxScore = score;
            result.worstAnswer = info.answer;
        }
        if (progress) progress(info, static_cast<int>(ai) + 1, result.total);
    }
    result.average = static_cast<double>(sumScores) / numAnswers;
    return result;
}

}  // namespace wordle