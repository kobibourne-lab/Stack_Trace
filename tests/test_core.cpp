// tests/test_core.cpp - automatic tests for the code that doesn't draw.
//
// Run them with:   make test        (mingw32-make test on Windows)
//
// No window is opened, and raylib isn't needed. They check:
//   1. topics.txt loading, including comments, "-" for no file, and bad lines
//   2. lesson loading: code kept exactly, steps, memory cells, Windows line endings
//   3. quiz loading: both kinds of question, and broken questions skipped
//   4. a quiz session: score, lives, and the last answer's feedback staying up
//   5. progress: points, mastery levels, and saving then loading it back
//   6. the real content files in content/ all load
//   7. activity for the profile: date maths, answers per day and the streak
//   8. the code editor's text: typing, auto-indent, backspace, paste, undo
//   9. Exam prep problems: loading, the rule checker, reading test results,
//      solved problems in progress.txt, every real problem file loading,
//      and one real answer compiled and run (skipped if there's no compiler)
//
// Same style as the Hospital project's tests: nothing printed when a check
// passes, a FAIL line when one doesn't, and exit code 1 if anything failed.

#include <cstdio>
#include <string>
#include "../content.h"
#include "../quiz.h"
#include "../progress.h"
#include "../practice.h"
#include "../editbuf.h"

static int checksFailed = 0;
static int testsRun = 0, testsFailed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("  FAIL line %d: %s\n", __LINE__, #cond);       \
            checksFailed++;                                             \
        }                                                               \
    } while (0)

static void run(const char *name, void (*test)())
{
    checksFailed = 0;
    test();
    testsRun++;
    if (checksFailed > 0) testsFailed++;
    std::printf("%s  %s\n", checksFailed == 0 ? "PASS" : "FAIL", name);
}

// ---------------------------------------------------------------------------

static void test_topics()
{
    std::vector<Topic> topics;
    std::string error;
    std::string text =
        "# id | title | track | exam | lesson | quiz\n"
        "pointers | Pointers & addresses | Pointers & memory | Exam 2 | lessons/a.txt | quizzes/a.txt\n"
        "\n"
        "arith | Pointer arithmetic | Pointers & memory | Exam 2 | - | -\r\n"
        "this line has no bars\n"
        "| no id | track\n";
    CHECK(parse_topics(text, topics, error));
    CHECK(topics.size() == 2);
    CHECK(topics[0].id == "pointers");
    CHECK(topics[0].title == "Pointers & addresses");
    CHECK(topics[0].lessonFile == "lessons/a.txt");
    CHECK(topics[1].exam == "Exam 2");
    CHECK(topics[1].lessonFile.empty());   // "-" means none
    CHECK(topics[1].quizFile.empty());

    CHECK(!parse_topics("# only a comment\n", topics, error));
    CHECK(!load_topics("does/not/exist.txt", topics, error));
    CHECK(!error.empty());
}

static void test_lesson()
{
    Lesson l;
    std::string error;
    std::string text =
        "TITLE: Twin operators\r\n"
        "TOPIC: pointers\r\n"
        "TEXT: First paragraph.\r\n"
        "TEXT: Second paragraph.\r\n"
        "CODE:\r\n"
        "int i = 5;\r\n"
        "\r\n"
        "    int *ptr = &i;   # not a comment inside code\r\n"
        "END\r\n"
        "STEP: 1\r\n"
        "EXPLAIN: i is made.\r\n"
        "MEM: main | i | int | 1000 | 5\r\n"
        "STEP: 3\r\n"
        "MEM: main | i | int | 1000 | 5\r\n"
        "MEM: main | ptr | int* | 1004 | ->i | changed\r\n"
        "MEM: too | few\r\n"
        "STEP: 99\r\n";
    CHECK(parse_lesson(text, l, error));
    CHECK(l.title == "Twin operators");
    CHECK(l.text == "First paragraph.\nSecond paragraph.");
    CHECK(l.code.size() == 3);
    CHECK(l.code[1].empty());                         // blank code lines kept
    CHECK(l.code[2] == "    int *ptr = &i;   # not a comment inside code");
    CHECK(l.steps.size() == 3);
    CHECK(l.steps[0].line == 1);
    CHECK(l.steps[0].explain == "i is made.");
    CHECK(l.steps[1].cells.size() == 2);              // the short MEM line is skipped
    CHECK(l.steps[1].cells[1].value == "->i");
    CHECK(l.steps[1].cells[1].changed);
    CHECK(!l.steps[1].cells[0].changed);
    CHECK(l.steps[2].line == 0);                      // past the end: no highlight

    CHECK(!parse_lesson("TITLE: no steps\n", l, error));
    CHECK(!parse_lesson("STEP: 1\n", l, error));       // no title
}

