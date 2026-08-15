#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>

using namespace std;

const int WORD_LEN = 5;
const int NUM_OUTCOMES = 243;

vector<string> loadWords(const string& filename) {
    ifstream file(filename);
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

string codeToPattern(int code) {
    string p(WORD_LEN, '.');
    for (int i = 0; i < WORD_LEN; ++i) {
        int d = code % 3;
        p[i] = d == 2 ? 'g' : (d == 1 ? 'y' : '.');
        code /= 3;
    }
    return p;
}

// entropy (average information gain) of a guess over the possible answers
double guessEntropy(const string& guess, const vector<string>& answers,
                    int counts[NUM_OUTCOMES] = nullptr) {
    int local[NUM_OUTCOMES] = {0};
    int* c = counts ? counts : local;
    for (int i = 0; i < NUM_OUTCOMES; ++i) c[i] = 0;
    for (const string& a : answers)
        c[feedbackCode(guess, a)]++;

    double entropy = 0.0;
    for (int i = 0; i < NUM_OUTCOMES; ++i) {
        if (c[i] == 0) continue;
        double p = (double)c[i] / answers.size();
        entropy += p * -log2(p);
    }
    return entropy;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <word> | all\n";
        return 1;
    }

    string wordsDir = findWordsDir(argc > 0 ? argv[0] : "");
    vector<string> answers = loadWords(wordsDir + "possible answers.json");
    vector<string> allWords = loadWords(wordsDir + "possible answers + valid words.json");

    if (answers.empty() || allWords.empty()) {
        cerr << "Error: could not load word lists from " << wordsDir << endl;
        return 1;
    }

    string arg = argv[1];
    transform(arg.begin(), arg.end(), arg.begin(),
              [](unsigned char c) { return tolower(c); });

    if (arg == "all") {
        auto t0 = chrono::high_resolution_clock::now();
        vector<pair<double, string>> ranked;
        ranked.reserve(allWords.size());
        for (const string& w : allWords)
            ranked.push_back({guessEntropy(w, answers), w});
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();

        sort(ranked.begin(), ranked.end(),
             [](const pair<double, string>& x, const pair<double, string>& y) {
                 return x.first > y.first;
             });

        cout << "Tested " << allWords.size() << " valid words against "
             << answers.size() << " answers\n";
        cout << "Total time: " << ms << " ms\n\n";
        cout << "Top 20 guesses, highest entropy first:\n";
        cout << "  rank  word    entropy\n";
        for (int i = 0; i < 20 && i < (int)ranked.size(); ++i)
            cout << "  " << (i + 1) << "     " << ranked[i].second
                 << "     " << ranked[i].first << " bits\n";
        return 0;
    }

    string guess = arg;
    if (guess.size() != WORD_LEN ||
        !all_of(guess.begin(), guess.end(),
                [](char c) { return c >= 'a' && c <= 'z'; })) {
        cerr << "Error: word must be " << WORD_LEN << " lowercase letters\n";
        return 1;
    }

    if (!count(allWords.begin(), allWords.end(), guess)) {
        cerr << "Error: \"" << guess << "\" is not in the valid words list\n";
        return 1;
    }

    auto t0 = chrono::high_resolution_clock::now();

    int counts[NUM_OUTCOMES] = {0};
    double entropy = guessEntropy(guess, answers, counts);

    auto t1 = chrono::high_resolution_clock::now();
    double ms = chrono::duration<double, milli>(t1 - t0).count();

    int reachable = 0;
    for (int i = 0; i < NUM_OUTCOMES; ++i)
        if (counts[i] > 0) reachable++;

    cout << "Guess:          " << guess << "\n";
    cout << "Possible answers: " << answers.size() << "\n";
    cout << "Reachable outcomes: " << reachable << " / " << NUM_OUTCOMES << "\n";
    cout << "Compute time:   " << ms << " ms\n\n";

    cout << "Outcome  eliminated  left   %        I\n";
    cout << "------------------------------------------\n";
    vector<pair<int, string>> rows;
    for (int i = 0; i < NUM_OUTCOMES; ++i)
        if (counts[i] > 0)
            rows.push_back({answers.size() - counts[i], codeToPattern(i)});
    sort(rows.begin(), rows.end(),
         [](const pair<int, string>& x, const pair<int, string>& y) {
             return x.first > y.first;
         });
    double avgPct = 0.0;
    for (auto& row : rows) {
        int left = answers.size() - row.first;
        double p = (double)left / answers.size();
        avgPct += p;
        cout << "  " << row.second << "      " << row.first << "        "
             << left << "     " << (100.0 * p) << "%   "
             << -log2(p) << " bits\n";
    }
    avgPct = 100.0 * avgPct / rows.size();
    cout << "------------------------------------------\n";
    cout << "Average information (entropy): " << entropy << " bits\n";
    cout << "Average outcome percentage:    " << avgPct << "%\n";

    return 0;
}