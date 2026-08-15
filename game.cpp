#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <cctype>
#include <algorithm>
#include <random>

using namespace std;

const int MAX_GUESSES = 6;
const int WORD_LEN = 5;

const string GREEN = "\033[48;5;28m\033[97m";
const string YELLOW = "\033[48;5;220m\033[30m";
const string GRAY = "\033[48;5;59m\033[97m";
const string RESET = "\033[0m";

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
        if (!dir.empty()) return dir + "/words/";
    }
    return "words/";
}

string colorFor(char c, int kind) {
    switch (kind) {
        case 0: return GRAY;
        case 1: return YELLOW;
        default: return GREEN;
    }
}

int main(int argc, char* argv[]) {
    string wordsDir = findWordsDir(argc > 0 ? argv[0] : "");
    vector<string> answers = loadWords(wordsDir + "possible answers.json");
    vector<string> allWords = loadWords(wordsDir + "possible answers + valid words.json");

    if (answers.empty() || allWords.empty()) {
        cerr << "Error: could not load word lists from " << wordsDir << endl;
        return 1;
    }

    set<string> validSet(allWords.begin(), allWords.end());

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<size_t> dist(0, answers.size() - 1);
    string answer = answers[dist(gen)];

    cout << "WORDLE\n";
    cout << "Guess the " << WORD_LEN << "-letter word. You have "
              << MAX_GUESSES << " tries.\n\n";

    int letterStatus[26];
    for (int i = 0; i < 26; ++i) letterStatus[i] = -1;

    for (int turn = 0; turn < MAX_GUESSES; ++turn) {
        string guess;
        while (true) {
            cout << "Guess " << (turn + 1) << "/" << MAX_GUESSES << ": ";
            if (!getline(cin, guess)) return 1;

            transform(guess.begin(), guess.end(), guess.begin(),
                           [](unsigned char c) { return tolower(c); });
            guess.erase(remove_if(guess.begin(), guess.end(), ::isspace), guess.end());

            if (guess.size() != WORD_LEN) {
                cout << "  Word must be " << WORD_LEN << " letters long.\n";
                continue;
            }
            if (any_of(guess.begin(), guess.end(),
                            [](char c) { return c < 'a' || c > 'z'; })) {
                cout << "  Letters only (a-z).\n";
                continue;
            }
            if (!validSet.count(guess)) {
                cout << "  Not in the word list.\n";
                continue;
            }
            break;
        }

        int result[WORD_LEN] = {0};
        int counts[26] = {0};
        for (char c : answer) counts[c - 'a']++;

        for (int i = 0; i < WORD_LEN; ++i) {
            if (guess[i] == answer[i]) {
                result[i] = 2;
                counts[guess[i] - 'a']--;
            }
        }
        for (int i = 0; i < WORD_LEN; ++i) {
            if (result[i] == 0) {
                int idx = guess[i] - 'a';
                if (counts[idx] > 0) {
                    result[i] = 1;
                    counts[idx]--;
                }
            }
        }

        cout << "  ";
        for (int i = 0; i < WORD_LEN; ++i) {
            cout << colorFor(guess[i], result[i]) << ' ' << guess[i] << ' ' << RESET;
        }
        cout << "\n";

        for (int i = 0; i < WORD_LEN; ++i) {
            int idx = guess[i] - 'a';
            if (result[i] == 0) {
                if (letterStatus[idx] < 0) letterStatus[idx] = 0;
            } else {
                letterStatus[idx] = max(letterStatus[idx], result[i]);
            }
        }

        cout << "  Alphabet: ";
        for (char c = 'a'; c <= 'z'; ++c) {
            int st = letterStatus[c - 'a'];
            if (st == 2) cout << GREEN << ' ' << c << ' ' << RESET;
            else if (st == 1) cout << YELLOW << ' ' << c << ' ' << RESET;
            else if (st == 0) cout << GRAY << ' ' << c << ' ' << RESET;
            else cout << ' ' << c << ' ';
        }
        cout << "\n";

        if (guess == answer) {
            cout << "\nYou won in " << (turn + 1) << (turn == 0 ? " guess!\n" : " guesses!\n");
            return 0;
        }
    }

    cout << "\nYou lost. The word was: " << answer << "\n";
    return 0;
}
