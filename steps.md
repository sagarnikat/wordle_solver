# Building a Wordle Solver — Steps and Logic

Goal: a program that, given the colored feedback of each guess, narrows down the
answer and picks the best next guess. It should solve any game in 6 tries (ideally 3-4).

---

## Step 0 — Understand the feedback model

Each guess gets 5 colors per letter:

| Color  | Meaning                                              |
|--------|------------------------------------------------------|
| Green  | letter is correct **and** in the right position      |
| Yellow | letter is in the word but in the **wrong** position  |
| Gray   | letter is **not** in the word                        |

**Important subtlety — duplicate letters.** Two passes are needed:

1. First pass: mark all green matches. Decrement a running count of each letter
   from the answer.
2. Second pass: for remaining positions, mark yellow **only if** the answer still
   has a spare copy of that letter; otherwise gray.

Example: answer `abcde`, guess `aaccc`:
- `a` at pos 0 → green (spare `a` left in answer? no, answer has one `a`).
- `a` at pos 1 → yellow **if** a spare `a` remains, else gray.
- `c` at pos 3 → yellow (one spare), `c` at pos 4 → gray (no spares left).

The key insight of the whole solver: **the feedback is fully determined by the
pair (guess, answer)**. Same guess + same answer always gives the same colors.

---

## Step 1 — Get the word lists

You already have them in `words/`:

- `possible answers.json` — the ~2300 words that can actually be the answer.
- `possible answers + valid words.json` — all ~13,000 words that are legal guesses
  (answers plus "weird" words people can type).

**Why two lists?** The answer must be a common word, but you are allowed to guess
words like `aesir` or `tares` that could never be the answer — they are still useful
for gathering information. Your solver's candidate pool is the answers list; its
guess pool can be the bigger list.

Write a loader that reads the JSON array into a `vector<string>` and keep only
5-letter words.

---

## Step 2 — Implement feedback (`feedbackFor`)

Input: a guess and a candidate answer. Output: a 5-char string like `"y.g.g"`.

Logic:

```
counts[] = letter counts of answer
for each position i:
    if guess[i] == answer[i]:
        result[i] = green
        counts[guess[i]]--
for each position i where result[i] is unset:
    if counts[guess[i]] > 0:
        result[i] = yellow
        counts[guess[i]]--
    else:
        result[i] = gray
```

This one function is used for three things: scoring guesses, pruning candidates,
and (in auto-play mode) acting as the "secret word".

---

## Step 3 — Prune the candidate list

After each guess you know the feedback, so you can throw away words that are
impossible.

### Option A — manual rules (what `prune.cpp` does)

- **Gray** `c`: any word containing `c` is impossible.
- **Yellow** `c at pos i`: word must contain `c` somewhere **except** position `i`.
- **Green** `c at pos i`: word must have `c` exactly at position `i`.

Caveat: with duplicate letters these simple rules are incomplete. Example:
answer `abbey`, guess `array` → `r` gray, but a repeated letter can be part gray
and part green. Handling counts by hand gets messy.

### Option B — the universal trick (recommended)

A word is still possible **if and only if it produces the exact same feedback as
the actual answer**:

```
possible(word)  ⇔  feedbackFor(guess, word) == actualFeedback
```

One check, one loop over candidates, and duplicate letters are handled correctly
for free, because `feedbackFor` already encodes the count logic. This is the
cleanest pruning you can write.

---

## Step 4 — Choose a guess: frequency heuristic (the simple strategy)

Idea: guess words made of common letters in common positions, so you are most
likely to hit green/yellow and eliminate a lot.

1. Count, for each position 1-5, how often each letter `a-z` appears across the
   **remaining candidates** (`letter_frequency.cpp` already does this analysis).
2. Score a word = sum of `freq[position][letter]` over its 5 letters.
3. Bonus rule: don't double-count repeated letters in a word (a word with two `e`s
   reveals less than two different letters).
4. Pick the highest-scoring word from the candidate list (or from the full valid
   list for a fancier first guess).

