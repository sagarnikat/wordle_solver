#include "wordle/entropy.hpp"

#include <algorithm>
#include <cmath>

namespace wordle {

double guessEntropy(const std::vector<uint8_t>& matrix, size_t numAnswers,
                    const std::vector<int>& candidates, size_t guessIndex) {
    int counts[kNumOutcomes] = {};
    for (int c : candidates) counts[matrix[guessIndex * numAnswers + c]]++;

    const double n = static_cast<double>(candidates.size());
    double entropy = 0.0;
    for (int i = 0; i < kNumOutcomes; ++i) {
        if (counts[i] == 0) continue;
        const double p = counts[i] / n;
        entropy += p * -std::log2(p);
    }
    return entropy;
}

std::vector<int> filterCandidates(const std::vector<uint8_t>& matrix,
                                  size_t numAnswers,
                                  const std::vector<int>& candidates,
                                  size_t guessIndex, Feedback feedback) {
    std::vector<int> next;
    next.reserve(candidates.size());
    for (int c : candidates)
        if (matrix[guessIndex * numAnswers + c] == feedback)
            next.push_back(c);
    return next;
}

std::vector<RankedGuess> rankTop(const std::vector<uint8_t>& matrix,
                                 size_t numAnswers, size_t numGuesses,
                                 const std::vector<int>& candidates, int n) {
    std::vector<RankedGuess> scores;
    scores.reserve(numGuesses);
    for (size_t g = 0; g < numGuesses; ++g)
        scores.emplace_back(guessEntropy(matrix, numAnswers, candidates, g), g);

    const int k = std::min(n, static_cast<int>(scores.size()));
    std::partial_sort(scores.begin(), scores.begin() + k, scores.end(),
                      [](const RankedGuess& a, const RankedGuess& b) {
                          return a.first > b.first;
                      });
    scores.resize(k);
    return scores;
}

}  // namespace wordle