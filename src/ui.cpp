#include "wordle/ui.hpp"

#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace wordle::ui {

namespace {

termios g_orig;
bool g_raw = false;
volatile sig_atomic_t g_resized = 0;
int g_cols = 80;
int g_rows = 24;

const char* kEnterAlt = "\033[?1049h";
const char* kLeaveAlt = "\033[?1049l";
const char* kHideCursor = "\033[?25l";
const char* kShowCursor = "\033[?25h";
const char* kClear = "\033[2J\033[H";

void writeAll(const char* s, size_t n) {
    while (n > 0) {
        ssize_t w = write(STDOUT_FILENO, s, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            return;
        }
        s += w;
        n -= static_cast<size_t>(w);
    }
}

void enterAltScreen() {
    writeAll(kEnterAlt, std::strlen(kEnterAlt));
    writeAll(kHideCursor, std::strlen(kHideCursor));
}

void leaveAltScreen() {
    writeAll(kLeaveAlt, std::strlen(kLeaveAlt));
    writeAll(kShowCursor, std::strlen(kShowCursor));
}

bool setRaw(bool on) {
    if (!isatty(STDIN_FILENO)) return false;
    if (on) {
        if (g_raw) return true;
        if (tcgetattr(STDIN_FILENO, &g_orig) != 0) return false;
        termios raw = g_orig;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_iflag &= static_cast<tcflag_t>(~(ICRNL | INLCR | IGNCR | IXON));
        raw.c_oflag &= static_cast<tcflag_t>(~OPOST);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return false;
        g_raw = true;
        return true;
    }
    if (!g_raw) return true;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig);
    g_raw = false;
    return true;
}

void restoreAll() {
    setRaw(false);
    leaveAltScreen();
}

void onExit(int sig) {
    if (sig == SIGWINCH) {
        g_resized = 1;
        return;
    }
    restoreAll();
    _exit(128 + sig);
}

void onTstp(int) {
    restoreAll();
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
}

void onCont(int) {
    enterAltScreen();
    setRaw(true);
    g_resized = 1;
}

void updateSize() {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 &&
        ws.ws_row > 0) {
        g_cols = ws.ws_col;
        g_rows = ws.ws_row;
    }
}

}  // namespace

bool colorEnabled() {
    static const bool enabled = [] {
        if (getenv("NO_COLOR") != nullptr) return false;
        if (!isatty(STDOUT_FILENO)) return false;
        const char* t = getenv("TERM");
        if (t != nullptr && std::strcmp(t, "dumb") == 0) return false;
        return true;
    }();
    return enabled;
}

const bool g_color = colorEnabled();

const char* kReset = g_color ? "\033[0m" : "";
const char* kBold = g_color ? "\033[1m" : "";
const char* kDim = g_color ? "\033[2m" : "";
const char* kMuted = g_color ? "\033[38;5;244m" : "";
const char* kError = g_color ? "\033[38;5;196m" : "";
const char* kOk = g_color ? "\033[38;5;82m" : "";
const char* kAccent = g_color ? "\033[38;5;220m" : "";

Session::Session() {
    setRaw(true);
    enterAltScreen();
    updateSize();

    struct sigaction sa {};
    sa.sa_handler = onExit;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGWINCH, &sa, nullptr);
    sa.sa_handler = onTstp;
    sigaction(SIGTSTP, &sa, nullptr);
    sa.sa_handler = onCont;
    sigaction(SIGCONT, &sa, nullptr);
}

Session::~Session() {
    restoreAll();
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGWINCH, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
}

bool Session::resized() {
    const bool r = g_resized != 0;
    g_resized = 0;
    return r;
}

int Session::cols() const {
    updateSize();
    return g_cols;
}

int Session::rows() const {
    updateSize();
    return g_rows;
}

int readKey(int timeoutMs) {
    pollfd p{STDIN_FILENO, POLLIN, 0};
    const int r = poll(&p, 1, timeoutMs);
    if (r == 0) return -1;
    if (r < 0) return errno == EINTR ? -1 : 0;

    char c = 0;
    while (read(STDIN_FILENO, &c, 1) != 1) {
        if (errno == EINTR) continue;
        return 0;
    }

    if (c == '\x1b') {
        pollfd pend{STDIN_FILENO, POLLIN, 0};
        if (poll(&pend, 1, 20) > 0) {
            char buf[8];
            while (read(STDIN_FILENO, buf, sizeof buf) > 0 &&
                   poll(&pend, 1, 5) > 0) {
            }
            return -1;
        }
        return '\x1b';
    }
    if (c == '\r') return '\n';
    if (c == 0x7f || c == 0x08) return 0x7f;
    return static_cast<unsigned char>(c);
}

std::string tile(char c, int kind) {
    if (g_color) {
        const char* bg;
        const char* fg;
        switch (kind) {
            case 2: bg = "\033[48;5;28m"; fg = "\033[38;5;15m"; break;
            case 1: bg = "\033[48;5;220m"; fg = "\033[38;5;16m"; break;
            case 0: bg = "\033[48;5;59m"; fg = "\033[38;5;15m"; break;
            case -2: bg = "\033[48;5;250m"; fg = "\033[38;5;0m"; break;
            default: bg = "\033[48;5;236m"; fg = "\033[38;5;242m"; break;
        }
        return std::string(bg) + fg + " " + c + " " + kReset;
    }
    const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (kind) {
        case 2: return std::string("[") + up + "]";
        case 1: return std::string("(") + up + ")";
        case 0: return std::string(" ") + c + " ";
        case -2: return " _ ";
        default: return c == ' ' ? " . " : std::string(" ") + c + " ";
    }
}

std::string keyCap(char c, int status) {
    if (g_color) {
        const char* bg = "\033[48;5;237m";
        const char* fg = "\033[38;5;245m";
        switch (status) {
            case 2: bg = "\033[48;5;28m"; fg = "\033[38;5;15m"; break;
            case 1: bg = "\033[48;5;220m"; fg = "\033[38;5;16m"; break;
            case 0: bg = "\033[48;5;59m"; fg = "\033[38;5;15m"; break;
        }
        return std::string(bg) + fg + " " + c + " " + kReset;
    }
    const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (status) {
        case 2: return std::string("[") + up + "]";
        case 1: return std::string("(") + up + ")";
        case 0: return std::string(kDim) + " " + c + " " + kReset;
        default: return std::string(" ") + c + " ";
    }
}

int visibleLen(const std::string& s) {
    int n = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\033') {
            i += 2;
            while (i < s.size() && s[i] != 'm') ++i;
        } else {
            ++n;
        }
    }
    return n;
}

std::string center(const std::string& s, int width) {
    const int pad = (width - visibleLen(s)) / 2;
    return (pad > 0 ? std::string(pad, ' ') : "") + s;
}

std::string lrpad(const std::string& left, const std::string& right, int width) {
    int gap = width - visibleLen(left) - visibleLen(right);
    if (gap < 1) gap = 1;
    return left + std::string(gap, ' ') + right;
}

std::string fixedCol(const std::string& left, const std::string& right,
                     int rightCol) {
    int pad = rightCol - visibleLen(left);
    if (pad < 1) pad = 1;
    return left + std::string(pad, ' ') + right;
}

void paint(const std::vector<std::string>& rows, int width) {
    std::string out = kClear;
    for (const auto& row : rows) {
        out += row;
        const int w = visibleLen(row);
        if (w < width) out.append(static_cast<size_t>(width - w), ' ');
        out += "\r\n";
    }
    out += kHideCursor;
    writeAll(out.data(), out.size());
}

}  // namespace wordle::ui
