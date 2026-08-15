#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <cstdio>

using namespace std;

const int WORD_LEN = 5;
const int NUM_OUTCOMES = 243;
const int MAX_GRID_ROWS = 6;
const size_t BAR_LEN = 20;

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

struct SolveInfo {
    string answer;
    vector<string> guesses;
    vector<string> patterns;
    vector<int> reductions;
};

// top n guesses (by entropy over cands) -> (entropy, guess index)
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

string timeFmt(double sec) {
    int m = (int)(sec / 60), s = (int)sec % 60;
    char buf[16];
    snprintf(buf, sizeof buf, "%02d:%02d", m, s);
    return buf;
}

void drawGrid(const SolveInfo& s) {
    int rows = (int)s.guesses.size();
    int from = max(0, rows - MAX_GRID_ROWS);
    for (int r = from; r < rows; ++r) {
        for (int c = 0; c < WORD_LEN; ++c) {
            char ch = s.patterns[r][c];
            int bg = ch == 'g' ? 22 : (ch == 'y' ? 178 : 234);
            cout << "\033[1;97;48;5;" << bg << "m " << s.guesses[r][c] << " \033[0m";
        }
        cout << "\n";
    }
}

void drawTop(const SolveInfo& s, int done, int total, bool finished,
             const vector<int>& dist, double avg) {
    cout << "\033[H\033[J";
    cout << "Score: " << s.guesses.size() << "\n";
    cout << "Answer: " << s.answer << "\n";
    cout << "Guesses: [";
    for (int i = 0; i < (int)s.guesses.size(); ++i) {
        if (i) cout << ", ";
        cout << "'" << s.guesses[i] << "'";
    }
    cout << "]\n";
    cout << "Reductions: [";
    for (int i = 0; i < (int)s.reductions.size(); ++i) {
        if (i) cout << ", ";
        cout << s.reductions[i];
    }
    cout << "]\n\n";

    drawGrid(s);
    cout << "\n";

    if (finished) {
        cout << "Distribution: [";
        for (int i = 0; i < (int)dist.size(); ++i) {
            if (i) cout << ", ";
            cout << dist[i];
        }
        cout << "]\n";
        cout << "Average: " << avg << "\n\n";
    } else {
        cout << "Distribution: computing...\n";
        cout << "Average: ---\n\n";
    }
}

void drawProgress(int done, int total,
                  chrono::steady_clock::time_point t0) {
    double el = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
    double pct = 100.0 * done / total;
    int filled = (int)round(pct * BAR_LEN / 100.0);
    string bar;
    for (size_t i = 0; i < BAR_LEN; ++i)
        bar += i < (size_t)filled ? "\u2588" : "\u2591";
    double rate = el > 0 ? done / el : 0;
    double eta = rate > 0 ? (total - done) / rate : 0;

    cout << "\r\033[2KTrying all wordle answers: "
         << (int)round(pct) << "% " << bar << " | "
         << done << "/" << total << " ["
         << timeFmt(el) << "<" << timeFmt(eta) << ", "
         << fixed << setprecision(2) << rate << "it/s]";
    cout.flush();
}

int main(int argc, char* argv[]) {
    cout << "Loading word lists...\n";
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

    vector<int> allCands(A);
    for (size_t i = 0; i < A; ++i) allCands[i] = (int)i;

    string arg = argc > 1 ? argv[1] : "";
    transform(arg.begin(), arg.end(), arg.begin(),
              [](unsigned char c) { return tolower(c); });

    if (arg == "interactive") {
        vector<int> cands = allCands;
        cout << "\nInteractive solver (" << cands.size() << " candidates)\n";
        while (true) {
            vector<pair<double, size_t>> top = rankTop(mat, A, allWords, cands, 10);
            cout << "\nTop guesses (highest entropy over "
                 << cands.size() << " candidates):\n";
            for (int i = 0; i < (int)top.size(); ++i)
                cout << "  " << (i + 1) << "  " << allWords[top[i].second]
                     << "  " << top[i].first << " bits\n";

            if (cands.size() == 1) {
                cout << "Answer: " << answers[cands[0]] << "\n";
                return 0;
            }

            string guess, pattern;
            cout << "Guess used (enter for " << allWords[top[0].second] << "): ";
            getline(cin, guess);
            if (guess.empty()) guess = allWords[top[0].second];
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

            size_t gi = find(allWords.begin(), allWords.end(), guess) - allWords.begin();
            if (gi == G) {
                cerr << "Not a valid word\n";
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
            cout << "Remaining: " << cands.size() << "\n";
        }
    }

    auto t0 = chrono::steady_clock::now();
    SolveInfo info{"", {}, {}, {}};
    drawTop(info, 0, (int)A, false, {}, 0.0);
    drawProgress(0, (int)A, t0);

    vector<pair<double, string>> openRank = loadOpenRank(wordsDir + "openrank.bin");
    if (openRank.empty()) {
        vector<pair<double, size_t>> opening = rankTop(mat, A, allWords, allCands, 1);
        openRank = {{opening[0].first, allWords[opening[0].second]}};
    }
    string openWord = openRank[0].second;
    size_t openG = find(allWords.begin(), allWords.end(), openWord) - allWords.begin();

    vector<int> dist(7, 0);
    long long sumScores = 0;
    int done = 0;

    for (size_t ai = 0; ai < A; ++ai) {
        vector<int> cands = allCands;
        vector<string> guesses, patterns;
        vector<int> reductions;
        size_t g = openG;

        while (cands.size() > 1) {
            uint8_t code = mat[g * A + ai];
            guesses.push_back(allWords[g]);
            patterns.push_back(codeToPattern(code));

            vector<int> next;
            for (int c : cands)
                if (mat[g * A + c] == code)
                    next.push_back(c);
            cands = move(next);
            reductions.push_back((int)cands.size());

            if (cands.size() > 1) {
                vector<pair<double, size_t>> top =
                    rankTop(mat, A, allWords, cands, 1);
                g = top[0].second;
            }
        }

        int score = (int)guesses.size();
        if (score >= (int)dist.size()) dist.resize(score + 1, 0);
        dist[score]++;
        sumScores += score;

        info = {answers[ai], guesses, patterns, reductions};
        ++done;
        drawTop(info, done, (int)A, false, dist, 0.0);
        drawProgress(done, (int)A, t0);
    }

    double avg = (double)sumScores / A;
    drawTop(info, done, (int)A, true, dist, avg);
    drawProgress(done, (int)A, t0);
    cout << "\n\033[0m";
    return 0;
}