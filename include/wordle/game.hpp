#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "wordle/word_list.hpp"

namespace wordle {

// Interactive Wordle: you guess, the program colors each letter and keeps a
// running alphabet of known gray / yellow / green letters.
int playGame(const WordLists& lists);

// Interactive solver assistant: you enter each guess and its feedback pattern
// ('.' gray, 'y' yellow, 'g' green); it narrows the candidates and suggests
// the best next guesses by entropy.
int solveInteractively(const WordLists& lists,
                       const std::vector<uint8_t>& matrix);

// ANSI-colored tile for a single letter: kind 0 = gray, 1 = yellow, 2 = green.
std::string colorize(char c, int kind);

}  // namespace wordle