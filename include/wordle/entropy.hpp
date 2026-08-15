#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "wordle/feedback.hpp"

namespace wordle {

// (entropy in bits, guess index into the guess list)
using RankedGuess = std::pair<double, size_t>;

// Expected information gain (entropy, in bits) of guessing `guessIndex`
// over the candidate answers:
//   H = -sum over outcomes p * log2(p)
double guessEntropy(const std::vector<uint8_t>& matrix, size_t numAnswers,
                    const std::vector<int>& candidates, size_t guessIndex);

// Keep only the candidates that produce exactly `feedback` for `guessIndex`.
// A word is still possible iff it yields the same feedback as the real answer,
// so duplicate-letter cases are handled correctly for free.
std::vector<int> filterCandidates(const std::vector<uint8_t>& matrix,
                                  size_t numAnswers,
                                  const std::vector<int>& candidates,
                                  size_t guessIndex, Feedback feedback);

// The `n` best guesses over the candidate set, ranked by entropy (descending).
std::vector<RankedGuess> rankTop(const std::vector<uint8_t>& matrix,
                                 size_t numAnswers, size_t numGuesses,
                                 const std::vector<int>& candidates, int n);

}  // namespace wordle