static void test_quiz_loading()
{
    Quiz q;
    std::string error;
    std::string text =
        "TITLE: Pointers quiz\n"
        "TOPIC: pointers\n"
        "QUESTION: Predict the output\n"
        "PROMPT: What is printed?\n"
        "CODE:\n"
        "printf(\"%d\", *p);\n"
        "END\n"
        "OPTION: 3\n"
        "OPTION: 5\n"
        "ANSWER: 2\n"
        "EXPLAIN: p points at i.\n"
        "QUESTION: Spot the bug\n"
        "KIND: line\n"
        "PROMPT: Click the line that leaks.\n"
        "CODE:\n"
        "int *p = new int;\n"
        "p = new int;\n"
        "delete p;\n"
        "END\n"
        "ANSWER: 2\n"
        "QUESTION: Broken - answer out of range\n"
        "PROMPT: ?\n"
        "OPTION: a\n"
        "OPTION: b\n"
        "ANSWER: 7\n";
    CHECK(parse_quiz(text, q, error));
    CHECK(q.title == "Pointers quiz");
    CHECK(q.questions.size() == 2);                   // the broken one is skipped
    CHECK(q.questions[0].kind == QuestionKind::Choice);
    CHECK(q.questions[0].options.size() == 2);
    CHECK(q.questions[0].answer == 2);
    CHECK(q.questions[0].code.size() == 1);
    CHECK(q.questions[1].kind == QuestionKind::Line);
    CHECK(q.questions[1].code.size() == 3);

    CHECK(!parse_quiz("QUESTION: x\nPROMPT: y\n", q, error));
    CHECK(error.find("skipped") != std::string::npos);
}

static Quiz three_question_quiz()
{
    Quiz q;
    for (int i = 0; i < 3; i++) {
        Question qu;
        qu.prompt = "Q";
        qu.options = {"a", "b", "c"};
        qu.answer = 1;
        q.questions.push_back(qu);
    }
    return q;
}

static void test_quiz_session()
{
    QuizSession s;
    quiz_start(s, three_question_quiz());
    CHECK(s.lives == START_LIVES);
    CHECK(quiz_current(s) != nullptr);

    CHECK(quiz_answer(s, 1));           // right
    CHECK(!quiz_answer(s, 2));          // already answered: ignored
    CHECK(s.score == 1);
    quiz_next(s);

    CHECK(!quiz_answer(s, 2));          // wrong
    CHECK(s.lives == START_LIVES - 1);
    CHECK(s.missed.size() == 1 && s.missed[0] == 1);
    quiz_next(s);
    quiz_next(s);                       // not answered yet: doesn't move
    CHECK(s.index == 2);

    CHECK(!quiz_is_last(s));            // not answered yet
    quiz_answer(s, 1);
    CHECK(quiz_is_last(s));             // answered the last question
    quiz_next(s);
    CHECK(quiz_finished(s));
    CHECK(quiz_current(s) == nullptr);
    CHECK(s.score == 2);

    // Running out of lives: the last answer's feedback is still shown
    quiz_start(s, three_question_quiz());
    s.lives = 1;
    quiz_answer(s, 3);
    CHECK(s.lives == 0);
    CHECK(!quiz_finished(s));           // still showing why it was wrong
    CHECK(quiz_current(s) != nullptr);
    CHECK(quiz_is_last(s));             // out of lives, so next means results
    CHECK(!quiz_answer(s, 1));          // can't answer again
    quiz_next(s);
    CHECK(quiz_finished(s));
}

