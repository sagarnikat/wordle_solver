#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <cctype>

using namespace std;

const int WORD_LEN = 5;

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

int main(int argc, char* argv[]) {
    string wordsDir = findWordsDir(argc > 0 ? argv[0] : "");
    vector<string> answers = loadWords(wordsDir + "possible answers.json");
    vector<string> allWords = loadWords(wordsDir + "possible answers + valid words.json");

    if (answers.empty() || allWords.empty()) {
        cerr << "Error: could not load word lists from " << wordsDir << endl;
        return 1;
    }

    string answer;
    if (argc > 1 && string(argv[1]).size() == WORD_LEN) {
        answer = argv[1];
        transform(answer.begin(), answer.end(), answer.begin(),
                  [](unsigned char c) { return tolower(c); });
        if (!count(answers.begin(), answers.end(), answer)) {
            cerr << "Warning: \"" << answer << "\" is not in the answers list\n";
        }
    } else {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<size_t> dist(0, answers.size() - 1);
        answer = answers[dist(gen)];
        cout << "Random answer: " << answer << "\n\n";
    }

    vector<pair<int, string>> results;
    results.reserve(allWords.size());

    for (const string& guess : allWords) {
        int target = feedbackCode(guess, answer);
        int survived = 0;
        for (const string& a : answers)
            if (feedbackCode(guess, a) == target)
                survived++;
        results.push_back({(int)answers.size() - survived, guess});
    }

    sort(results.begin(), results.end(),
         [](const pair<int, string>& x, const pair<int, string>& y) {
             return x.first > y.first;
         });

    cout << "Answer: " << answer << " (" << answers.size() << " possible answers)\n\n";
    cout << "Best guesses (most words eliminated):\n";
    cout << "  rank  word    eliminated   left\n";
    for (int i = 0; i < 10; ++i) {
        int eliminated = results[i].first;
        cout << "  " << (i + 1) << "     " << results[i].second
             << "     " << eliminated
             << "          " << answers.size() - eliminated << "\n";
    }

    cout << "\nWorst guesses (least words eliminated):\n";
    cout << "  rank  word    eliminated   left\n";
    for (int i = (int)results.size() - 10; i < (int)results.size(); ++i) {
        int eliminated = results[i].first;
        cout << "  " << (results.size() - i)
             << "     " << results[i].second
             << "     " << eliminated
             << "          " << answers.size() - eliminated << "\n";
    }

    return 0;
}