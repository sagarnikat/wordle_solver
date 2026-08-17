#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "wordle/entropy.hpp"
#include "wordle/feedback.hpp"
#include "wordle/solver.hpp"
#include "wordle/word_list.hpp"

using namespace wordle;

namespace {
int failures = 0;

#define EXPECT(cond)                                                       \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "FAIL: " << #cond << " (line " << __LINE__ << ")\n"; \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

const std::vector<std::string> kAnswers = {
    "abcde", "abode", "trite", "amaze", "error",
    "crane", "slate", "abide", "zebra", "quack",
};
const std::vector<std::string> kGuesses = {
    "abcde", "abode", "abide", "trite", "amaze", "error",
    "crane", "slate", "zebra", "quack", "xyzzy", "aaaaa",
};

std::vector<int> indices(size_t n) {
    std::vector<int> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<int>(i);
    return v;
}
}  // namespace

int main() {
    const auto matrix = buildFeedbackMatrix(kGuesses, kAnswers);
    const size_t A = kAnswers.size();
    const size_t G = kGuesses.size();

    // filterCandidates: a guess with all-green feedback keeps only itself.
    {
        auto it = std::find(kGuesses.begin(), kGuesses.end(), "abcde");
        const size_t gi = static_cast<size_t>(it - kGuesses.begin());
        auto kept = filterCandidates(matrix, A, indices(A), gi,
                                     feedbackCode("abcde", "abcde"));
        EXPECT(kept.size() == 1);
        EXPECT(kAnswers[kept[0]] == "abcde");
    }

    // rankTop returns results sorted by entropy, descending, and caps at n.
    {
        const auto top = rankTop(matrix, A, G, indices(A), 3);
        EXPECT(top.size() == 3);
        for (size_t i = 1; i < top.size(); ++i)
            EXPECT(top[i - 1].first >= top[i].first);
        EXPECT(top[0].second < G);
    }

    // guessEntropy is 0 when the candidate set is a single word.
    {
        EXPECT(guessEntropy(matrix, A, std::vector<int>{0}, 0) == 0.0);
    }

    // Every answer is solved within 6 guesses, and the candidate pool
    // converges to exactly one word.
    {
        const size_t opening = bestOpeningGuess(matrix, {kAnswers, kGuesses});
        const auto result =
            solveAll(matrix, {kAnswers, kGuesses}, opening);
        EXPECT(result.total == static_cast<int>(A));
        EXPECT(result.maxScore <= 6);
        EXPECT(result.maxScore >= 1);
        EXPECT(result.average >= 1.0);
        EXPECT(result.distribution.size() >= 2);

        for (size_t ai = 0; ai < A; ++ai) {
            const auto info = solveOne(matrix, {kAnswers, kGuesses}, ai, opening);
            EXPECT(!info.remaining.empty());
            EXPECT(info.remaining.back() == 1);
            EXPECT(info.guesses.size() <= 6);
            EXPECT(info.guesses.size() == info.patterns.size());
            EXPECT(info.guesses.size() == info.remaining.size());
        }
    }

    // Real word lists (resolved via $WORDLE_DATA_DIR or ./data): the full
    // 2309-word solve must finish under 6 guesses and keep the distribution
    // sane.
    {
        const auto answers = loadWordsFromFile(findDataDir("") + "/answers.txt");
        const auto guesses = loadWordsFromFile(findDataDir("") + "/guesses.txt");
        EXPECT(answers.size() == 2309);
        EXPECT(guesses.size() == 12947);

        const auto full = buildFeedbackMatrix(guesses, answers);
        const size_t opening = bestOpeningGuess(full, {answers, guesses});
        const auto result = solveAll(full, {answers, guesses}, opening);
        EXPECT(result.maxScore <= 6);
        EXPECT(result.average > 0 && result.average < 6);
        EXPECT(result.total == 2309);
    }

    if (failures == 0) {
        std::cout << "test_solver: all tests passed\n";
        return 0;
    }
    std::cerr << "test_solver: " << failures << " test(s) failed\n";
    return 1;
}