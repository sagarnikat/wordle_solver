#pragma once

#include <string>
#include <vector>

namespace wordle {

struct WordLists {
    std::vector<std::string> answers;  // words that can actually be the answer
    std::vector<std::string> guesses;  // every legal guess (superset of answers)
};

// Load lowercase 5-letter words from a plain text file, one word per line.
std::vector<std::string> loadWordsFromFile(const std::string& path);

// Locate the data/ directory. Resolution order:
//   1. $WORDLE_DATA_DIR
//   2. <exe-dir>/data and <exe-dir>/../data
//   3. ./data
std::string findDataDir(const std::string& exePath);

// Load answers.txt and guesses.txt from the data directory.
WordLists loadWordLists(const std::string& exePath);

}  // namespace wordle