#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>

using namespace std;

const int WORD_LEN = 5;
const int NUM_OUTCOMES = 243;

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

uint8_t feedbackCode(const string& guess, const string& answer) {
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
    return (uint8_t)code;
}

string codeToPattern(uint8_t code) {
    string p(WORD_LEN, '.');
    for (int i = 0; i < WORD_LEN; ++i) {
        int d = code % 3;
        p[i] = d == 2 ? 'g' : (d == 1 ? 'y' : '.');
        code /= 3;
    }
    return p;
}

int patternCode(const string& p) {
    int code = 0, mul = 1;
    for (char c : p) {
        int d = c == 'g' ? 2 : (c == 'y' ? 1 : 0);
        code += d * mul;
        mul *= 3;
    }
    return code;
}

// top n guesses by entropy over cands -> (entropy, guess index)
vector<pair<double, size_t>> rankTop(const vector<uint8_t>& mat, size_t A,
                                     const vector<string>& allWords,
                                     const vector<int>& cands, int n) {
    size_t G = allWords.size();
    vector<pair<double, size_t>> scores;
    scores.reserve(G);
    double inv = 1.0 / cands.size();
    uint8_t counts[NUM_OUTCOMES];
    for (size_t g = 0; g < G; ++g) {
        memset(counts, 0, sizeof counts);
        for (int c : cands) counts[mat[g * A + c]]++;
        double e = 0.0;
        for (int i = 0; i < NUM_OUTCOMES; ++i) {
            if (!counts[i]) continue;
            double p = counts[i] * inv;
            e += p * -log2(p);
        }
        scores.push_back({e, g});
    }
    int k = min(n, (int)scores.size());
    partial_sort(scores.begin(), scores.begin() + k, scores.end(),
                 [](const pair<double, size_t>& x, const pair<double, size_t>& y) {
                     return x.first > y.first;
                 });
    scores.resize(k);
    return scores;
}

// opening ranking from openrank.bin (sorted desc: entropy, word)
vector<pair<double, string>> loadOpenRank(const string& binFile) {
    ifstream in(binFile, ios::binary);
    if (!in) return {};
    uint32_t n;
    in.read((char*)&n, sizeof n);
    vector<pair<double, string>> ranked;
    ranked.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        double e;
        char buf[WORD_LEN];
        if (!in.read((char*)&e, sizeof e) || !in.read(buf, WORD_LEN)) break;
        ranked.push_back({e, string(buf, WORD_LEN)});
    }
    return ranked;
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

    size_t A = answers.size(), G = allWords.size();
    cout << "Precomputing feedback matrix (" << G << " x " << A << ")...\n";
    vector<uint8_t> mat(G * A);
    for (size_t g = 0; g < G; ++g)
        for (size_t a = 0; a < A; ++a)
            mat[g * A + a] = feedbackCode(allWords[g], answers[a]);

    vector<int> cands(A);
    for (size_t i = 0; i < A; ++i) cands[i] = (int)i;

    cout << "\nPossible answers: " << cands.size() << "\n";
    cout << "Best guesses to reduce search space:\n";
    vector<pair<double, string>> openRank = loadOpenRank(wordsDir + "openrank.bin");
    for (int i = 0; i < 10 && i < (int)openRank.size(); ++i)
        cout << "  " << (i + 1) << ". " << openRank[i].second
             << "  " << openRank[i].first << " bits\n";

    while (true) {
        string guess, pattern;
        cout << "Guess: ";
        if (!getline(cin, guess)) return 0;
        transform(guess.begin(), guess.end(), guess.begin(),
                  [](unsigned char c) { return tolower(c); });
        if (guess.empty() || guess.size() != WORD_LEN) {
            cerr << "Invalid guess\n";
            continue;
        }
        size_t gi = find(allWords.begin(), allWords.end(), guess) - allWords.begin();
        if (gi == G) {
            cerr << "Not a valid word\n";
            continue;
        }

        cout << "Pattern ('.' gray 'y' yellow 'g' green): ";
        if (!getline(cin, pattern)) return 0;
        transform(pattern.begin(), pattern.end(), pattern.begin(),
                  [](unsigned char c) { return tolower(c); });
        if (pattern.size() != WORD_LEN ||
            !all_of(pattern.begin(), pattern.end(),
                    [](char c) { return c == '.' || c == 'y' || c == 'g'; })) {
            cerr << "Invalid pattern\n";
            continue;
        }

        int code = patternCode(pattern);
        vector<int> next;
        for (int c : cands)
            if (mat[gi * A + c] == (uint8_t)code)
                next.push_back(c);

        if (next.empty()) {
            cerr << "No answers match that pattern\n";
            continue;
        }
        cands = move(next);
        cout << "\nPossible answers: " << cands.size();
        if ((int)cands.size() <= 30) {
            cout << "\n  ";
            for (size_t i = 0; i < cands.size(); ++i)
                cout << answers[cands[i]] << (i + 1 < cands.size() ? " " : "");
        } else {
            cout << " (showing first 10)\n  ";
            for (int i = 0; i < 10; ++i)
                cout << answers[cands[i]] << " ";
        }
        cout << "\n";

        if (cands.size() == 1) {
            cout << "Answer: " << answers[cands[0]] << "\n";
            return 0;
        }

        cout << "Best guesses to reduce search space:\n";
        vector<pair<double, size_t>> top = rankTop(mat, A, allWords, cands, 10);
        for (int i = 0; i < (int)top.size(); ++i)
            cout << "  " << (i + 1) << ". " << allWords[top[i].second]
                 << "  " << top[i].first << " bits\n";
    }
}