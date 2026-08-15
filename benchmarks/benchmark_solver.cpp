#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "wordle/entropy.hpp"
#include "wordle/feedback.hpp"
#include "wordle/solver.hpp"
#include "wordle/word_list.hpp"

using namespace wordle;

namespace {

constexpr size_t kBarLen = 20;
constexpr size_t kMaxGridRows = 6;

std::string timeFmt(double sec) {
    const int m = static_cast<int>(sec / 60);
    const int s = static_cast<int>(sec) % 60;
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", m, s);
    return buf;
}

std::string elapsedMs(std::chrono::steady_clock::time_point t0) {
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f ms", ms);
    return buf;
}

void drawGrid(const SolveInfo& info) {
    const size_t from =
        info.guesses.size() > kMaxGridRows ? info.guesses.size() - kMaxGridRows : 0;
    for (size_t r = from; r < info.guesses.size(); ++r) {
        for (int c = 0; c < kWordLen; ++c) {
            const char ch = info.patterns[r][c];
            const int bg = ch == 'g' ? 22 : (ch == 'y' ? 178 : 234);
            std::cout << "\033[1;97;48;5;" << bg << "m " << info.guesses[r][c]
                      << " \033[0m";
        }
        std::cout << "\n";
    }
}

void drawTop(const SolveInfo& info, int done, int total, bool finished,
             const std::vector<int>& dist, double avg) {
    std::cout << "\033[H\033[J";
    std::cout << "Score: " << info.guesses.size() << "\n";
    std::cout << "Answer: " << info.answer << "\n";

    std::cout << "Guesses: [";
    for (size_t i = 0; i < info.guesses.size(); ++i) {
        if (i) std::cout << ", ";
        std::cout << "'" << info.guesses[i] << "'";
    }
    std::cout << "]\n";

    std::cout << "Reductions: [";
    for (size_t i = 0; i < info.remaining.size(); ++i) {
        if (i) std::cout << ", ";
        std::cout << info.remaining[i];
    }
    std::cout << "]\n\n";

    drawGrid(info);
    std::cout << "\n";

    if (finished) {
        std::cout << "Distribution: [";
        for (size_t i = 0; i < dist.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << dist[i];
        }
        std::cout << "]\n";
        std::cout << "Average: " << avg << "\n\n";
    } else {
        std::cout << "Distribution: computing...\n";
        std::cout << "Average: ---\n\n";
    }
}

void drawProgress(int done, int total, std::chrono::steady_clock::time_point t0) {
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
            .count();
    const double pct = 100.0 * done / total;
    const int filled = static_cast<int>(std::round(pct * kBarLen / 100.0));

    std::string bar;
    bar.reserve(kBarLen);
    for (size_t i = 0; i < kBarLen; ++i)
        bar += i < static_cast<size_t>(filled) ? "\u2588" : "\u2591";

    const double rate = elapsed > 0 ? done / elapsed : 0;
    const double eta = rate > 0 ? (total - done) / rate : 0;

    std::cout << "\r\033[2KTrying all wordle answers: "
              << static_cast<int>(std::round(pct)) << "% " << bar << " | "
              << done << "/" << total << " ["
              << timeFmt(elapsed) << "<" << timeFmt(eta) << ", " << std::fixed
              << std::setprecision(2) << rate << "it/s]";
    std::cout.flush();
}

void printSummary(const BenchmarkResult& result,
                  std::chrono::steady_clock::time_point t0) {
    std::cout << "\n\033[0m";
    std::cout << "Solved every one of the " << result.total
              << " answers.\n\n";
    std::cout << "Guess-count distribution:\n";
    const int maxCount = *std::max_element(result.distribution.begin(),
                                           result.distribution.end());
    for (size_t score = 0; score < result.distribution.size(); ++score) {
        if (result.distribution[score] == 0) continue;
        const int width = 40 * result.distribution[score] / maxCount;
        std::cout << "  " << score << (score == 1 ? " guess : " : " guesses: ")
                  << std::right << std::setw(4) << result.distribution[score]
                  << "  ";
        for (int i = 0; i < width; ++i) std::cout << "\u2588";
        std::cout << "\n";
    }

    const double pctSix =
        100.0 * (result.total - result.distribution.back()) / result.total;
    std::cout << "\n";
    std::cout << "Average guesses:    " << std::fixed << std::setprecision(2)
              << result.average << "\n";
    std::cout << "Worst case:         " << result.maxScore << " guesses (answer: \""
              << result.worstAnswer << "\")\n";
    std::cout << "Solved in <= 6:     " << std::setprecision(1) << pctSix << "%\n";
    std::cout << "Total time:         " << timeFmt(std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - t0).count()) << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    const bool plain = argc > 1 && std::string(argv[1]) == "--plain";
    const auto wall0 = std::chrono::steady_clock::now();

    const WordLists lists = loadWordLists(argv[0]);
    if (lists.answers.empty() || lists.guesses.empty()) {
        std::cerr << "Error: could not load word lists from data/ "
                     "(set WORDLE_DATA_DIR to override)\n";
        return 1;
    }
    std::cout << "Answers: " << lists.answers.size()
              << ", guesses: " << lists.guesses.size() << "\n";

    std::cout << "Precomputing feedback matrix ("
              << lists.guesses.size() << " x " << lists.answers.size()
              << ")...\n";
    auto t0 = std::chrono::steady_clock::now();
    const auto matrix = buildFeedbackMatrix(lists.guesses, lists.answers);
    std::cout << "Matrix done in " << elapsedMs(t0) << "\n";

    std::cout << "Computing best opening guess...\n";
    t0 = std::chrono::steady_clock::now();
    const size_t opening = bestOpeningGuess(matrix, lists);
    std::cout << "Opening guess: " << lists.guesses[opening] << " ("
              << elapsedMs(t0) << ")\n\n";

    SolveInfo current;
    BenchmarkResult result;
    if (plain) {
        result = solveAll(matrix, lists, opening);
    } else {
        drawTop(current, 0, result.total, false, {}, 0.0);
        drawProgress(0, result.total, wall0);
        result = solveAll(matrix, lists, opening,
                          [&](const SolveInfo& info, int done, int total) {
                              current = info;
                              drawTop(current, done, total, false, {}, 0.0);
                              drawProgress(done, total, wall0);
                          });
    }

    if (plain) {
        printSummary(result, wall0);
    } else {
        drawTop(current, result.total, result.total, true,
                result.distribution, result.average);
        printSummary(result, wall0);
    }
    return 0;
}