#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
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

    cout << "Ranking " << G << " guesses by entropy over all answers...\n";
    vector<pair<double, string>> ranked;
    ranked.reserve(G);
    double inv = 1.0 / A;
    uint8_t counts[NUM_OUTCOMES];
    for (size_t g = 0; g < G; ++g) {
        memset(counts, 0, sizeof counts);
        for (size_t a = 0; a < A; ++a) counts[mat[g * A + a]]++;
        double e = 0.0;
        for (int i = 0; i < NUM_OUTCOMES; ++i) {
            if (!counts[i]) continue;
            double p = counts[i] * inv;
            e += p * -log2(p);
        }
        ranked.push_back({e, allWords[g]});
    }
    sort(ranked.begin(), ranked.end(),
         [](const pair<double, string>& x, const pair<double, string>& y) {
             return x.first > y.first;
         });

    string outFile = wordsDir + "openrank.bin";
    ofstream out(outFile, ios::binary);
    if (!out) {
        cerr << "Error: cannot write " << outFile << endl;
        return 1;
    }
    uint32_t n = (uint32_t)ranked.size();
    out.write((const char*)&n, sizeof n);
    for (auto& r : ranked) {
        double e = r.first;
        out.write((const char*)&e, sizeof e);
        out.write(r.second.c_str(), WORD_LEN);
    }
    out.close();

    cout << "Wrote " << n << " ranked guesses to " << outFile << "\n";
    cout << "Top 10:\n";
    for (int i = 0; i < 10 && i < (int)ranked.size(); ++i)
        cout << "  " << (i + 1) << ". " << ranked[i].second
             << "  " << ranked[i].first << " bits\n";
    return 0;
}