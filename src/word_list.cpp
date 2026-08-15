#include "wordle/word_list.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "wordle/feedback.hpp"

namespace wordle {

namespace {

bool isValidWord(const std::string& word) {
    return word.size() == kWordLen &&
           std::all_of(word.begin(), word.end(),
                       [](char c) { return c >= 'a' && c <= 'z'; });
}

bool hasAnswersFile(const std::string& dir) {
    std::ifstream test(dir + "/answers.txt");
    return static_cast<bool>(test);
}

std::string exeDir(const std::string& exePath) {
    size_t slash = exePath.find_last_of("/\\");
    return slash == std::string::npos ? "" : exePath.substr(0, slash);
}

}  // namespace

std::vector<std::string> loadWordsFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};

    std::vector<std::string> words;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::transform(line.begin(), line.end(), line.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (isValidWord(line)) words.push_back(line);
    }
    return words;
}

std::string findDataDir(const std::string& exePath) {
    if (const char* env = std::getenv("WORDLE_DATA_DIR")) {
        if (*env) return env;
    }

    std::string dir = exeDir(exePath);
    if (!dir.empty()) {
        if (hasAnswersFile(dir + "/data")) return dir + "/data";
        if (hasAnswersFile(dir + "/../data")) return dir + "/../data";
    }
    return hasAnswersFile("data") ? "data" : "data";
}

WordLists loadWordLists(const std::string& exePath) {
    const std::string dir = findDataDir(exePath);
    WordLists lists;
    lists.answers = loadWordsFromFile(dir + "/answers.txt");
    lists.guesses = loadWordsFromFile(dir + "/guesses.txt");
    return lists;
}

}  // namespace wordle