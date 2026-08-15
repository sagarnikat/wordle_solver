#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wordle {

constexpr int kWordLen = 5;
constexpr int kNumOutcomes = 243;  // 3^5 possible feedback patterns

// Feedback encoding: 0 = gray, 1 = yellow, 2 = green,
// packed as a base-3 number (fits in one byte).
using Feedback = uint8_t;

// Compute the Wordle feedback for a (guess, answer) pair. Duplicate letters
// are handled with the standard two-pass algorithm:
//   1. mark all exact (green) matches, removing them from the letter counts,
//   2. mark remaining letters yellow only while a spare copy is left.
Feedback feedbackCode(const std::string& guess, const std::string& answer);

// Convert a feedback code to a readable pattern, e.g. ".g.y.".
std::string codeToPattern(Feedback code);

// Convert a pattern ('.' gray, 'y' yellow, 'g' green) back to a code.
Feedback patternCode(const std::string& pattern);

// Precompute the full feedback matrix: matrix[guess * numAnswers + answer].
std::vector<uint8_t> buildFeedbackMatrix(
    const std::vector<std::string>& guesses,
    const std::vector<std::string>& answers);

}  // namespace wordle