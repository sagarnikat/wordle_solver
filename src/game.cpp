#include "wordle/game.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "wordle/entropy.hpp"
#include "wordle/feedback.hpp"
#include "wordle/ui.hpp"

namespace wordle {

namespace {

using ui::center;
using ui::fixedCol;
using ui::kBold;
using ui::kDim;
using ui::kError;
using ui::kMuted;
using ui::kOk;
using ui::kReset;
using ui::lrpad;
using ui::tile;

constexpr int kMaxGuesses = 6;
constexpr int kMinCols = 52;
constexpr int kMinRows = 20;

// ---------------------------------------------------------------------------
// Shared state and helpers
// ---------------------------------------------------------------------------

struct GameState {
    char board[kMaxGuesses][kWordLen] = {};
    int kinds[kMaxGuesses][kWordLen];
    int keys[26];
    int turn = 0;
    bool over = false;
    bool won = false;
    std::string cur;
    std::string msg;
    int msgKind = 0;  // 0 muted, 1 error, 2 ok
    std::string answer;

    GameState() {
        for (int r = 0; r < kMaxGuesses; ++r)
            for (int c = 0; c < kWordLen; ++c) kinds[r][c] = -1;
        std::fill(std::begin(keys), std::end(keys), -1);
    }
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool isLowerLetters(const std::string& s) {
    return std::all_of(s.begin(), s.end(),
                       [](char c) { return c >= 'a' && c <= 'z'; });
}

std::string statusText(const GameState& s) {
    if (s.msg.empty()) return "";
    if (s.msgKind == 0) return s.msg;
    const char* c = s.msgKind == 1 ? kError : kOk;
    return std::string(c) + s.msg + kReset;
}

// ---------------------------------------------------------------------------
// Board rendering
// ---------------------------------------------------------------------------

const char kH = '-';
const char kV = '|';

std::string hbar(int n) {
    const std::string seg = "\xe2\x94\x80";
    std::string s;
    for (int i = 0; i < n; ++i) s += seg;
    return s;
}

std::string gridLine(int row) {
    const bool color = ui::colorEnabled();
    if (color) {
        const char* c1 = row == 0 ? "\xe2\x94\x8c" : (row == 5 ? "\xe2\x94\x94" : "\xe2\x94\x9c");
        const char* cm = row == 0 ? "\xe2\x94\xac" : (row == 5 ? "\xe2\x94\xb4" : "\xe2\x94\xbc");
        const char* c2 = row == 0 ? "\xe2\x94\x90" : (row == 5 ? "\xe2\x94\x98" : "\xe2\x94\xa4");
        std::string s = c1;
        for (int i = 0; i < kWordLen; ++i) {
            s += "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80";
            if (i < kWordLen - 1) s += cm;
        }
        s += c2;
        return std::string(kDim) + s + kReset;
    }
    std::string s = "+";
    for (int i = 0; i < kWordLen; ++i) {
        s += "---";
        if (i < kWordLen - 1) s += "+";
    }
    s += "+";
    return s;
}

std::string cellAt(const GameState& s, int row, int col) {
    if (row < s.turn) return tile(s.board[row][col], s.kinds[row][col]);
    if (row == s.turn) {
        if (col < static_cast<int>(s.cur.size())) return tile(s.cur[col], -1);
        if (col == static_cast<int>(s.cur.size())) return tile(' ', -2);
        return tile(' ', -1);
    }
    return tile(' ', -1);
}

std::string cellRow(const GameState& s, int row) {
    std::string out = ui::colorEnabled() ? "\xe2\x94\x82" : "|";
    for (int i = 0; i < kWordLen; ++i) {
        out += cellAt(s, row, i);
        out += ui::colorEnabled() ? "\xe2\x94\x82" : "|";
    }
    return out;
}

std::string keyRow(const GameState& s, const char* letters) {
    std::string out;
    for (; *letters; ++letters) out += ui::keyCap(*letters, s.keys[*letters - 'a']);
    return out;
}

const char* kHintsPlay = "a-z spell \xc2\xb7 Enter submit \xc2\xb7 Backspace delete \xc2\xb7 q quit";
const char* kHintsOver = "Enter or q to exit";

// ---------------------------------------------------------------------------
// Play mode
// ---------------------------------------------------------------------------

std::vector<std::string> playFrame(const GameState& s, int cols) {
    std::vector<std::string> out;

    std::string title = tile('W', 2) + tile('O', 2) + tile('R', 2) +
                        tile('D', 2) + tile('L', 2) + tile('E', 2);
    std::string right;
    if (s.over) {
        const char* c = s.won ? kOk : kError;
        right = std::string(c) + (s.won ? "WON " : "LOST ") +
                std::to_string(s.turn) + "/" + std::to_string(kMaxGuesses) +
                kReset;
    } else {
        right = std::string(kMuted) + "Guess " + std::to_string(s.turn + 1) + "/" +
                std::to_string(kMaxGuesses) + kReset;
    }
    out.push_back(lrpad(title, right, cols));
    out.push_back(std::string(kDim) + hbar(cols) + kReset);

    for (int r = 0; r < kMaxGuesses; ++r) {
        out.push_back(gridLine(r));
        out.push_back(cellRow(s, r));
    }

    out.push_back("");
    out.push_back(center(keyRow(s, "qwertyuiop"), cols));
    out.push_back(center(keyRow(s, "asdfghjkl"), cols));
    out.push_back(center(keyRow(s, "zxcvbnm"), cols));

    const std::string hints =
        std::string(kMuted) + (s.over ? kHintsOver : kHintsPlay) + kReset;
    out.push_back(lrpad(statusText(s), hints, cols));
    return out;
}

std::vector<std::string> tooSmallFrame(int cols, int rows) {
    const std::string need = "Need at least " + std::to_string(kMinCols) + "\xc3\x97" +
                             std::to_string(kMinRows) + "  (currently " +
                             std::to_string(cols) + "\xc3\x97" +
                             std::to_string(rows) + ")";
    return {
        "",
        center(std::string(kBold) + "Terminal too small" + kReset, cols),
        center(std::string(kMuted) + need + kReset, cols),
        "",
        center(std::string(kMuted) + "Resize the window, or press q to quit" + kReset, cols),
    };
}

}  // namespace

int playGame(const WordLists& lists) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, lists.answers.size() - 1);
    const std::string answer = lists.answers[dist(gen)];

