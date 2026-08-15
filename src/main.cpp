#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "wordle/entropy.hpp"
#include "wordle/feedback.hpp"
#include "wordle/game.hpp"
#include "wordle/solver.hpp"
#include "wordle/word_list.hpp"

namespace {

using namespace wordle;

const std::string kUsage =
    "Usage: wordle <command> [args]\n"
    "\n"
    "Commands:\n"
    "  play              Play an interactive game of Wordle\n"
    "  solve             Interactive solver assistant (enter guess + feedback)\n"
    "  analyze <word>    Outcome breakdown and entropy for a single guess\n"
    "  analyze all       Top 20 opening guesses by entropy\n";

WordLists loadListsOrDie(const char* prog) {
    WordLists lists = loadWordLists(prog);
    if (lists.answers.empty() || lists.guesses.empty()) {
        std::cerr << "Error: could not load word lists from data/ "
                     "(set WORDLE_DATA_DIR to override)\n";
        std::exit(1);
    }
    return lists;
}

int cmdAnalyze(const WordLists& lists, const std::vector<uint8_t>& matrix,
               const std::string& arg) {
    const size_t numAnswers = lists.answers.size();

    if (arg == "all") {
        std::vector<int> cands(numAnswers);
        for (size_t i = 0; i < numAnswers; ++i)
            cands[i] = static_cast<int>(i);

        const auto top =
            rankTop(matrix, numAnswers, lists.guesses.size(), cands, 20);
        std::cout << "Tested " << lists.guesses.size() << " valid words against "
                  << numAnswers << " answers\n";
        std::cout << "Top 20 opening guesses, highest entropy first:\n";
        std::cout << "  rank  word      entropy\n";
        for (size_t i = 0; i < top.size(); ++i)
            std::cout << "  " << std::left << std::setw(5) << (i + 1)
                      << std::left << std::setw(10) << lists.guesses[top[i].second]
                      << top[i].first << " bits\n";
        return 0;
    }

    std::string word = arg;
    std::transform(word.begin(), word.end(), word.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (word.size() != kWordLen ||
        !std::all_of(word.begin(), word.end(),
                     [](char c) { return c >= 'a' && c <= 'z'; })) {
        std::cerr << "Error: word must be " << kWordLen
                  << " lowercase letters\n";
        return 1;
    }

    const auto it = std::find(lists.guesses.begin(), lists.guesses.end(), word);
    if (it == lists.guesses.end()) {
        std::cerr << "Error: \"" << word << "\" is not in the valid words list\n";
        return 1;
    }
    const size_t gi = static_cast<size_t>(it - lists.guesses.begin());

    auto t0 = std::chrono::high_resolution_clock::now();
    int counts[kNumOutcomes] = {};
    for (size_t a = 0; a < numAnswers; ++a) counts[matrix[gi * numAnswers + a]]++;
    auto t1 = std::chrono::high_resolution_clock::now();

    std::vector<int> cands(numAnswers);
    for (size_t i = 0; i < numAnswers; ++i)
        cands[i] = static_cast<int>(i);
    const double entropy = guessEntropy(matrix, numAnswers, cands, gi);

    int reachable = 0;
    for (int i = 0; i < kNumOutcomes; ++i)
        if (counts[i] > 0) reachable++;

    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "Guess:            " << word << "\n";
    std::cout << "Possible answers: " << numAnswers << "\n";
    std::cout << "Reachable outcomes: " << reachable << " / " << kNumOutcomes
              << "\n";
    std::cout << "Compute time:     " << ms << " ms\n\n";

    std::cout << "Outcome  eliminated  left   %        I\n";
    std::cout << "------------------------------------------\n";
    std::vector<std::pair<int, std::string>> rows;
    for (int i = 0; i < kNumOutcomes; ++i)
        if (counts[i] > 0)
            rows.push_back(
                {static_cast<int>(numAnswers) - counts[i], codeToPattern(i)});
    std::sort(rows.begin(), rows.end(),
              [](const std::pair<int, std::string>& a,
                 const std::pair<int, std::string>& b) {
                  return a.first > b.first;
              });

    for (const auto& row : rows) {
        const int left = static_cast<int>(numAnswers) - row.first;
        const double p = static_cast<double>(left) / numAnswers;
        std::cout << "  " << row.second << "      " << row.first << "        "
                  << left << "     " << std::fixed << std::setprecision(2)
                  << (100.0 * p) << "%   " << std::setprecision(4)
                  << -std::log2(p) << " bits\n";
    }
    std::cout << "------------------------------------------\n";
    std::cout << "Average information (entropy): " << std::fixed
              << std::setprecision(4) << entropy << " bits\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << kUsage;
        return 1;
    }

    const std::string cmd = argv[1];

    if (cmd == "play") {
        return playGame(loadListsOrDie(argv[0]));
    }

    if (cmd == "solve") {
        const WordLists lists = loadListsOrDie(argv[0]);
        const auto matrix = buildFeedbackMatrix(lists.guesses, lists.answers);
        return solveInteractively(lists, matrix);
    }

    if (cmd == "analyze") {
        if (argc < 3) {
            std::cout << kUsage;
            return 1;
        }
        const WordLists lists = loadListsOrDie(argv[0]);
        const auto matrix = buildFeedbackMatrix(lists.guesses, lists.answers);
        return cmdAnalyze(lists, matrix, argv[2]);
    }

    std::cout << kUsage;
    return 1;
}