Why adapt frequency to the remaining candidates each turn? Because the word
`tares` is a great first guess, but if feedback showed `t` and `r` are impossible,
the best letters to probe change completely.

---

## Step 5 — Choose a guess: information theory (the smart strategy)

The frequency heuristic is greedy — it ignores *what the feedback will tell you*.
Information theory picks the guess that is expected to split the candidate list
the most evenly.

**The entropy idea**: if a guess makes the candidates split into groups, each
group corresponding to a possible feedback pattern, then the best guess is the one
whose groups are as small as possible. The most balanced guess (all groups of size
`N / 243`) is best because any feedback you get leaves few possibilities.

**Expected remaining candidates** (the quantity to minimize):

```
for each possible guess g:
    for each answer a in candidates:
        pattern = feedbackFor(g, a)
        groups[pattern]++

    expectedRemaining(g) = sum over patterns of  (groupSize / N) * groupSize
                         = sum of  groupSize² / N

pick g with the smallest expectedRemaining
```

Explanation: after guessing `g`, the answer will be in one group; the chance of
landing in a group of size `s` is `s/N`, and then `s` candidates remain. So the
expected number of remaining candidates is `Σ (s/N) · s`.

Optionally convert to **entropy (bits)**:
`entropy = Σ (s/N) · log2(N/s)` — maximize bits gained.

**Practical notes:**
- Computing this for all ~13,000 guesses against ~2,300 candidates is ~30 million
  feedback computations — a second or two in C++ with `-O2`. Encode the feedback
  pattern as a small integer (base-3, `3^5 = 243` patterns) instead of a string to
  keep it fast.
- The first guess from information theory is something like `raise` / `crane` —
  the same reason humans start with words full of common letters.
- Mid-game, the best guess is often a word **not** in the candidate list (a "probe"
  word like `aesir`) because it tests five new letters at once. This is why the
  guess pool should be the big valid-words list.

---

## Step 6 — Put it together: the game loop

```
candidates = answers list
tried = empty set

for turn in 1..6:
    guess = bestGuess(pool, candidates)      # Step 4 or Step 5
    if guess in tried: pick next best        # never repeat a guess
    tried.add(guess)

    show guess

    if interactive mode:
        read feedback from the human (g/y/.) 
    else (auto-play mode):
        feedback = feedbackFor(guess, secret)

    if feedback == "ggggg":  WIN
    candidates = prune(candidates, guess, feedback)
    print "candidates left: N"

if loop ends: LOSE (print the answer in auto mode)
```

Rules for guessing:
- Never guess a word you already guessed (wasted turn).
- When only one candidate remains, guess it — it's the answer.
- If the candidate list becomes empty, the feedback was typed wrong.

---

## Step 7 — Test and measure

1. **Interactive test**: run `game.cpp` in one terminal, the solver in another;
   feed the game's colors back to the solver as `g`/`y`/`.`.
2. **Auto-play test**: pick a random answer, let the solver play itself against
   `feedbackFor`, and watch it narrow `2309 → ~100 → ~15 → ...`.
3. **Full benchmark** (best proof): run auto-play against *every* answer in the
   list and record the guess-count distribution:
   - Frequency heuristic: usually solves everything in ≤ 6, average ~4.
   - Information theory: similar average but fewer losses and more 3-guess wins.

---

## Possible improvements (in order of value)

1. **Count-aware feedback in manual pruning** — or just always use the universal
   Option B trick (Step 3).
2. **Encode patterns as integers** — 5-char strings are slow in the hot loop.
3. **Memoize feedback** between guesses — the same (guess, answer) pairs reappear
   across turns.
4. **Hard mode rules** — restrict guesses to candidate words only.
5. **Benchmark output** — print a histogram of guess counts to compare strategies.
6. **Better first guesses** — hardcode a known-good opener from a precomputed
   table instead of recomputing on every start.

---

## One-line summary

**Collect words → simulate feedback → eliminate everything that disagrees with
that feedback → guess the word that (by frequency or by entropy) is expected to
eliminate the most.**
