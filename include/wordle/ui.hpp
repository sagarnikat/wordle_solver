#pragma once

#include <string>
#include <vector>

namespace wordle::ui {

// Full-screen interactive session: alternate screen + raw mode, with terminal
// state restored on destruction and on SIGINT/SIGTERM/SIGTSTP. Tracks SIGWINCH
// resize events so the caller can repaint.
class Session {
public:
    Session();
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool resized();   // true once since the last call after a SIGWINCH
    int cols() const;  // current terminal width (queried live)
    int rows() const;  // current terminal height
};

// Key read with an idle timeout. Returns:
//   -1   timeout (nothing pressed; poll again / check resize)
//    0   EOF (stdin closed)
//  0x7f  Backspace
//  '\n' Enter
//  '\x1b' Esc
//  anything else: the character itself
int readKey(int timeoutMs = 200);

// Semantic style tokens. Empty strings when colors are disabled, so callers
// can always concatenate them. Honors NO_COLOR / TERM=dumb / non-tty.
bool colorEnabled();
extern const char* kReset;
extern const char* kBold;
extern const char* kDim;
extern const char* kMuted;
extern const char* kError;
extern const char* kOk;
extern const char* kAccent;

// Wordle tile: kind -2 = cursor slot, -1 = unrevealed, 0 = gray, 1 = yellow,
// 2 = green. In monochrome mode the colors become symbols ([s] / (s) / s).
std::string tile(char c, int kind);

// Keyboard keycap; status -1 = unused, 0 = gray, 1 = yellow, 2 = green.
std::string keyCap(char c, int status);

// Length of a string ignoring ANSI escapes.
int visibleLen(const std::string& s);

// Center a (possibly styled) string within `width` cells.
std::string center(const std::string& s, int width);

// left on the left edge, right pushed so it ends at the terminal edge.
std::string lrpad(const std::string& left, const std::string& right, int width);

// left on the left, right aligned so it starts at column `rightCol`.
std::string fixedCol(const std::string& left, const std::string& right,
                     int rightCol);

// Paint a full frame: clears the screen and prints rows padded to `width`.
void paint(const std::vector<std::string>& rows, int width);

}  // namespace wordle::ui
