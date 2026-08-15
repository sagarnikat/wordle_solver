#include <iostream>
#include <string>

#include "wordle/feedback.hpp"

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
}  // namespace

int main() {
    // Exact match: every tile green. "ggggg" == 2 * (81+27+9+3+1) = 242.
    EXPECT(feedbackCode("abcde", "abcde") == 242);
    EXPECT(codeToPattern(242) == "ggggg");
    EXPECT(patternCode("ggggg") == 242);

    // No shared letters: all gray.
    EXPECT(feedbackCode("zzzzz", "abcde") == 0);
    EXPECT(codeToPattern(0) == ".....");

    // Mixed case, hand-computed: hello vs world -> "...gy" (code 135).
    EXPECT(feedbackCode("hello", "world") == 135);
    EXPECT(codeToPattern(135) == "...gy");
    EXPECT(patternCode("...gy") == 135);

    // Non-adjacent greens: crane vs slate -> "..g.g" (code 180).
    EXPECT(feedbackCode("crane", "slate") == 180);

    // Duplicate letters in the guess: aaccc vs abcde -> "g.g.." (code 20).
    EXPECT(feedbackCode("aaccc", "abcde") == 20);

    // Duplicate letters, only one match: aaaaa vs happy -> "g...." (code 6).
    EXPECT(feedbackCode("aaaaa", "happy") == 6);

    // Repeated letter in the answer: happy vs pappy -> ".gggg" (code 240).
    EXPECT(feedbackCode("happy", "pappy") == 240);

    // Code <-> pattern round trip over all 243 outcomes.
    for (int code = 0; code < kNumOutcomes; ++code)
        EXPECT(patternCode(codeToPattern(static_cast<Feedback>(code))) == code);

    // Invalid chars are treated as gray.
    EXPECT(patternCode("gg...") == 2 + 2 * 3);
    EXPECT(patternCode("...gg") == 2 * 27 + 2 * 81);
    EXPECT(patternCode(".y.g.") == 57);

    if (failures == 0) {
        std::cout << "test_feedback: all tests passed\n";
        return 0;
    }
    std::cerr << "test_feedback: " << failures << " test(s) failed\n";
    return 1;
}