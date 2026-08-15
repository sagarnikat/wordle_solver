#include "wordle/game.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "wordle/entropy.hpp"
#include "wordle/feedback.hpp"

namespace wordle {

namespace {

constexpr int kMaxGuesses = 6;

const std::string kGreen = "\033[48;5;28m\033[97m";
const std::string kYellow = "\033[48;5;220m\033[30m";
const std::string kGray = "\033[48;5;59m\033[97m";
const std::string kReset = "\033[0m";

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool isLowerLetters(const std::string& s) {
    return std::all_of(s.begin(), s.end(),
                       [](char c) { return c >= 'a' && c <= 'z'; });
}

}  // namespace

std::string colorize(char c, int kind) {
    const std::string& bg = kind == 2 ? kGreen : (kind == 1 ? kYellow : kGray);
    return bg + " " + c + " " + kReset;
}

int playGame(const WordLists& lists) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, lists.answers.size() - 1);
    const std::string answer = lists.answers[dist(gen)];

    const std::set<std::string> valid(lists.guesses.begin(),
                                      lists.guesses.end());

    std::cout << "WORDLE\n";
    std::cout << "Guess the " << kWordLen << "-letter word. You have "
              << kMaxGuesses << " tries.\n\n";

    int letterStatus[26];
    std::fill(std::begin(letterStatus), std::end(letterStatus), -1);

    for (int turn = 0; turn < kMaxGuesses; ++turn) {
        std::string guess;
        while (true) {
            std::cout << "Guess " << (turn + 1) << "/" << kMaxGuesses << ": ";
            if (!std::getline(std::cin, guess)) return 1;
            guess = toLower(guess);

            if (guess.size() != kWordLen) {
                std::cout << "  Word must be " << kWordLen << " letters long.\n";
                continue;
            }
            if (!isLowerLetters(guess)) {
                std::cout << "  Letters only (a-z).\n";
                continue;
            }
            if (!valid.count(guess)) {
                std::cout << "  Not in the word list.\n";
                continue;
            }
            break;
        }

        const std::string pattern = codeToPattern(feedbackCode(guess, answer));

        std::cout << "  ";
        for (int i = 0; i < kWordLen; ++i) {
            const int kind = pattern[i] == 'g' ? 2 : (pattern[i] == 'y' ? 1 : 0);
            std::cout << colorize(guess[i], kind);
        }
        std::cout << "\n";

        for (int i = 0; i < kWordLen; ++i) {
            const int idx = guess[i] - 'a';
            const int kind = pattern[i] == 'g' ? 2 : (pattern[i] == 'y' ? 1 : 0);
            if (kind == 0) {
                if (letterStatus[idx] < 0) letterStatus[idx] = 0;
            } else {
                letterStatus[idx] = std::max(letterStatus[idx], kind);
            }
        }

        std::cout << "  Alphabet: ";
        for (char c = 'a'; c <= 'z'; ++c) {
            const int st = letterStatus[c - 'a'];
            if (st == 2) std::cout << colorize(c, 2);
            else if (st == 1) std::cout << colorize(c, 1);
            else if (st == 0) std::cout << colorize(c, 0);
            else std::cout << ' ' << c << ' ';
        }
        std::cout << "\n";

        if (guess == answer) {
            std::cout << "\nYou won in " << (turn + 1)
                      << (turn == 0 ? " guess!\n" : " guesses!\n");
            return 0;
        }
    }

    std::cout << "\nYou lost. The word was: " << answer << "\n";
    return 0;
}

int solveInteractively(const WordLists& lists,
                       const std::vector<uint8_t>& matrix) {
    const size_t numAnswers = lists.answers.size();

    std::vector<int> cands(numAnswers);
    for (size_t i = 0; i < numAnswers; ++i)
        cands[i] = static_cast<int>(i);

    std::cout << "\nInteractive solver (" << cands.size() << " candidates)\n";
    while (true) {
        const auto top =
            rankTop(matrix, numAnswers, lists.guesses.size(), cands, 10);
        std::cout << "\nTop guesses (highest entropy over " << cands.size()
                  << " candidates):\n";
        for (size_t i = 0; i < top.size(); ++i)
            std::cout << "  " << (i + 1) << "  " << lists.guesses[top[i].second]
                      << "  " << top[i].first << " bits\n";

        if (cands.size() == 1) {
            std::cout << "Answer: " << lists.answers[cands[0]] << "\n";
            return 0;
        }

        std::string guess, pattern;
        std::cout << "Guess used (enter for " << lists.guesses[top[0].second]
                  << "): ";
        if (!std::getline(std::cin, guess)) return 0;
        if (guess.empty()) guess = lists.guesses[top[0].second];
        guess = toLower(guess);
        if (guess.size() != kWordLen) {
            std::cerr << "Invalid guess\n";
            continue;
        }

        std::cout << "Pattern ('.' gray 'y' yellow 'g' green): ";
        if (!std::getline(std::cin, pattern)) return 0;
        pattern = toLower(pattern);
        if (pattern.size() != kWordLen ||
            !std::all_of(pattern.begin(), pattern.end(), [](char c) {
                return c == '.' || c == 'y' || c == 'g';
            })) {
            std::cerr << "Invalid pattern\n";
            continue;
        }

        const auto it = std::find(lists.guesses.begin(), lists.guesses.end(),
                                  guess);
        const size_t gi =
            static_cast<size_t>(it - lists.guesses.begin());
        if (gi == lists.guesses.size()) {
            std::cerr << "Not a valid word\n";
            continue;
        }

        cands = filterCandidates(matrix, numAnswers, cands, gi,
                                 patternCode(pattern));
        if (cands.empty()) {
            std::cerr << "No answers match that pattern\n";
            continue;
        }
        std::cout << "Remaining: " << cands.size() << "\n";
    }
}

}  // namespace wordle