static void test_progress()
{
    Progress p;
    CHECK(topic_mastery(p, "pointers") == Mastery::NotStarted);

    progress_record(p, "pointers", false, "2026-11-03");
    CHECK(topic_points(p, "pointers") == 0);         // never below 0
    CHECK(topic_mastery(p, "pointers") == Mastery::Attempted);

    for (int i = 0; i < 5; i++) progress_record(p, "pointers", true, "2026-11-03");
    CHECK(topic_points(p, "pointers") == 50);
    CHECK(topic_mastery(p, "pointers") == Mastery::Familiar);

    for (int i = 0; i < 10; i++) progress_record(p, "pointers", true, "2026-11-04");
    CHECK(topic_points(p, "pointers") == POINTS_MAX);  // capped
    CHECK(topic_mastery(p, "pointers") == Mastery::Mastered);
    CHECK(p.answers.size() == 16);

    // Save and load it back
    std::string error;
    const char *path = "test_progress.txt";
    CHECK(progress_save(path, p, error));
    Progress back;
    CHECK(progress_load(path, back, error));
    CHECK(topic_points(back, "pointers") == POINTS_MAX);
    CHECK(back.answers.size() == 16);
    CHECK(back.answers[0].date == "2026-11-03");
    CHECK(!back.answers[0].correct);
    CHECK(back.answers[15].correct);
    std::remove(path);

    // A missing file is a fresh start, not an error
    CHECK(progress_load("no_such_progress.txt", back, error));
    CHECK(back.answers.empty());

    CHECK(today_string().size() == 10);
}

// Every file the app ships with should load. Run from the project folder.
static void test_real_content()
{
    std::vector<Topic> topics;
    std::string error;
    CHECK(load_topics("content/topics.txt", topics, error));
    if (topics.empty()) {
        std::printf("  (run the tests from the project folder: %s)\n", error.c_str());
        return;
    }
    for (const Topic &t : topics) {
        if (!t.lessonFile.empty()) {
            Lesson l;
            bool ok = load_lesson("content/" + t.lessonFile, l, error);
            if (!ok) std::printf("  %s\n", error.c_str());
            CHECK(ok);
            CHECK(l.topic == t.id);
        }
        if (!t.quizFile.empty()) {
            Quiz q;
            bool ok = load_quiz("content/" + t.quizFile, q, error);
            if (!ok) std::printf("  %s\n", error.c_str());
            CHECK(ok);
            CHECK(q.topic == t.id);
        }
    }
}

static void test_activity()
{
    // Dates: round trips, month and leap-year edges, bad input
    CHECK(date_to_days("1970-01-01") == 0);
    CHECK(days_to_date(date_to_days("2026-11-03")) == "2026-11-03");
    CHECK(date_add_days("2026-11-01", -1) == "2026-10-31");
    CHECK(date_add_days("2028-02-28", 1) == "2028-02-29");
    CHECK(date_add_days("2026-12-31", 1) == "2027-01-01");
    CHECK(date_to_days("not a date") == -1);
    CHECK(date_weekday("2026-11-03") == 1);   // a Tuesday (0 = Monday)
    CHECK(date_weekday("2026-11-01") == 6);   // a Sunday

    Progress p;
    CHECK(progress_streak(p, "2026-11-03") == 0);

    progress_record(p, "pointers", true, "2026-10-30");
    progress_record(p, "pointers", true, "2026-11-01");
    progress_record(p, "pointers", false, "2026-11-02");
    progress_record(p, "dynmem", true, "2026-11-02");
    CHECK(answers_per_day(p)["2026-11-02"] == 2);
    CHECK(progress_correct_count(p) == 3);
    CHECK(progress_total_points(p) == 15 + 10);   // pointers 10+10-5, dynmem 10

    // Nothing today yet: the streak still counts back from yesterday (1st, 2nd)
    CHECK(progress_streak(p, "2026-11-03") == 2);
    progress_record(p, "dynmem", true, "2026-11-03");
    CHECK(progress_streak(p, "2026-11-03") == 3);
    // A gap of two days breaks it
    CHECK(progress_streak(p, "2026-11-05") == 0);
}

