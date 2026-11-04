# Stack Trace

A learning app for C and C++, written in C++ with
[raylib](https://www.raylib.com/). It teaches the way Noel's slides do: step
through code one line at a time and watch what happens in memory, with stack
frames, heap blocks and an arrow from every pointer to whatever it points at.
Then a quiz checks you've got it, and your mastery of each topic builds up on
the topic map.

Advanced Programming group project 2, SETU, by Ryan Mulcahy and Kobi Bourne.
Built with the help of AI (Claude, by Anthropic), as recorded in our
development journals.

## Building and running

Same setup as the Hospital project: raylib and a C++ compiler. The same
`makefile` works on Mac and Windows.

**Mac** (raylib from Homebrew: `brew install raylib`)

```
make run
```

**Windows** (raylib's w64devkit, which includes g++ and `mingw32-make`)

```
mingw32-make run
```

If raylib isn't in `C:/raylib`: `mingw32-make run RAYLIB_BIN=D:/raylib/w64devkit/bin`

Run it from the project folder, so it can find `content/` and `fonts/`.

In VS Code, Ctrl+Shift+B (Cmd+Shift+B on Mac) builds and runs it. The other
tasks (build, run the tests, clean) are under Terminal > Run Task.

| Command (Mac) | Command (Windows) | What it does |
|---|---|---|
| `make` | `mingw32-make` | build the program |
| `make run` | `mingw32-make run` | build it and run it |
| `make test` | `mingw32-make test` | build and run the automatic tests (no window, no raylib needed) |
| `make clean` | `mingw32-make clean` | delete the program and the test program |

## Using it

| Screen | Keys |
|---|---|
| Topic map | LEFT / RIGHT choose a topic, UP / DOWN change track, ENTER lesson, Q quiz, E exam prep, P profile |
| Lesson | RIGHT or SPACE next line, LEFT back a line, Q quiz, ESC topic map |
| Quiz | 1 to 4 pick an option (or click; click a line for "spot the bug"), ENTER next, ESC topic map |
| Results | R try again, ENTER topic map |
| Exam prep | UP / DOWN choose a problem, LEFT / RIGHT change set, ENTER solve, X exam mode, V versus, ESC topic map |
| A problem | type in the editor, Ctrl+Enter (Cmd+Enter on Mac) or F5 run, Ctrl/Cmd+Shift+Enter or F6 submit, Ctrl/Cmd+V paste, Ctrl/Cmd+Z undo, ESC back to the list |
| Versus | the same keys as a problem; ESC twice leaves the match; on the result, R rematch |

Your progress is saved in `progress.txt` after every answer. Delete it to start again.
Your Exam prep code is saved as you type, in `saves/`.

## How it works

- **Topics, lessons and quizzes are text files** in `content/`, not code. Either
  of us can add a lesson or a question without touching the C++.
- **The memory view** (`memview.cpp`) draws whatever a lesson step lists: stack
  frames (the newest on top), heap blocks, and an arrow for each pointer. A heap
  block that nothing points at is marked as leaked automatically.
- **Mastery** works like Khan Academy: a right answer is +10 points on that
  topic, a wrong one -5, up to 100. 50 is Familiar, 80 Proficient, 100 Mastered.
- **Exam prep works like LeetCode.** Each problem has a description, a code
  editor with starter code, and tests. **Run** builds one test program from
  some helpers, your code and the problem's tests, compiles it with the
  computer's own C++ compiler (the same one that builds the app), runs it and
  shows each test: passed, or expected vs what your code gave. **Submit** does
  the same and, if every test passes and no rule is broken, marks the problem
  solved with your time. Compile errors show your own line numbers, an endless
  loop stops after 5 seconds ("Time limit exceeded"), and a crash shows as a
  runtime error. Rules like "no `[]`" are checked by reading your code (comments
  and strings don't count). **Exam mode** adds a countdown, hides the hints and
  submits by itself when time runs out. Solving a problem for the first time
  also counts towards its topic's mastery on the map.
- **Versus mode** puts two people on the same problem with the same clock.
  One copy of Stack Trace hosts (listens on port 5050), the other joins with the
  host's address, and they swap short lines of text: names, the match settings,
  "I'm running", each run's result ("3 of 4 tests"), "accepted" with the time,
  and at the end, each player's code. Never the code during the match. The
  first to submit an accepted answer wins; if nobody does, whoever passed the
  most tests. `net.cpp` moves the lines (Mac/Linux sockets, or Winsock on
  Windows), `versus.cpp` decides what they mean, and `screen_versus.cpp` draws
  it, reusing the problem screen's editor, runner and results.
- **The code that doesn't draw** (`content.cpp`, `quiz.cpp`, `progress.cpp`,
  `practice.cpp`, `editbuf.cpp`, `versus.cpp`, `net.cpp`) never calls raylib. That keeps it testable without a window, the same split
  as `sim.c` in the Hospital project.

## Writing content

Every file is plain text, one `KEY: value` per line. Lines starting with `#`
are comments. Code goes between a `CODE:` line and an `END` line and is kept
exactly as written. `make test` loads every file listed in `topics.txt`, so run
it after adding content: it tells you which file and what's wrong.

Only plain ASCII characters show up (no curly quotes, long dashes or arrows).

### `content/topics.txt`

One topic per line, in the order they appear on the map:

```
pointers | Pointers & addresses | Pointers & memory | Exam 2 | lessons/pointers.txt | quizzes/pointers.txt
```

id | title | track | exam | lesson file | quiz file. Use `-` for no exam, or a
file that isn't written yet. Topics with the same track share a row on the map.

### A lesson: `content/lessons/*.txt`

```
TITLE: The twin operators: & and *
TOPIC: pointers
TEXT: A paragraph of the tutorial. Add more TEXT lines for more paragraphs.
CODE:
int i = 5;
int *ptr = &i;
END
STEP: 2
EXPLAIN: ptr now holds the address of i.
MEM: main | i | int | 1000 | 5
MEM: main | ptr | int* | 1008 | ->i | changed
```

- `STEP: n` starts a step and highlights line n of the code (0 for no line).
- Each step lists everything in memory at that point, not just what changed.
- `MEM: area | name | type | address | value [| changed]`. The area is the stack
  frame's function name, or `heap`. A value of `->name` draws an arrow to the
  cell called `name`; `?` shows an uninitialised value. `changed` highlights the
  cell in amber.

### A quiz: `content/quizzes/*.txt`

```
TITLE: Pointers & addresses
TOPIC: pointers

QUESTION: Predict the output
PROMPT: What does this print?
CODE:
...
END
OPTION: 5 10
OPTION: 3 10
ANSWER: 2
EXPLAIN: Shown after answering.

QUESTION: Spot the bug
KIND: line
PROMPT: Click the line that uses a dangling pointer.
CODE:
...
END
ANSWER: 7
```

- `QUESTION:` starts a question; its value is the tag shown above it.
- `KIND: line` means "click a line of the code"; `ANSWER` is then a line number.
  Otherwise `ANSWER` is the option number. Both count from 1.
- A question with a missing or out-of-range answer is skipped, and `make test` says so.

### An Exam prep problem: `content/practice/`

`content/practice/problems.txt` lists the sets (the tabs on the Exam prep
screen) and their problem files, in order:

```
SET: exam2 | Coding Exam 2 lab | Exam 2
exam2/01_swap_ints.txt
```

A problem file:

```
TITLE: Sum Array
FUNCTION: sum_array
GROUP: Arrays
DIFFICULTY: Easy
TOPIC: arith
MINUTES: 10
PROTOTYPE: int sum_array(int *arr, int size);
TEXT: Compute the sum of all elements in an integer array using pointers.
RULE: No array indexing (arr[i]): pointer arithmetic only
FORBID: [ | No arr[i]: this lab is pointer arithmetic only.
HINT: Add *(arr + i), or move a pointer from arr to arr + size.
EXAMPLE:
int arr[4] = {1, 2, 3, 4};
END
STARTER:
int sum_array(int *arr, int size)
{
    // your code here
    return 0;
}
END
TESTS:
    int a[] = {1, 2, 3, 4};
    checkInt(sum_array(a, 4), 10, "{1,2,3,4} -> 10");
END
```

- `GROUP` is the heading the problem sits under; `TOPIC` is the topic id on the
  map that solving it counts towards; `MINUTES` is exam mode's time limit.
- `RULE` lines are shown to the student. `FORBID: token | message` is checked:
  a word (`if`) must stand alone, a symbol (`[`, `?`, `<<`) can be anywhere.
- `TESTS` is the body of a function that calls the check helpers:
  `checkInt`, `checkBool`, `checkString`, `checkText`, `checkArray(n, got, expected, ...)`,
  `checkGrid` (4 x 6 grids), and for code that prints, `checkOutput(lambda, expected, ...)`
  (exact) and `checkOutputColumns(lambda, expected, ...)` (same numbers, columns lined up).
- The Lab 1 problems' tests come from the Lab 1 question files. The Exam 2
  problems use the lab sheet's prototypes and examples, with new tests.

## Playing Versus

1. Put both laptops on **one phone's hotspot**. College Wi-Fi stops laptops
   connecting to each other.
2. On one laptop (a Mac is easiest: Windows Firewall is stricter about
   incoming connections), go to Exam prep, press **Versus**, then **Host a match**.
   It shows its address, e.g. `10.170.59.202`.
3. On the other, press **Versus**, type that address in **Join** (the start is
   filled in already, since you share a hotspot) and press **Join match**.
4. The host picks the problem set, difficulty and minutes, and presses **Start**.

If a firewall asks whether Stack Trace may accept connections, allow it (on
Windows, tick Public as well as Private, as a hotspot usually counts as public).

To check the network on its own, before playing: on the host's Mac,
`nc -l 5050`; on the Windows laptop, in PowerShell,
`Test-NetConnection <host address> -Port 5050`. `TcpTestSucceeded : True` means
it'll work.

## Tests

`make test` runs `tests/test_core.cpp`, with no window:

1. loading `topics.txt`, including comments, `-` and bad lines
2. loading lessons: code kept exactly, steps, memory cells, Windows line endings
3. loading quizzes: both kinds of question, and broken questions skipped
4. a quiz run: score, lives, and the last answer's feedback staying on screen
5. progress: points, mastery levels, saving and loading it back
6. every real file in `content/` loads
7. activity for the profile: date maths, answers per day and the streak
8. the code editor's text: typing, auto-indent, backspace, paste and undo
9. Exam prep: loading problems, the rule checker, reading test results, solved
   problems in `progress.txt`, every problem file loading, and one real answer
   compiled and run (right, wrong and not compiling)
10. Versus: the messages, a whole match from start to result, who wins (faster,
    only one solved, most tests, someone left), and the end-of-time rules
11. Versus: a real connection between two sockets on this computer: joining,
    swapping lines (including one bigger than a network packet), noticing when
    the other side leaves, and the errors for a wrong or unreachable address

## Files

| File | What's in it |
|---|---|
| `main.cpp` | window, main loop, moving between screens |
| `app.h` | what the screens share (the `App` struct) |
| `content.cpp/.h` | loading topics, lessons and quizzes (no drawing) |
| `quiz.cpp/.h` | score and lives for a quiz (no drawing) |
| `progress.cpp/.h` | mastery points and the answer log in `progress.txt` (no drawing) |
| `ui.cpp/.h` | shared drawing: text, panels, buttons, code listings, the header |
| `memview.cpp/.h` | the memory view |
| `theme.h` | colours and sizes |
| `screen_map.cpp` | the topic map |
| `screen_lesson.cpp` | a lesson in three panes: learn, code, memory |
| `screen_quiz.cpp` | a quiz and its results |
| `practice.cpp/.h` | Exam prep problems: loading, rule checks, compiling and testing answers (no drawing) |
| `editbuf.cpp/.h` | the code editor's text and edits (no drawing) |
| `editor.cpp/.h` | the code editor: drawing with C/C++ colours, keys and clicks |
| `screen_exam.cpp` | Exam prep: the problem sets and list, NeetCode style |
| `screen_problem.cpp` | one problem, LeetCode style: description, editor, test results |
| `net.cpp/.h` | Versus: the connection between two computers (no raylib: windows.h clashes with it) |
| `versus.cpp/.h` | Versus: the messages and match rules (no drawing) |
| `screen_versus.cpp` | Versus: lobby, countdown, live match and result |
| `screen_profile.cpp` | the profile: stats, badges, pinned topics, activity heatmap |
| `content/` | topics, lessons, quizzes, and Exam prep problems in `content/practice/` |
| `fonts/` | IBM Plex Sans and JetBrains Mono, both under the SIL Open Font License (licences included) |
| `tests/test_core.cpp` | the automatic tests |

## Still to build

- **Exam prep**: 36 problems (the 13 from the Coding Exam 2 Preparation lab and
  23 from Lab 1) can be solved, run, submitted and timed. Still to do: problems
  for Exam 1 once it's confirmed, and a model answer to show after solving.
- **Profile**: stats, pinned topics, the heatmap and recent activity already
  read `progress.txt`. Still to do: choosing a name (it uses the computer's user
  name for now), Edit profile, and the unlock rules for most badges ("No []
  allowed" unlocks when every "no []" Exam prep problem is solved).
- **More content**: lessons and quizzes for the other topics.
- **Linked lists in the memory view.** Arrows from one heap block to another
  already work, but a block holds one value, and a node needs two (`data` and
  `next`, plus `prev` for a doubly linked list). `MEM` needs a way to list a
  block's fields, and `memview.cpp` a way to draw them.

## Who did what

We split the work by feature, each on our own branch, and every change goes into
`main` through a pull request that the other person tests (one on Mac, one on
Windows) and merges.

**Ryan:**

**Kobi:**
