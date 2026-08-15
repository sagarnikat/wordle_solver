#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>

using namespace std;

const int WORD_LEN = 5;
const int NUM_OUTCOMES = 243;

// load words from a binary file (5 bytes per word) or JSON fallback
vector<string> loadWords(const string& binFile, const string& jsonFile) {
    ifstream bin(binFile, ios::binary);
    if (bin) {
        vector<string> words;
        char buf[WORD_LEN];
        while (bin.read(buf, WORD_LEN))
            words.emplace_back(buf, WORD_LEN);
        return words;
    }

    ifstream file(jsonFile);
    if (!file) return {};

    string content((istreambuf_iterator<char>(file)),
                        istreambuf_iterator<char>());
    file.close();

    size_t start = content.find('[');
    size_t end = content.rfind(']');
    if (start == string::npos || end == string::npos || end <= start)
        return {};

    string array = content.substr(start + 1, end - start - 1);

    vector<string> words;
    size_t pos = 0;
    while ((pos = array.find('"', pos)) != string::npos) {
        size_t close = array.find('"', pos + 1);
        if (close == string::npos) break;
        string word = array.substr(pos + 1, close - pos - 1);
        if (word.size() == WORD_LEN) words.push_back(word);
        pos = close + 1;
    }
    return words;
}

string findWordsDir(const string& exePath) {
    size_t slash = exePath.find_last_of("/\\");
    if (slash != string::npos) {
        string dir = exePath.substr(0, slash);
        if (!dir.empty()) {
            ifstream test(dir + "/words/possible answers.json");
            if (test) return dir + "/words/";
            test.close();
            ifstream test2(dir + "/../words/possible answers.json");
            if (test2) return dir + "/../words/";
        }
    }
    return "words/";
}

// feedback encoded as base-3 number (0=gray 1=yellow 2=green)
int feedbackCode(const string& guess, const string& answer) {
    int counts[26] = {0};
    for (char c : answer) counts[c - 'a']++;

    int res[WORD_LEN] = {0};
    for (int i = 0; i < WORD_LEN; ++i) {
        if (guess[i] == answer[i]) {
            res[i] = 2;
            counts[guess[i] - 'a']--;
        }
    }
    for (int i = 0; i < WORD_LEN; ++i) {
        if (res[i] == 0) {
            int idx = guess[i] - 'a';
            if (counts[idx] > 0) {
                res[i] = 1;
                counts[idx]--;
            }
        }
    }

    int code = 0, mul = 1;
    for (int i = 0; i < WORD_LEN; ++i) {
        code += res[i] * mul;
        mul *= 3;
    }
    return code;
}

// entropy (average information gain) of a guess over the possible answers
double guessEntropy(const string& guess, const vector<string>& answers) {
    int counts[NUM_OUTCOMES] = {0};
    for (const string& a : answers)
        counts[feedbackCode(guess, a)]++;

    double entropy = 0.0;
    for (int i = 0; i < NUM_OUTCOMES; ++i) {
        if (counts[i] == 0) continue;
        double p = (double)counts[i] / answers.size();
        entropy += p * -log2(p);
    }
    return entropy;
}

// feedback pattern ".yg.." -> base-3 code (0=gray 1=yellow 2=green)
int patternCode(const string& p) {
    int code = 0, mul = 1;
    for (char c : p) {
        int d = c == 'g' ? 2 : (c == 'y' ? 1 : 0);
        code += d * mul;
        mul *= 3;
    }
    return code;
}

// rank guesses by entropy over the current candidate answers
vector<pair<double, string>> rankGuesses(const vector<string>& guesses,
                                         const vector<string>& candidates) {
    vector<pair<double, string>> ranked;
    ranked.reserve(guesses.size());
    for (const string& w : guesses)
        ranked.push_back({guessEntropy(w, candidates), w});
    sort(ranked.begin(), ranked.end(),
         [](const pair<double, string>& x, const pair<double, string>& y) {
             return x.first > y.first;
         });
    return ranked;
}

void printTop(const vector<pair<double, string>>& ranked, int n) {
    for (int i = 0; i < n && i < (int)ranked.size(); ++i)
        cout << "  " << (i + 1) << "  " << ranked[i].second
             << "  " << ranked[i].first << " bits\n";
}

int main(int argc, char* argv[]) {
    string wordsDir = findWordsDir(argc > 0 ? argv[0] : "");
    vector<string> answers = loadWords(wordsDir + "answers.bin",
                                       wordsDir + "possible answers.json");
    vector<string> allWords = loadWords(wordsDir + "possible.bin",
                                        wordsDir + "possible answers + valid words.json");

    if (answers.empty() || allWords.empty()) {
        cerr << "Error: could not load word lists from " << wordsDir << endl;
        return 1;
    }

    vector<string> candidates = answers;
    cout << "Solver: " << candidates.size() << " possible answers\n";

    while (true) {
        cout << "\nTop guesses (highest entropy over "
             << candidates.size() << " candidates):\n";
        vector<pair<double, string>> ranked = rankGuesses(allWords, candidates);
        printTop(ranked, 10);

        if (candidates.size() == 1) {
            cout << "\nAnswer: " << candidates[0] << "\n";
            return 0;
        }

        string guess, pattern;
        cout << "Guess used (enter for " << ranked[0].second << "): ";
        getline(cin, guess);
        if (guess.empty()) guess = ranked[0].second;
        transform(guess.begin(), guess.end(), guess.begin(),
                  [](unsigned char c) { return tolower(c); });
        if (guess.size() != WORD_LEN) {
            cerr << "Invalid guess\n";
            continue;
        }

        cout << "Pattern ('.' gray 'y' yellow 'g' green): ";
        getline(cin, pattern);
        transform(pattern.begin(), pattern.end(), pattern.begin(),
                  [](unsigned char c) { return tolower(c); });
        if (pattern.size() != WORD_LEN ||
            !all_of(pattern.begin(), pattern.end(),
                    [](char c) { return c == '.' || c == 'y' || c == 'g'; })) {
            cerr << "Invalid pattern\n";
            continue;
        }

        int code = patternCode(pattern);
        vector<string> next;
        for (const string& a : candidates)
            if (feedbackCode(guess, a) == code)
                next.push_back(a);

        if (next.empty()) {
            cerr << "No answers match that pattern\n";
            continue;
        }
        candidates = move(next);
        cout << "Remaining: " << candidates.size() << "\n";
    }
}