static void test_editor()
{
    EditBuffer b;
    eb_set_text(b, "int f()\n{\n}\n");
    CHECK(b.lines.size() == 3);
    CHECK(eb_text(b) == "int f()\n{\n}\n");

    // Enter after { indents, and with the } right after it, the } goes below
    eb_set_text(b, "{}");
    eb_move_to(b, 0, 1);
    eb_newline(b);
    CHECK(b.lines.size() == 3);
    CHECK(b.lines[1] == "    ");
    CHECK(b.lines[2] == "}");
    CHECK(b.row == 1 && b.col == 4);

    // Typing, then a new line keeps the indent
    for (char c : std::string("x = 1;")) eb_type(b, c);
    CHECK(b.lines[1] == "    x = 1;");
    eb_newline(b);
    CHECK(b.lines[2] == "    ");

    // Backspace in the indent removes a whole level
    eb_backspace(b);
    CHECK(b.lines[2] == "");
    // ...and at the start of a line joins it to the one above
    eb_backspace(b);
    CHECK(b.lines[1] == "    x = 1;");
    CHECK(b.row == 1 && b.col == 10);

    // '}' on a blank indented line moves back one level
    eb_set_text(b, "    ");
    eb_end(b);
    eb_type(b, '}');
    CHECK(b.lines[0] == "}");

    // Tab goes to the next multiple of 4
    eb_set_text(b, "ab");
    eb_end(b);
    eb_tab(b);
    CHECK(b.lines[0] == "ab  ");

    // Paste: several lines, tabs become spaces, Windows line endings dropped
    eb_set_text(b, "<>");
    eb_move_to(b, 0, 1);
    eb_paste(b, "one\r\n\ttwo");
    CHECK(b.lines.size() == 2);
    CHECK(b.lines[0] == "<one");
    CHECK(b.lines[1] == "    two>");
    CHECK(b.row == 1 && b.col == 7);

    // Undo: a word typed in one go is one step; the paste is another
    eb_undo(b);
    CHECK(b.lines.size() == 1 && b.lines[0] == "<>");
    eb_set_text(b, "");
    for (char c : std::string("abc")) eb_type(b, c);
    eb_type(b, ' ');
    eb_undo(b);
    CHECK(b.lines[0] == "abc");
    eb_undo(b);
    CHECK(b.lines[0] == "");

    // Reset code can be undone
    eb_set_text(b, "mine");
    eb_replace_all(b, "starter");
    CHECK(b.lines[0] == "starter");
    eb_undo(b);
    CHECK(b.lines[0] == "mine");

    // Moving: up/down remember the column, clicks are clamped
    eb_set_text(b, "long line\nab\nlong line");
    eb_move_to(b, 0, 7);
    eb_down(b);
    CHECK(b.col == 2);
    eb_down(b);
    CHECK(b.col == 7);
    eb_move_to(b, 99, 99);
    CHECK(b.row == 2 && b.col == 9);
    eb_home(b);
    CHECK(b.col == 0);
}

static void test_practice()
{
    Problem p;
    std::string error;
    std::string text =
        "TITLE: Sum Array\n"
        "FUNCTION: sum_array\n"
        "DIFFICULTY: Easy\n"
        "TEXT: Add them up.\n"
        "FORBID: [ | no brackets\n"
        "FORBID: if | no if\n"
        "HINT: one\n"
        "STARTER:\n"
        "int sum_array(int *a, int n)\n"
        "{\n"
        "\treturn 0;\n"
        "}\n"
        "END\n"
        "TESTS:\n"
        "    int a[] = {1, 2};\n"
        "    checkInt(sum_array(a, 2), 3, \"{1,2} -> 3\");\n"
        "END\n";
    CHECK(parse_problem(text, p, error));
    CHECK(p.title == "Sum Array");
    CHECK(p.starter.size() == 4);
    CHECK(p.starter[2] == "    return 0;");     // tabs become spaces
    CHECK(p.tests.size() == 2);
    CHECK(p.forbids.size() == 2);
    CHECK(forbids_token(p, "["));
    Problem empty;
    CHECK(!parse_problem("TITLE: no code\n", empty, error));

    // Rules: comments and strings don't count, words must stand alone
    CHECK(check_rules(p, "int f(int *a) { return *(a + 1); }").empty());
    CHECK(check_rules(p, "// a[i] in a comment\nint x = 1; /* [ */ const char *s = \"[\";").empty());
    CHECK(check_rules(p, "int f(int *a) { return a[1]; }").size() == 1);
    CHECK(check_rules(p, "int gift = 1;").empty());            // "if" inside a word
    CHECK(check_rules(p, "if (x) y = 1;").size() == 1);
    CHECK(check_rules(p, "if (a[0]) {}").size() == 2);

    // The generated program puts the student's code at line 1 of solution.cpp
    std::string program = build_test_program(p, "int sum_array(int *a, int n) { return 0; }", 5);
    CHECK(program.find("#line 1 \"solution.cpp\"\nint sum_array") != std::string::npos);
    CHECK(program.find("checkInt(sum_array(a, 2), 3") != std::string::npos);

    // Reading results back
    RunResult r;
    parse_test_output("\n@@PASS\tfirst\nhello from printf\n@@FAIL\tsecond\tA\x1f" "AA\tA\n@@SUMMARY\t1\t1\n", r);
    CHECK(r.status == RunStatus::Failed);
    CHECK(r.passed == 1 && r.failed == 1);
    CHECK(r.tests.size() == 2);
    CHECK(r.tests[1].expected == "A\nAA");             // multi-line values come back
    CHECK(r.programOutput == "hello from printf\n");
    parse_test_output("@@PASS\tone\n", r);
    CHECK(r.status == RunStatus::Crashed);             // no summary: it stopped part way
    parse_test_output("@@PASS\tone\n@@TIMEOUT\n", r);
    CHECK(r.status == RunStatus::TimedOut);
    parse_test_output("@@PASS\tone\n@@SUMMARY\t1\t0\n", r);
    CHECK(r.status == RunStatus::Passed);
    CHECK(run_accepted(r));
    r.ruleProblems.push_back("no brackets");
    CHECK(!run_accepted(r));                           // passing isn't enough if a rule is broken

    // Solved problems: the first time counts, then only a faster time is kept
    Progress prog;
    CHECK(progress_mark_solved(prog, "exam2/09_sum_array", "2026-11-03", 300));
    CHECK(!progress_mark_solved(prog, "exam2/09_sum_array", "2026-11-04", 500));
    CHECK(prog.solved["exam2/09_sum_array"].seconds == 300);
    progress_mark_solved(prog, "exam2/09_sum_array", "2026-11-04", 120);
    CHECK(prog.solved["exam2/09_sum_array"].seconds == 120);
    CHECK(prog.solved["exam2/09_sum_array"].date == "2026-11-03");
    const char *path = "test_progress.txt";
    CHECK(progress_save(path, prog, error));
    Progress back;
    CHECK(progress_load(path, back, error));
    CHECK(progress_is_solved(back, "exam2/09_sum_array"));
    CHECK(back.solved["exam2/09_sum_array"].seconds == 120);
    std::remove(path);
}

