# Wordle Solver

A C++17 Wordle game and information-theory solver. The solver guesses every
word in the official answer list in **2.64 guesses on average** (worst case 5),
never losing a single game out of 2,309.

## Problem statement

Wordle is a word-guessing game:

- A secret 5-letter word is chosen from a list of ~2,300 common answers.
- You have 6 guesses. Every letter of a guess is colored:
  - **green** — letter is correct and in the right position,
  - **yellow** — letter is in the word but in the wrong position,
  - **gray** — letter is not in the word.
- Duplicate letters follow strict rules: if the answer has only one `e` and you
  guess two, only one can be yellow (the other is gray).

The goal of this project is a program that, given the colored feedback of each
guess, narrows down the answer and picks the best next guess — and can solve
**any** game within 6 tries, ideally in 3–4.

## Algorithm

### 1. Feedback model

Feedback is computed for a `(guess, answer)` pair with a two-pass algorithm:

1. Mark all exact (green) matches, removing them from the letter counts.
2. For the remaining positions, mark yellow **only while a spare copy** of the
   letter is left in the answer; otherwise gray.

The key insight: **the feedback is fully determined by the pair `(guess,
answer)`**. So each feedback pattern is encoded as a base-3 number
(0 = gray, 1 = yellow, 2 = green) that fits in one byte — there are only
`3^5 = 243` possible outcomes. The whole game is precomputed into a single
feedback matrix `matrix[guess][answer]` (12,947 × 2,309 bytes).

### 2. Pruning the candidate pool

A word is still possible **if and only if it produces the exact same feedback
as the real answer**:

```
possible(word)  ⇔  feedback(guess, word) == actualFeedback
```

One check per candidate — and duplicate-letter cases are handled correctly for
free, because the feedback function already encodes the count logic.

### 3. Choosing the best guess — information theory

The solver maximizes **expected information gain (entropy)**. For each guess it
looks at how the candidate list would split into the 243 feedback groups:

```
H(guess) = −Σ over outcomes  p · log2(p)       p = groupSize / N
```

The guess whose outcomes are as even as possible wins — whatever feedback you
get leaves the fewest remaining candidates. The candidate pool is the answer
list, but the guess pool is the full ~12,947 legal words, because a "probe"
word that tests five fresh letters (like `soare`, `roate`, `raise`) often beats
any candidate word early in the game.

### 4. Game loop

```
candidates = answers
guess = word with highest entropy over candidates

while candidates.size() > 1:
    feedback = feedback(guess, answer)          # or typed in interactively
    candidates = {w in candidates : feedback(guess, w) == feedback}
    guess = word with highest entropy over new candidates
```

When one candidate remains, that is the answer.

## Project structure

```
wordle/
├── CMakeLists.txt              # Build configuration (library + CLI + tests)
├── README.md
├── LICENSE                     # MIT
├── .gitignore
│
├── include/wordle/
│   ├── word_list.hpp           # Loading answers.txt / guesses.txt
│   ├── feedback.hpp            # Feedback encoding (green/yellow/gray logic)
│   ├── entropy.hpp             # Entropy-based guessing and candidate pruning
│   ├── solver.hpp              # Auto-play: solve one answer or all of them
│   └── game.hpp                # Interactive game + solver assistant
│
├── src/
│   ├── word_list.cpp
│   ├── feedback.cpp
│   ├── entropy.cpp
│   ├── solver.cpp
│   ├── game.cpp
│   └── main.cpp                # CLI entry point (play / solve / analyze)
│
├── data/
│   ├── answers.txt             # 2,309 valid answers (one word per line)
│   └── guesses.txt             # 12,947 legal guesses (superset of answers)
│
├── tests/
│   ├── test_feedback.cpp       # Feedback computation correctness
│   ├── test_solver.cpp         # Solver correctness and full-word-list run
│   └── CMakeLists.txt
│
├── benchmarks/
│   └── benchmark_solver.cpp    # Auto-play every answer, report stats
│
└── scripts/
    └── run_all_words.sh        # Run benchmark against every answer, log stats
```

## Build

Requires CMake ≥ 3.16 and a C++17 compiler.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build          # run unit tests
```

Binaries: `build/wordle` (CLI) and `build/wordle-benchmark` (full benchmark).

## Usage

```
Usage: wordle <command> [args]

Commands:
  play              Play an interactive game of Wordle
  solve             Interactive solver assistant (enter guess + feedback)
  analyze <word>    Outcome breakdown and entropy for a single guess
  analyze all       Top 20 opening guesses by entropy
```

### Play a game

```sh
$ wordle play
```

A full-screen game opens: type a letter, Backspace to delete, Enter to submit,
`q` to quit. The board and a QWERTY keyboard are colored green/yellow/gray as
you play, and the answer is revealed on the last guess. Resize the window to
see the board re-centre; `NO_COLOR=1` switches to a symbol-based monochrome
mode (`[s]` green, `(s)` yellow).

### Interactive solver assistant

Handy to play along with the real Wordle:

```sh
$ wordle solve
```

Type the word you guessed (or press Enter for the best suggestion), then enter
its colors with `1/2/3` (or `g/y/.`). The candidates are narrowed live until
the answer is found.

### Analyze a single guess

```sh
$ wordle analyze soare
Guess:            soare
Possible answers: 2309
Reachable outcomes: 127 / 243

Outcome  eliminated  left   %        I
------------------------------------------
  ggg..      2306        3    0.13%   9.589 bits
  ...
Average information (entropy): 5.8852 bits
```

### Full benchmark

```sh
$ build/wordle-benchmark          # animated run over all 2,309 answers
$ build/wordle-benchmark --plain  # no animation, final stats only
$ scripts/run_all_words.sh        # same, but logs to logs/benchmark-*.log
```

The data directory is found next to the executable, in the current directory,
or via the `WORDLE_DATA_DIR` environment variable.

## Benchmarks

Solver strategy: entropy-maximizing guess over the full 12,947-word guess pool,
with the opening guess `soare`. Run against every possible answer (2,309 words).

```
Guess-count distribution:
  1 guess :   22
  2 guesses:  898
  3 guesses: 1288
  4 guesses:   99
  5 guesses:    2

Average guesses:    2.64
Worst case:         5 guesses (answer: "rarer")
Solved in <= 6:     100.0%
Total time:         00:33        (single-threaded, Release build)
```

| Metric        | Value            |
|---------------|------------------|
| Average guesses | 2.64           |
| Solved in 1–2  | 39.8%           |
| Solved in ≤ 3  | 95.6%           |
| Worst case     | 5 guesses (`rarer`) |
| Failures       | 0 / 2,309        |
| Full run       | ~33 s           |

## Tests

```sh
ctest --test-dir build
```

- `test_feedback` — feedback correctness: exact matches, duplicates in guess
  and answer, code ⇄ pattern round trip over all 243 outcomes.
- `test_solver` — candidate filtering, entropy ranking, a hermetic
  small-list solve, and a full solve over the real 2,309-word list.

## License

MIT — see [LICENSE](LICENSE).