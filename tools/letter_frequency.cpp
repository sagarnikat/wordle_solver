#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>

int main(int argc, char* argv[]) {
    std::string filename = argc > 1 ? argv[1] : "";

    if (filename.empty()) {
        size_t slash = std::string(argv[0]).find_last_of("/\\");
        std::string dir = slash != std::string::npos
                              ? std::string(argv[0]).substr(0, slash) : ".";
        filename = dir + "/words/possible answers + valid words.json";
        std::ifstream test(filename);
        if (!test) {
            filename = dir + "/../words/possible answers + valid words.json";
            test.open(filename);
        }
        if (!test) filename = "words/possible answers + valid words.json";
    }

    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: could not open file: " << filename << std::endl;
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    size_t start = content.find('[');
    size_t end = content.rfind(']');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        std::cerr << "Error: no array found in file" << std::endl;
        return 1;
    }

    std::string array = content.substr(start + 1, end - start - 1);

    std::vector<std::string> words;
    size_t pos = 0;
    while ((pos = array.find('"', pos)) != std::string::npos) {
        size_t close = array.find('"', pos + 1);
        if (close == std::string::npos) break;
        std::string word = array.substr(pos + 1, close - pos - 1);
        word.erase(std::remove_if(word.begin(), word.end(), ::isspace), word.end());
        if (!word.empty()) words.push_back(word);
        pos = close + 1;
    }

    if (words.empty()) {
        std::cerr << "Error: no words found" << std::endl;
        return 1;
    }

    size_t wordLen = words[0].size();
    for (const auto& w : words) {
        if (w.size() != wordLen) {
            std::cerr << "Error: words of different lengths found (" << w << ")" << std::endl;
            return 1;
        }
    }

    std::vector<std::vector<int>> freq(wordLen, std::vector<int>(26, 0));
    std::vector<int> overall(26, 0);

    for (const auto& w : words) {
        bool seen[26] = {false};
        for (size_t i = 0; i < w.size(); ++i) {
            char c = std::tolower(w[i]);
            if (c >= 'a' && c <= 'z') {
                freq[i][c - 'a']++;
                if (!seen[c - 'a']) {
                    overall[c - 'a']++;
                    seen[c - 'a'] = true;
                }
            }
        }
    }

    std::cout << "Total words: " << words.size() << ", word length: " << wordLen << "\n\n";
    std::cout << "Letter frequency at each position (percent of words):\n";
    std::cout << "======================================================\n";
    for (size_t i = 0; i < wordLen; ++i) {
        std::cout << "Position " << (i + 1) << ":\n";
        for (int j = 0; j < 26; ++j) {
            if (freq[i][j] > 0) {
                double pct = 100.0 * freq[i][j] / words.size();
                std::cout << "  " << (char)('a' + j) << ": " << pct << "%\n";
            }
        }
        std::cout << "\n";
    }

    std::cout << "Overall (percent of words containing the letter):\n";
    std::cout << "==================================================\n";
    for (int j = 0; j < 26; ++j) {
        if (overall[j] > 0) {
            double pct = 100.0 * overall[j] / words.size();
            std::cout << "  " << (char)('a' + j) << ": " << pct << "%\n";
        }
    }

    return 0;
}