// Every problem in content/practice loads, and one real answer compiles and passes.
static void test_real_problems()
{
    std::vector<ProblemSet> sets;
    std::string error;
    bool ok = load_problem_sets("content/practice", sets, error);
    if (!error.empty()) std::printf("  %s\n", error.c_str());
    CHECK(ok);
    CHECK(error.empty());
    int count = 0;
    const Problem *leap = nullptr;
    for (const ProblemSet &s : sets)
        for (const Problem &p : s.problems) {
            count++;
            CHECK(!p.prototype.empty());
            if (p.id == "lab1/q01_leap_year") leap = &p;
        }
    CHECK(count == 36);
    if (leap == nullptr) return;

    if (find_compiler().empty()) {
        std::printf("  (no C++ compiler found, so the compile-and-run check was skipped)\n");
        return;
    }
    RunResult right = run_answer(*leap, "bool isLeapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }\n", "practice_build");
    CHECK(run_accepted(right));
    CHECK(right.passed == 10);
    RunResult wrong = run_answer(*leap, "bool isLeapYear(int y) { return y % 4 == 0; }\n", "practice_build");
    CHECK(wrong.status == RunStatus::Failed);
    CHECK(wrong.failed == 3);                           // 1700, 1900 and 2100
    RunResult broken = run_answer(*leap, "bool isLeapYear(int y) { return y % 4 == 0 }\n", "practice_build");
    CHECK(broken.status == RunStatus::CompileError);
    CHECK(broken.compilerOutput.find("solution.cpp:1") != std::string::npos);   // the student's own line number
}

int main()
{
    run("topics.txt loading", test_topics);
    run("lesson loading", test_lesson);
    run("quiz loading", test_quiz_loading);
    run("quiz session: score and lives", test_quiz_session);
    run("progress: points, levels, save and load", test_progress);
    run("the real content files load", test_real_content);
    run("activity: dates, answers per day and streaks", test_activity);
    run("code editor: typing, indent, paste, undo", test_editor);
    run("exam prep: loading, rules, results, solved", test_practice);
    run("exam prep: real problems, and one compiled and run", test_real_problems);

    std::printf("\n%d of %d tests passed\n", testsRun - testsFailed, testsRun);
    return testsFailed == 0 ? 0 : 1;
}
