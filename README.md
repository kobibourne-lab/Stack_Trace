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

Your progress is saved in `progress.txt` after every answer. Delete it to start again.

## How it works

- **Topics, lessons and quizzes are text files** in `content/`, not code. Either
  of us can add a lesson or a question without touching the C++.
- **The memory view** (`memview.cpp`) draws whatever a lesson step lists: stack
  frames (the newest on top), heap blocks, and an arrow for each pointer. A heap
  block that nothing points at is marked as leaked automatically.
- **Mastery** works like Khan Academy: a right answer is +10 points on that
  topic, a wrong one -5, up to 100. 50 is Familiar, 80 Proficient, 100 Mastered.
- **The code that doesn't draw** (`content.cpp`, `quiz.cpp`, `progress.cpp`)
  never calls raylib. That keeps it testable without a window, the same split
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

## Tests

`make test` runs `tests/test_core.cpp`, with no window:

1. loading `topics.txt`, including comments, `-` and bad lines
2. loading lessons: code kept exactly, steps, memory cells, Windows line endings
3. loading quizzes: both kinds of question, and broken questions skipped
4. a quiz run: score, lives, and the last answer's feedback staying on screen
5. progress: points, mastery levels, saving and loading it back
6. every real file in `content/` loads

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
| `screen_placeholder.cpp` | Exam prep and Profile, still to be built |
| `content/` | topics, lessons and quizzes |
| `fonts/` | IBM Plex Sans and JetBrains Mono, both under the SIL Open Font License (licences included) |
| `tests/test_core.cpp` | the automatic tests |

## Still to build

- **Exam prep** (NeetCode style): the 10 functions from the Coding Exam 2
  Preparation lab, with difficulty tags, a solved tick and an exam mode.
- **Profile** (GitHub style): streak, badges, pinned topics and a heatmap of
  answers per day, all from `progress.txt`.
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