    const std::set<std::string> valid(lists.guesses.begin(),
                                      lists.guesses.end());

    ui::Session term;
    GameState st;
    st.answer = answer;
    st.msg = "Guess the " + std::to_string(kWordLen) + "-letter word in " +
             std::to_string(kMaxGuesses) + " tries.";

    bool dirty = true;
    for (;;) {
        if (term.resized()) dirty = true;
        if (dirty) {
            const int cols = term.cols();
            const int rows = term.rows();
            if (cols < kMinCols || rows < kMinRows)
                ui::paint(tooSmallFrame(cols, rows), std::max(cols, kMinCols));
            else
                ui::paint(playFrame(st, cols), cols);
            dirty = false;
        }

        const int key = ui::readKey();
        if (key < 0) continue;
        if (key == 0) return 0;

        if (st.over) {
            if (key == '\n' || key == 'q' || key == '\x1b') return 0;
            continue;
        }

        if (key == 'q' || key == '\x1b') return 0;

        if (key == 0x7f) {
            if (!st.cur.empty()) st.cur.pop_back();
            dirty = true;
            continue;
        }

        if (key >= 'a' && key <= 'z') {
            if (static_cast<int>(st.cur.size()) < kWordLen) {
                st.cur += static_cast<char>(key);
                st.msg.clear();
            }
            dirty = true;
            continue;
        }

        if (key == '\n') {
            if (st.cur.size() != kWordLen) {
                st.msg = "Only " + std::to_string(st.cur.size()) + " of " +
                         std::to_string(kWordLen) + " letters \xe2\x80\x94 keep typing";
                st.msgKind = 1;
                dirty = true;
                continue;
            }
            if (!valid.count(st.cur)) {
                st.msg = "\"" + st.cur + "\" is not in the word list";
                st.msgKind = 1;
                dirty = true;
                continue;
            }

            const std::string guess = st.cur;
            const std::string pattern = codeToPattern(feedbackCode(guess, answer));
            for (int i = 0; i < kWordLen; ++i) {
                st.board[st.turn][i] = guess[i];
                const int kind = pattern[i] == 'g' ? 2 : (pattern[i] == 'y' ? 1 : 0);
                st.kinds[st.turn][i] = kind;
                const int idx = guess[i] - 'a';
                if (kind == 0) {
                    if (st.keys[idx] < 0) st.keys[idx] = 0;
                } else {
                    st.keys[idx] = std::max(st.keys[idx], kind);
                }
            }
            ++st.turn;
            st.cur.clear();

            if (guess == answer) {
                st.over = true;
                st.won = true;
                st.msg = "You won in " + std::to_string(st.turn) + "/" +
                         std::to_string(kMaxGuesses) + "!";
                st.msgKind = 2;
            } else if (st.turn == kMaxGuesses) {
                st.over = true;
                st.won = false;
                std::string m = std::string(kError) + "The word was " + kReset;
                for (char c : answer) m += tile(c, -1);
                st.msg = m;
                st.msgKind = 0;
            }
            dirty = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Solver mode
// ---------------------------------------------------------------------------

enum class SolverPhase { Guess, Feedback, Done };

struct SolverState {
    std::vector<int> cands;
    std::vector<RankedGuess> top;
    SolverPhase phase = SolverPhase::Guess;
    std::string guess;
    int feedback[kWordLen];
    int fcount = 0;
    bool haveGuess = false;
    size_t guessIdx = 0;
    std::string answer;
    std::string msg;
    int msgKind = 0;
};

std::string solverStatus(const SolverState& s) {
    if (s.msg.empty()) return "";
    if (s.msgKind == 0) return s.msg;
    const char* c = s.msgKind == 1 ? kError : kOk;
    return std::string(c) + s.msg + kReset;
}

std::vector<std::string> solverFrame(const SolverState& s,
                                     const WordLists& lists, int cols) {
    std::vector<std::string> out;

    std::string title = tile('S', 1) + tile('O', 1) + tile('L', 1) +
                        tile('V', 1) + tile('E', 1) + tile('R', 1);
    std::string right;
    if (s.phase == SolverPhase::Done) {
        right = std::string(kOk) + "Answer found" + kReset;
    } else {
        right = kMuted + std::to_string(s.cands.size()) + "/" +
                std::to_string(lists.answers.size()) + " candidates" + kReset;
    }
    out.push_back(lrpad(title, right, cols));
    out.push_back(std::string(kDim) + hbar(cols) + kReset);

    out.push_back(std::string(kMuted) + "Top guesses (highest entropy):" + kReset);
    for (size_t i = 0; i < s.top.size(); ++i) {
        const RankedGuess& g = s.top[i];
        char bits[32];
        std::snprintf(bits, sizeof bits, "%.4f bits", g.first);
        const std::string left = "  " + std::to_string(i + 1) + "  " + kBold +
                                 lists.guesses[g.second] + kReset;
        const std::string rightBits = std::string(kMuted) + bits + kReset;
        out.push_back(center(fixedCol(left, rightBits, 24), cols));
    }
    out.push_back("");

    // Guess line: typed word as tiles (colored by feedback once entered).
    std::string gline = std::string(kMuted) + "Guess   " + kReset;
    for (int i = 0; i < kWordLen; ++i) {
        if (s.haveGuess) {
            const int kind = i < s.fcount ? s.feedback[i] : -1;
            gline += tile(s.guess[i], kind);
        } else if (i < static_cast<int>(s.guess.size())) {
            gline += tile(s.guess[i], -1);
        } else if (i == static_cast<int>(s.guess.size()) &&
                   s.phase == SolverPhase::Guess) {
            gline += tile(' ', -2);
        } else {
            gline += tile(' ', -1);
        }
    }
    if (s.phase == SolverPhase::Guess && s.guess.empty() && !s.top.empty()) {
        gline += std::string(kDim) + "  (Enter = " + lists.guesses[s.top[0].second] + ")" +
                 kReset;
    }
    out.push_back(center(gline, cols));

    // Pattern line: live feedback entry, or the revealed answer at the end.
    std::string fline = std::string(kMuted) + "Pattern " + kReset;
    if (s.phase == SolverPhase::Done) {
        for (char c : s.answer) fline += tile(c, 2);
    } else {
        for (int i = 0; i < kWordLen; ++i) {
            if (i < s.fcount)
                fline += tile(' ', s.feedback[i]);
            else if (i == s.fcount && s.phase == SolverPhase::Feedback)
                fline += tile(' ', -2);
            else
                fline += tile(' ', -1);
        }
        if (s.phase == SolverPhase::Feedback)
            fline += std::string(kDim) + "  (1/2/3 or g/y/.)" + kReset;
    }
    out.push_back(center(fline, cols));

    const char* hints = "a-z type \xc2\xb7 Enter accept \xc2\xb7 q quit";
    if (s.phase == SolverPhase::Feedback)
        hints = "1/2/3 or g/y/. colors \xc2\xb7 Enter apply \xc2\xb7 Backspace undo \xc2\xb7 Esc back";
    if (s.phase == SolverPhase::Done) hints = "Enter or q to exit";
    out.push_back(lrpad(solverStatus(s), std::string(kMuted) + hints + kReset, cols));
    return out;
}

int solveInteractively(const WordLists& lists,
                       const std::vector<uint8_t>& matrix) {
    const size_t numAnswers = lists.answers.size();
    const std::set<std::string> valid(lists.guesses.begin(),
                                      lists.guesses.end());

    std::vector<int> cands(numAnswers);
    for (size_t i = 0; i < numAnswers; ++i)
        cands[i] = static_cast<int>(i);

    ui::Session term;
    SolverState st;
    st.cands = cands;
    st.top = rankTop(matrix, numAnswers, lists.guesses.size(), cands, 10);
    std::fill(std::begin(st.feedback), std::end(st.feedback), -1);
    st.msg = "Type the word you guessed in Wordle, or press Enter for the best suggestion.";
    st.msgKind = 0;

    bool dirty = true;
    for (;;) {
        if (term.resized()) dirty = true;
        if (dirty) {
            const int cols = term.cols();
            const int rows = term.rows();
            if (cols < kMinCols || rows < kMinRows)
                ui::paint(tooSmallFrame(cols, rows), std::max(cols, kMinCols));
            else
                ui::paint(solverFrame(st, lists, cols), cols);
            dirty = false;
        }

        const int key = ui::readKey();
        if (key < 0) continue;
        if (key == 0) return 0;

        if (st.phase == SolverPhase::Done) {
            if (key == '\n' || key == 'q' || key == '\x1b') return 0;
            continue;
        }

        if (st.phase == SolverPhase::Guess) {
            if (key == 'q') return 0;
            if (key == '\x1b') {
                if (!st.guess.empty()) {
                    st.guess.clear();
                    dirty = true;
                } else {
                    return 0;
                }
                continue;
            }
            if (key == 0x7f) {
                if (!st.guess.empty()) st.guess.pop_back();
                dirty = true;
                continue;
            }
            if (key >= 'a' && key <= 'z') {
                if (static_cast<int>(st.guess.size()) < kWordLen) {
                    st.guess += static_cast<char>(key);
                    st.msg.clear();
                }
                dirty = true;
                continue;
            }
            if (key == '\n') {
                std::string word = st.guess;
                if (word.empty()) {
                    if (st.top.empty()) {
                        st.msg = "No candidates left \xe2\x80\x94 something went wrong";
                        st.msgKind = 1;
                        dirty = true;
                        continue;
                    }
                    word = lists.guesses[st.top[0].second];
                }
                word = toLower(word);
                const auto it = std::find(lists.guesses.begin(),
                                          lists.guesses.end(), word);
                if (it == lists.guesses.end()) {
                    st.msg = "\"" + word + "\" is not a valid guess";
                    st.msgKind = 1;
                    dirty = true;
                    continue;
                }
                st.guess = word;
                st.haveGuess = true;
                st.guessIdx =
                    static_cast<size_t>(it - lists.guesses.begin());
                st.fcount = 0;
                std::fill(std::begin(st.feedback), std::end(st.feedback), -1);
                st.phase = SolverPhase::Feedback;
                st.msg = "Now enter the colors from your game: 1 = gray, 2 = yellow, 3 = green.";
                dirty = true;
            }
            continue;
        }

        // Feedback phase.
        if (key == 'q') return 0;
        if (key == '\x1b') {
            st.phase = SolverPhase::Guess;
            st.haveGuess = false;
            st.guess.clear();
            st.msg = "Re-type the guess (Enter for the best suggestion).";
            dirty = true;
            continue;
        }
        if (key == 0x7f) {
            if (st.fcount > 0) {
                --st.fcount;
                st.feedback[st.fcount] = -1;
            }
            dirty = true;
            continue;
        }
        int kind = -1;
        if (key == '1' || key == '.') kind = 0;
        else if (key == '2' || key == 'y') kind = 1;
        else if (key == '3' || key == 'g') kind = 2;
        if (kind >= 0 && st.fcount < kWordLen) {
            st.feedback[st.fcount++] = kind;
            dirty = true;
            continue;
        }
        if (key == '\n') {
            if (st.fcount != kWordLen) {
                st.msg = "Enter all " + std::to_string(kWordLen) +
                         " colors first";
                st.msgKind = 1;
                dirty = true;
                continue;
            }
            std::string pattern;
            for (int i = 0; i < kWordLen; ++i)
                pattern += st.feedback[i] == 0 ? '.' : (st.feedback[i] == 1 ? 'y' : 'g');
            std::vector<int> next = filterCandidates(
                matrix, numAnswers, st.cands, st.guessIdx, patternCode(pattern));
            if (next.empty()) {
                st.msg = "No answers match that pattern \xe2\x80\x94 check the colors";
                st.msgKind = 1;
                st.fcount = 0;
                std::fill(std::begin(st.feedback), std::end(st.feedback), -1);
                dirty = true;
                continue;
            }
            st.cands = next;
            st.top = rankTop(matrix, numAnswers, lists.guesses.size(),
                             st.cands, 10);
            if (st.cands.size() == 1) {
                st.phase = SolverPhase::Done;
                st.answer = lists.answers[st.cands[0]];
                st.msg = "The answer is " + st.answer + " \xe2\x80\x94 nice!";
                st.msgKind = 2;
            } else {
                st.phase = SolverPhase::Guess;
                st.haveGuess = false;
                st.guess.clear();
                st.msg = std::to_string(st.cands.size()) + " candidates remain.";
                st.msgKind = 2;
            }
            dirty = true;
        }
    }
}

std::string colorize(char c, int kind) { return tile(c, kind); }

}  // namespace wordle
