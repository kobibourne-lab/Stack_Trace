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
//  10. Versus: the messages, a whole match from both sides, and who wins
//  11. Versus: a real connection between two sockets on this computer
//  12. XP, levels and quiz combos
//  13. the campaign: loading, what's cleared and open, bosses in progress.txt,
//      and every boss's problem and topics really existing
//
// Same style as the Hospital project's tests: nothing printed when a check
// passes, a FAIL line when one doesn't, and exit code 1 if anything failed.

#include <cstdio>
#include <set>
#include <string>
#include "../content.h"
#include "../quiz.h"
#include "../progress.h"
#include "../practice.h"
#include "../editbuf.h"
#include "../versus.h"
#include "../net.h"
#include "../campaign.h"
#include "../mos.h"
#include "../wardrobe.h"
#include "../citymap.h"
#include <cmath>
#include "../minigames.h"
#include "../daily.h"
#include "../outputle.h"
#include "../codex.h"
#include "../bugdex.h"
#include "../farm.h"
#include <chrono>
#include <thread>

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
    CHECK(count == 38);   // 13 from the Exam 2 lab, 23 from Lab 1, 2 campaign bosses
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

static RunResult fake_run(RunStatus status, int passed, int failed)
{
    RunResult r;
    r.status = status;
    r.passed = passed;
    r.failed = failed;
    for (int i = 0; i < passed + failed; i++) {
        TestResult t;
        t.passed = i < passed;
        r.tests.push_back(t);
    }
    return r;
}

static void test_versus()
{
    // Messages
    VsMessage msg = vs_parse("HELLO 1 Kobi Bourne");
    CHECK(msg.kind == "HELLO");
    CHECK(msg.args.size() == 3);
    CHECK(msg.rest == "1 Kobi Bourne");
    std::string code = "int f()\n{\n\treturn 1; // a \\ b\n}\n";
    CHECK(vs_escape(code).find('\n') == std::string::npos);    // fits on one line
    CHECK(vs_unescape(vs_escape(code)) == code);
    CHECK(vs_unescape(vs_escape("a\\nb")) == "a\\nb");          // a real backslash-n stays as it was

    // A match, from Ryan's side (host), against Kobi
    VsMatch ryan;
    ryan.host = true;
    ryan.me.name = "Ryan";
    vs_apply(ryan, "HELLO " + std::to_string(VERSUS_VERSION) + " Kobi", 0);
    CHECK(ryan.helloReceived);
    CHECK(ryan.them.name == "Kobi");
    vs_begin(ryan, "exam2", "exam2/09_sum_array", 10, 100.0);
    CHECK(ryan.phase == VsPhase::Countdown);
    CHECK(!vs_check_end(ryan, 101.0));
    CHECK(ryan.phase == VsPhase::Countdown);
    vs_check_end(ryan, 103.5);
    CHECK(ryan.phase == VsPhase::Playing);
    CHECK(vs_remaining(ryan, 103.0 + 60) == 540);

    vs_apply(ryan, "RUNNING", 110);
    CHECK(ryan.them.running);
    vs_apply(ryan, "RAN 0 4 compile", 112);
    CHECK(!ryan.them.running);
    CHECK(ryan.them.compileErrors == 1);
    CHECK(ryan.feed.size() == 1 && ryan.feed[0].text == "Ran: compile error");
    vs_apply(ryan, "RAN 3 4 wrong", 150);
    CHECK(ryan.them.bestPassed == 3);
    CHECK(ryan.feed.back().text == "Ran: 3 / 4 tests");

    // Ryan's own runs: a wrong answer, then an accepted submit
    std::vector<std::string> out = vs_my_run(ryan, fake_run(RunStatus::Failed, 2, 2), false, 160);
    CHECK(out.size() == 1 && out[0] == "RAN 2 4 wrong");
    out = vs_my_run(ryan, fake_run(RunStatus::Passed, 4, 0), false, 170);
    CHECK(out.size() == 1);                        // Run, not Submit: passing isn't finishing
    CHECK(!ryan.me.finished);
    out = vs_my_run(ryan, fake_run(RunStatus::Passed, 4, 0), true, 200);
    CHECK(out.size() == 2 && out[1] == "DONE 97.0");
    CHECK(ryan.me.finished && ryan.me.over);
    CHECK(!vs_check_end(ryan, 201));               // Kobi is still going

    vs_apply(ryan, "DONE 140.5", 245);
    CHECK(vs_check_end(ryan, 245));
    CHECK(ryan.phase == VsPhase::Finished);
    std::string why;
    CHECK(vs_outcome(ryan, why) == 1);             // 97 seconds beats 140.5
    CHECK(ryan.myWins == 1 && ryan.theirWins == 0);
    vs_check_end(ryan, 300);
    CHECK(ryan.myWins == 1);                       // only counted once

    // Passing the tests but breaking a rule isn't a finish
    VsMatch m;
    vs_begin(m, "exam2", "x", 10, 0);
    vs_check_end(m, 4);
    RunResult rule = fake_run(RunStatus::Passed, 4, 0);
    rule.ruleProblems.push_back("no []");
    out = vs_my_run(m, rule, true, 30);
    CHECK(out.size() == 1 && out[0] == "RAN 4 4 rule");
    CHECK(!m.me.finished);
    CHECK(m.me.bestPassed == 0);                   // a rule-breaking run doesn't count

    // Nobody solves it: time's up, most tests wins
    vs_my_run(m, fake_run(RunStatus::Failed, 3, 1), true, 100);
    vs_apply(m, "RAN 1 4 wrong", 120);
    CHECK(!vs_check_end(m, 603));                  // time's up, but nobody has said they're over
    vs_my_time_up(m);
    CHECK(!vs_check_end(m, 605));                  // waiting for Kobi's last run
    vs_apply(m, "OVER", 606);
    CHECK(vs_check_end(m, 606));
    CHECK(vs_outcome(m, why) == 1);                // 3 tests beats 1

    // Kobi never says OVER: it ends anyway after the grace period
    VsMatch g;
    vs_begin(g, "exam2", "x", 1, 0);
    vs_check_end(g, 4);
    vs_my_time_up(g);
    CHECK(!vs_check_end(g, 3 + 60 + 5));
    CHECK(vs_check_end(g, 3 + 60 + END_GRACE_SECONDS + 1));
    CHECK(vs_outcome(g, why) == 0);                // nothing passed on either side: a draw

    // Leaving: the one who stays wins
    VsMatch l;
    l.them.name = "Kobi";
    vs_begin(l, "exam2", "x", 10, 0);
    vs_check_end(l, 4);
    vs_apply(l, "LEFT", 50);
    CHECK(vs_check_end(l, 50));
    CHECK(vs_outcome(l, why) == 1);
    CHECK(why == "Kobi left the match.");

    // The joiner: START opens the problem, SETTINGS only from the host
    VsMatch kobi;
    kobi.host = false;
    vs_apply(kobi, "SETTINGS lab1 Hard 20", 0);
    CHECK(kobi.lobbySet == "lab1" && kobi.lobbyDifficulty == "Hard" && kobi.lobbyMinutes == 20);
    vs_apply(kobi, "START exam2 exam2/01_swap_ints 5", 10);
    CHECK(kobi.startRequested);
    CHECK(kobi.problemId == "exam2/01_swap_ints" && kobi.minutes == 5);
    CHECK(kobi.phase == VsPhase::Countdown);
    vs_apply(kobi, "CODE int x;\\nint y;", 20);
    CHECK(kobi.them.hasCode && kobi.them.code == "int x;\nint y;");

    // Different versions don't play
    VsMatch old;
    vs_apply(old, "HELLO " + std::to_string(VERSUS_VERSION + 1) + " Kobi", 0);
    CHECK(!old.helloReceived);
    CHECK(!old.notice.empty());
    vs_apply(old, "SOMETHING NEW 1 2 3", 0);          // unknown messages are ignored

    CHECK(!vs_default_name().empty());
}

// Two sockets on this computer: one hosts, one joins, and they swap lines.
static void test_network()
{
    CHECK(net_init());
    const int port = 5099;                          // not 5050, so it doesn't clash with a real match
    NetConn host, guest;
    CHECK(net_host(host, port));
    CHECK(host.state == NetState::Listening);
    double t0 = 0;
    CHECK(net_join(guest, "127.0.0.1", port, t0));

    auto pump = [&](int frames) {
        for (int i = 0; i < frames; i++) {
            net_update(host, t0);
            net_update(guest, t0);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };
    for (int i = 0; i < 200 && (host.state != NetState::Connected || guest.state != NetState::Connected); i++) pump(1);
    CHECK(host.state == NetState::Connected);
    CHECK(guest.state == NetState::Connected);
    CHECK(host.peerAddress == "127.0.0.1");

    CHECK(net_send(guest, "HELLO 1 Kobi"));
    CHECK(net_send(guest, "RAN 3 4 wrong"));
    std::string big(20000, 'x');                    // bigger than one network packet
    CHECK(net_send(host, "CODE " + big));
    std::string line;
    std::vector<std::string> atHost;
    std::string atGuest;
    for (int i = 0; i < 200 && (atHost.size() < 2 || atGuest.empty()); i++) {
        pump(1);
        while (net_receive(host, line)) atHost.push_back(line);
        if (net_receive(guest, line)) atGuest = line;
    }
    CHECK(atHost.size() == 2);
    CHECK(atHost.size() == 2 && atHost[0] == "HELLO 1 Kobi" && atHost[1] == "RAN 3 4 wrong");
    CHECK(atGuest == "CODE " + big);

    // The guest leaves: the host notices
    net_close(guest);
    for (int i = 0; i < 200 && host.state == NetState::Connected; i++) pump(1);
    CHECK(host.state == NetState::Failed);
    CHECK(host.error == "The other player left.");
    net_close(host);

    // Joining when nobody is hosting fails with a clear message
    NetConn lonely;
    net_join(lonely, "127.0.0.1", port, 0);
    for (int i = 0; i < 200 && lonely.state == NetState::Connecting; i++) { net_update(lonely, 0); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    CHECK(lonely.state == NetState::Failed);
    net_close(lonely);

    // A bad address is caught before trying
    NetConn typo;
    CHECK(!net_join(typo, "10.170.59", port, 0));
    CHECK(typo.state == NetState::Failed);
    CHECK(!net_join(typo, "10.170.59.300", port, 0));

    // Joining somewhere that never answers gives up after the time limit
    NetConn slow;
    net_join(slow, "10.255.255.1", port, 0);
    if (slow.state == NetState::Connecting) {
        net_update(slow, 7.0);                      // pretend 7 seconds have passed
        CHECK(slow.state == NetState::Failed);
        CHECK(slow.error.find("same hotspot") != std::string::npos);
    }
    net_close(slow);

    net_shutdown();
}

static void test_xp_and_combo()
{
    // Levels: 0 XP is level 1, and each level needs 50 more than the last
    CHECK(level_for_xp(0) == 1);
    CHECK(level_for_xp(49) == 1);
    CHECK(level_for_xp(50) == 2);
    CHECK(level_for_xp(149) == 2);
    CHECK(level_for_xp(150) == 3);
    CHECK(xp_for_level(5) == 500);
    CHECK(level_for_xp(xp_for_level(12)) == 12);
    CHECK(level_title(1) == "Hello, World");
    CHECK(level_title(5) == "Pointer Apprentice");
    CHECK(level_title(40) == "Memory Wizard");   // past the end: the top title

    // XP: right +10, wrong +2, and it never goes down
    Progress p;
    CHECK(progress_xp(p) == 0);
    progress_record(p, "pointers", true, "2026-11-06");
    progress_record(p, "pointers", false, "2026-11-06");
    CHECK(progress_xp(p) == XP_CORRECT + XP_WRONG);

    // Combos: right answers in a row, reset by a wrong one
    Quiz quiz;
    for (int i = 0; i < 5; i++) {
        Question q;
        q.options = { "a", "b" };
        q.answer = 1;
        quiz.questions.push_back(q);
    }
    QuizSession s;
    quiz_start(s, quiz);
    quiz_answer(s, 1); quiz_next(s);
    quiz_answer(s, 1); quiz_next(s);
    CHECK(s.combo == 2);
    quiz_answer(s, 2); quiz_next(s);   // wrong
    CHECK(s.combo == 0);
    quiz_answer(s, 1); quiz_next(s);
    CHECK(s.combo == 1);
    CHECK(s.bestCombo == 2);
}

static void test_campaign()
{
    // Parsing: a region needs topics and a boss, and bad regions are left out
    std::vector<Region> regions;
    std::string error;
    CHECK(parse_campaign("# a comment\n"
                         "REGION: one | First | basics | Exam 1\n"
                         "TOPICS: a, b\n"
                         "BOSS: The Bug | set/p1 | offbyone\n"
                         "TAUNT: hi\n"
                         "REGION: two | Second | more | -\n"
                         "TOPICS: c\n"
                         "BOSS: The Other | set/p2\n"
                         "REGION: broken | No boss\n"
                         "TOPICS: d\n", regions, error));
    CHECK(regions.size() == 2);
    CHECK(error.find("broken") != std::string::npos);
    CHECK(regions[0].topics.size() == 2 && regions[0].topics[1] == "b");
    CHECK(regions[0].bossName == "The Bug" && regions[0].bossProblem == "set/p1" && regions[0].bossLook == "offbyone");
    CHECK(regions[0].exam == "Exam 1" && regions[1].exam.empty());
    CHECK(regions[0].taunt == "hi");

    // Clearing topics and opening regions
    Progress p;
    CHECK(region_open(regions, p, 0));
    CHECK(!region_open(regions, p, 1));
    CHECK(current_region(regions, p) == 0);
    CHECK(next_topic(regions[0], p) == 0);
    CHECK(!boss_unlocked(regions, p, 0));                                       // the boss waits for every topic
    for (int i = 0; i < 4; i++) progress_record(p, "a", true, "2026-11-10");   // 40 points: not yet
    CHECK(!topic_cleared(p, "a"));
    progress_record(p, "a", true, "2026-11-10");                                // 50: Familiar
    CHECK(topic_cleared(p, "a"));
    CHECK(next_topic(regions[0], p) == 1);
    CHECK(region_topics_left(regions[0], p) == 1);
    for (int i = 0; i < 5; i++) progress_record(p, "b", true, "2026-11-10");
    CHECK(next_topic(regions[0], p) == -1);                                     // only the boss left
    CHECK(boss_unlocked(regions, p, 0));
    CHECK(!boss_unlocked(regions, p, 1));                                       // a shut region's boss stays locked
    CHECK(!region_open(regions, p, 1));                                         // still shut until it's beaten

    // Beating the boss: saved, opens the next region, keeps the best, gives XP
    int xp = progress_xp(p);
    CHECK(progress_record_boss(p, "one", "2026-11-10", 400, 1));
    CHECK(!progress_record_boss(p, "one", "2026-11-11", 300, 3));               // not the first time
    CHECK(progress_xp(p) == xp + XP_BOSS + 3 * XP_STAR);
    CHECK(region_open(regions, p, 1));
    CHECK(current_region(regions, p) == 1);
    CHECK(boss_unlocked(regions, p, 0));                                        // a beaten boss can be fought again
    CHECK(!boss_unlocked(regions, p, 1));                                       // topic c isn't cleared yet
    CHECK(progress_boss_stars(p, "one") == 3);
    CHECK(p.bosses["one"].seconds == 300);
    CHECK(p.bosses["one"].date == "2026-11-10");
    std::string path = "test_progress.txt";
    CHECK(progress_save(path, p, error));
    Progress back;
    CHECK(progress_load(path, back, error));
    CHECK(progress_boss_beaten(back, "one") && back.bosses["one"].seconds == 300 && back.bosses["one"].stars == 3);
    std::remove(path.c_str());

    // Scoring a fight
    CHECK(boss_hp(5, 2) == 3);
    CHECK(boss_hp(5, 9) == 0);
    CHECK(boss_stars(false, 0, 10, 900) == 0);
    CHECK(boss_stars(true, 2, 800, 900) == 1);
    CHECK(boss_stars(true, 0, 800, 900) == 2);
    CHECK(boss_stars(true, 0, 300, 900) == 3);
    CHECK(boss_stars(true, 1, 300, 900) == 2);

    // Counting a problem's tests (its HP) from its check calls
    Problem prob;
    prob.tests = { "    checkInt(f(1), 2, \"one\");  checkBool(g(), true, \"two\");",
                   "    // a comment that says check is ignored, as is mycheckInt(",
                   "    checkOutput([]() { f(); }, \"x\", \"three\");" };
    CHECK(count_tests(prob) == 3);

    // The real campaign: every topic is in topics.txt, every boss's problem
    // exists, and its counted tests match a real run of its model answer's count
    std::vector<Region> real;
    error.clear();
    CHECK(load_campaign("content/campaign.txt", real, error));
    CHECK(error.empty());
    CHECK(real.size() == 4);
    std::vector<Topic> topics;
    CHECK(load_topics("content/topics.txt", topics, error));
    std::vector<ProblemSet> sets;
    CHECK(load_problem_sets("content/practice", sets, error));
    // The boss-only set is loaded (so the campaign can find it) but put last and
    // not shown in Exam prep or Versus
    CHECK(shown_set_count(sets) == (int)sets.size() - 1);
    CHECK(sets.back().id == "boss" && sets.back().campaignOnly);
    for (int i = 0; i < shown_set_count(sets); i++) CHECK(!sets[i].campaignOnly);
    int topicsUsed = 0;
    for (const Region &r : real) {
        CHECK(!r.taunt.empty() && !r.story.empty() && !r.win.empty());
        for (const std::string &t : r.topics) {
            bool found = false;
            for (const Topic &tp : topics) if (tp.id == t) found = true;
            if (!found) std::printf("  %s: topic %s isn't in topics.txt\n", r.id.c_str(), t.c_str());
            CHECK(found);
            topicsUsed++;
        }
        const Problem *boss = nullptr;
        for (const ProblemSet &s : sets) for (const Problem &pr : s.problems) if (pr.id == r.bossProblem) boss = &pr;
        if (!boss) std::printf("  %s: boss problem %s isn't in content/practice\n", r.id.c_str(), r.bossProblem.c_str());
        CHECK(boss != nullptr);
        if (boss) CHECK(count_tests(*boss) >= 3);
    }
    CHECK(topicsUsed == (int)topics.size());   // every topic is somewhere in the campaign
}

static void test_quiz_mastery_from_lives()
{
    // Perfect is 100, each life lost takes 20 off, out of lives is nothing
    CHECK(quiz_mastery_points(3, 3) == 100);
    CHECK(quiz_mastery_points(2, 3) == 80);
    CHECK(quiz_mastery_points(1, 3) == 60);
    CHECK(quiz_mastery_points(0, 3) == 0);

    Progress p;
    CHECK(!progress_finish_quiz(p, "basics", 0, 3));   // out of lives: no change
    CHECK(topic_points(p, "basics") == 0);
    CHECK(progress_finish_quiz(p, "basics", 1, 3));    // a pass clears it in the campaign
    CHECK(topic_points(p, "basics") == 60);
    CHECK(topic_cleared(p, "basics"));
    progress_finish_quiz(p, "basics", 2, 3);
    CHECK(topic_points(p, "basics") == 80);
    progress_finish_quiz(p, "basics", 1, 3);           // a worse run never lowers it
    CHECK(topic_points(p, "basics") == 80);
    progress_finish_quiz(p, "basics", 3, 3);
    CHECK(topic_mastery(p, "basics") == Mastery::Mastered);
}

// The last line MAL-OS printed, for checking what a command said.
static std::string last_line(const Mos &m) { return m.out.empty() ? "" : m.out.back().text; }

static void test_mal_os()
{
    Mos m;
    MosInfo info;
    info.user = "kobi";
    info.level = 2;
    mos_boot(m, info);

    // Programs unlock by level: snake at 2, heaphero at 3
    CHECK(mos_run(m, info, "run snake").action == MosAction::RunGame);
    CHECK(mos_run(m, info, "run heaphero").action == MosAction::None);
    CHECK(last_line(m).find("locked") != std::string::npos);
    CHECK(mos_run(m, info, "./snake").arg == "snake");

    // Files: the diary's later days need bosses, cd and paths work
    mos_run(m, info, "cd diary");
    CHECK(m.cwd == "/home/mal/diary");
    mos_run(m, info, "cat day2.txt");
    CHECK(last_line(m).find("Permission denied") != std::string::npos);
    info.bossesBeaten = 1;
    mos_run(m, info, "cat day2.txt");
    CHECK(last_line(m).find("Permission denied") == std::string::npos);
    mos_run(m, info, "cd ..");
    CHECK(m.cwd == "/home/mal");
    mos_run(m, info, "cd /nowhere");
    CHECK(last_line(m).find("No such") != std::string::npos);

    // Editing and compiling hand over to the screen; ./a.out needs a compile first
    CHECK(mos_run(m, info, "nano hello.c").action == MosAction::Edit);
    CHECK(mos_run(m, info, "gcc hello.c").action == MosAction::Compile);
    CHECK(mos_run(m, info, "./a.out").action == MosAction::None);
    m.compiled = true;
    CHECK(mos_run(m, info, "./a.out").action == MosAction::RunProgram);

    // sudo unlock-all opens everything; unknown commands say so
    mos_run(m, info, "sudo unlock-all");
    CHECK(mos_run(m, info, "run stacktower").action == MosAction::RunGame);
    mos_run(m, info, "flibble");
    CHECK(last_line(m).find("command not found") != std::string::npos);
    CHECK(mos_complete(m, info, "run sta") == "run stacktower ");
    CHECK(mos_complete(m, info, "cat dia") == "cat diary/");
    CHECK(mos_run(m, info, "exit").action == MosAction::Exit);

    // The dungeon: get the key, free the block, return to main to win
    mos_run(m, info, "run dungeon");
    CHECK(m.inDungeon);
    mos_run(m, info, "follow next");
    mos_run(m, info, "take key");
    mos_run(m, info, "follow next");
    mos_run(m, info, "follow other");
    mos_run(m, info, "free block");
    CHECK(m.dungeon.freed);
    mos_run(m, info, "return");
    mos_run(m, info, "return");
    mos_run(m, info, "return");
    CHECK(m.dungeon.room == "main");
    mos_run(m, info, "return");
    CHECK(m.dungeon.won);
    CHECK(mos_run(m, info, "ok").arg == "dungeon-won");
    CHECK(!m.inDungeon);
}

static void test_wardrobe_snake_scores()
{
    UnlockInfo u;
    u.level = 3;
    u.topicsTotal = 20;
    CHECK(wardrobe_earned(*wardrobe_find("drinkhat"), u));       // level 2: earned...
    CHECK(!wardrobe_unlocked(*wardrobe_find("drinkhat"), u));    // ...but it costs bytes to wear
    CHECK(wardrobe_find("drinkhat")->price == 20 && wardrobe_find("wizard")->price == 120);
    u.owned.insert("drinkhat");
    CHECK(wardrobe_unlocked(*wardrobe_find("drinkhat"), u));
    CHECK(!wardrobe_earned(*wardrobe_find("hardhat"), u));       // level 5
    CHECK(!wardrobe_unlocked(*wardrobe_find("bowtie"), u));      // 1 boss
    u.bossesBeaten = 1;
    CHECK(wardrobe_unlocked(*wardrobe_find("bowtie"), u));       // boss rewards are free
    CHECK(wardrobe_find("bowtie")->price == 0 && wardrobe_find("slayer")->price == 0 && wardrobe_find("diamond")->price == 0);
    CHECK(!wardrobe_unlocked(*wardrobe_find("crown"), u));       // every topic mastered
    u.topicsMastered = 20;
    CHECK(wardrobe_unlocked(*wardrobe_find("crown"), u));
    CHECK(wardrobe_requirement(*wardrobe_find("wizard")) == "Reach level 12");

    // Boss skins follow the bosses; the glitch skin needs a crash; pets are their own kind
    CHECK(wardrobe_unlocked(*wardrobe_find("offbyone"), u));     // 1 boss beaten
    CHECK(!wardrobe_unlocked(*wardrobe_find("dangling"), u));    // needs 2
    CHECK(!wardrobe_unlocked(*wardrobe_find("glitch"), u));
    u.crashes = 1;
    CHECK(wardrobe_unlocked(*wardrobe_find("glitch"), u));
    CHECK(wardrobe_requirement(*wardrobe_find("glitch")) == "Crash a MAL-OS game");
    CHECK(wardrobe_find("bird") && wardrobe_find("bird")->kind == WearKind::Pet);
    CHECK(wardrobe_find("afro") && wardrobe_find("mohawk"));
    CHECK(wardrobe_rename("tophat") == "helicopter" && wardrobe_rename("gold") == "diamond" && wardrobe_rename("afro") == "afro");
    CHECK(!wardrobe_unlocked(*wardrobe_find("diamond"), UnlockInfo()));

    // Snake: eating grows the list, hitting the wall is a segfault
    SnakeGame g;
    snake_start(g, 7);
    CHECK(g.body.size() == 3);
    g.food = { g.body.front().x + 1, g.body.front().y };
    CHECK(snake_step(g));
    CHECK(g.body.size() == 4);
    snake_turn(g, -1, 0);                  // straight back into the neck: ignored
    snake_step(g);
    CHECK(g.alive && g.dir.x == 1);
    for (int i = 0; i < 40 && g.alive; i++) snake_step(g);
    CHECK(!g.alive);

    // Scores keep the best; the outfit survives a save
    Progress p;
    CHECK(progress_record_score(p, "snake", 50));
    CHECK(!progress_record_score(p, "snake", 20));
    CHECK(progress_best_score(p, "snake") == 50);
    p.hat = "crown";
    p.pet = "cat";
    std::string error;
    CHECK(progress_save("test_progress.txt", p, error));
    Progress back;
    CHECK(progress_load("test_progress.txt", back, error));
    CHECK(back.hat == "crown" && back.skin == "classic" && back.pet == "cat" && back.scores["snake"] == 50);
}

static float city_dist(CityPoint a, CityPoint b) { return sqrtf((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)); }

static void test_memory_city()
{
    // Islands never overlap, however many regions there are
    for (int count = 1; count <= 6; count++)
        for (int a = 0; a < count; a++)
            for (int b = a + 1; b < count; b++)
                CHECK(city_dist(city_island_at(a, count), city_island_at(b, count)) >= CITY_ISLAND_SIZE);

    // Four regions like the real campaign: 5, 6, 3 and 6 topics, each with a boss
    const int sizes[4] = { 5, 6, 3, 6 };
    std::vector<CityNode> nodes;
    for (int r = 0; r < 4; r++) {
        for (int t = 0; t < sizes[r]; t++) nodes.push_back({ r, t, 2 });
        nodes.push_back({ r, -1, 2 });
    }
    std::vector<int> first(4, -1), last(4, -1);
    for (int i = 0; i < (int)nodes.size(); i++) {
        int r = nodes[i].region;
        if (first[r] < 0) first[r] = i;
        last[r] = i;
        CityPoint p = city_node_at(nodes, i, 4), home = city_island_at(r, 4);
        CHECK(city_dist(p, home) < CITY_ISLAND_SIZE / 2 - 0.6f);                 // on its island, not hanging off the edge
        for (int o = 0; o < 4; o++)
            if (o != r) CHECK(city_dist(p, city_island_at(o, 4)) > city_dist(p, home));   // nearer its own island than any other
        for (int j = 0; j < i; j++)
            if (nodes[j].region == r) CHECK(city_dist(p, city_node_at(nodes, j, 4)) > 1.2f);   // stops don't overlap
    }
    // Each island's path starts on the side facing the island before, and ends
    // (at the boss) on the side facing the next one, so the bridges are short
    for (int r = 1; r < 4; r++) {
        CityPoint prev = city_island_at(r - 1, 4);
        CHECK(city_dist(city_node_at(nodes, first[r], 4), prev) < city_dist(city_node_at(nodes, last[r], 4), prev));
    }
    for (int r = 0; r < 3; r++) {
        CityPoint next = city_island_at(r + 1, 4);
        CHECK(city_dist(city_node_at(nodes, last[r], 4), next) < city_dist(city_node_at(nodes, first[r], 4), next));
    }

    // The labels: what each island says as you play
    std::vector<Region> regions;
    std::string error;
    CHECK(parse_campaign("REGION: one | First | basics | -\nTOPICS: a, b\nBOSS: The Bug | set/p1\n"
                         "REGION: two | Second | more | -\nTOPICS: c\nBOSS: The Other | set/p2\n", regions, error));
    Progress p;
    std::vector<CityRegion> c = city_regions_for(regions, p);
    CHECK(c.size() == 2 && c[0].name == "First");
    CHECK(c[0].status == "0 / 2 topics" && c[0].open && c[0].playing);
    CHECK(c[1].status == "LOCKED" && !c[1].open && !c[1].playing);
    for (int i = 0; i < 5; i++) { progress_record(p, "a", true, "2026-11-11"); progress_record(p, "b", true, "2026-11-11"); }
    CHECK(city_regions_for(regions, p)[0].status == "BOSS READY");
    p.bosses["one"] = BossRecord{ "2026-11-11", 300, 2 };
    c = city_regions_for(regions, p);
    CHECK(c[0].status == "CLEARED" && c[0].beaten && !c[0].playing);
    CHECK(c[1].open && c[1].playing && c[1].status == "0 / 1 topics");
}

// Spectators: the pool and payouts, following a whole match from what the
// host passes on, and a real listening socket taking two spectators at once
static void test_spectators()
{
    // The pool, and what predictions pay
    std::vector<VsWatcher> room = { { "Aoife", 0, 40, {} }, { "Cian", 1, 60, {} }, { "Dara", -1, 0, {} } };
    int pool[2];
    vs_pool(room, pool);
    CHECK(pool[0] == 40 && pool[1] == 60);
    CHECK(vs_payout(40, 0, 0, pool) == 100);          // the less popular pick takes the whole pool
    CHECK(vs_payout(60, 1, 0, pool) == 0);
    CHECK(vs_payout(60, 1, 1, pool) == 100);
    CHECK(vs_payout(40, 0, -1, pool) == 40);          // a draw: bytes back
    int lopsided[2] = { 0, 50 };
    CHECK(vs_payout(50, 1, 0, lopsided) == 50);       // nobody picked the winner: bytes back
    CHECK(vs_payout(0, 0, 0, pool) == 0);
    CHECK(vs_payout(10, -1, 0, pool) == 0);           // no prediction, nothing back

    // The ROOM line both ways, and safe names
    std::vector<VsWatcher> back = vs_parse_room(vs_parse(vs_room_line(room)).rest);
    CHECK(back.size() == 3 && back[1].name == "Cian" && back[1].side == 1 && back[1].bytes == 60 && back[2].side == -1);
    CHECK(vs_clean_name("Ao ife!:x") == "Aoifex");
    CHECK(vs_clean_name("::") == "Watcher");

    // A whole match as a spectator sees it
    VsSpectate s;
    double t = 100;
    vs_spectate_apply(s, "P 0 HELLO 2 Ryan", t);
    vs_spectate_apply(s, "P 0 STATS 7 21", t);
    vs_spectate_apply(s, "P 1 HELLO 2 Kobi", t);
    vs_spectate_apply(s, "P 0 SETTINGS exam2 Easy 10", t);
    vs_spectate_apply(s, "ROOM Aoife:-1:0 Cian:1:60", t);
    CHECK(s.side[0].them.name == "Ryan" && s.side[1].them.name == "Kobi");
    CHECK(s.level[0] == 7 && s.solved[0] == 21);
    CHECK(s.side[0].lobbySet == "exam2" && s.side[0].lobbyMinutes == 10);
    CHECK(s.room.size() == 2 && s.open && s.phase == VsPhase::Lobby);
    vs_spectate_apply(s, "CLOSED", t);
    vs_spectate_apply(s, "P 0 START exam2 exam2/03_find_max 10", t);
    CHECK(!s.open && s.phase == VsPhase::Countdown);
    CHECK(s.side[0].problemId == "exam2/03_find_max" && s.side[1].minutes == 10);
    vs_spectate_tick(s, t + 3.5);
    CHECK(s.phase == VsPhase::Playing);
    vs_spectate_apply(s, "P 1 RUNNING", t + 20);
    CHECK(s.side[1].them.running);
    vs_spectate_apply(s, "P 1 RAN 2 5 wrong", t + 23);
    vs_spectate_apply(s, "P 1 LIVE int x;\\nint y;", t + 24);
    vs_spectate_apply(s, "P 0 RAN 0 5 compile", t + 30);
    CHECK(s.side[1].them.passed == 2 && s.side[1].them.bestPassed == 2 && !s.side[1].them.running);
    CHECK(s.side[0].them.compileErrors == 1);
    CHECK(s.live[1] == "int x;\nint y;");
    std::vector<VsNamedEvent> feed = vs_spectate_feed(s);
    CHECK(feed.size() == 2 && feed[0].side == 0 && feed[1].side == 1);   // newest first
    CHECK(feed[1].text == "Kobi ran: 2 / 5 tests");
    vs_spectate_apply(s, "P 0 DONE 95.0", t + 98);
    vs_spectate_apply(s, "P 0 CODE int f() {}\\n", t + 98);
    CHECK(s.side[0].them.finished && s.side[0].them.hasCode);
    vs_spectate_apply(s, "RESULT 0", t + 99);
    vs_spectate_apply(s, "SCORE 1 0 0", t + 99);
    CHECK(s.phase == VsPhase::Finished && s.winner == 0 && s.score[0] == 1);
    vs_spectate_apply(s, "P 1 REMATCH", t + 120);
    CHECK(s.phase == VsPhase::Lobby && s.winner == -2);
    vs_spectate_apply(s, "OPEN", t + 120);
    CHECK(s.open);
    // Arriving mid-match: the clock is set from how far in it is
    VsSpectate late;
    vs_spectate_apply(late, "P 0 START exam2 exam2/03_find_max 10", 500);
    vs_spectate_apply(late, "CLOCK 60.00", 500);
    CHECK(late.phase == VsPhase::Playing);
    CHECK(vs_elapsed(late.side[0], 500) > 59.9 && vs_elapsed(late.side[0], 500) < 60.1);

    // A real listening socket taking two spectators at once. Windows needs the
    // network started again: the earlier Versus test shut it down when it finished.
    CHECK(net_init());
    NetServer server;
    const int port = 5097;
    CHECK(net_server_open(server, port));
    NetConn a, b, inA, inB;
    CHECK(net_join(a, "127.0.0.1", port, 0));
    CHECK(net_join(b, "127.0.0.1", port, 0));
    int accepted = 0;
    for (int i = 0; i < 200 && accepted < 2; i++) {
        net_update(a, 0);
        net_update(b, 0);
        if (accepted == 0 && net_server_accept(server, inA)) accepted++;
        if (accepted == 1 && net_server_accept(server, inB)) accepted++;
    }
    CHECK(accepted == 2);
    CHECK(inA.state == NetState::Connected && inB.state == NetState::Connected);
    net_send(a, "WATCH 2 Aoife");
    net_send(inA, "ROOM Aoife:-1:0");
    net_send(inB, "ROOM Aoife:-1:0");
    std::string got, gotA, gotB;
    for (int i = 0; i < 200 && (got.empty() || gotA.empty() || gotB.empty()); i++) {
        net_update(a, 0); net_update(b, 0); net_update(inA, 0); net_update(inB, 0);
        if (got.empty()) net_receive(inA, got);
        if (gotA.empty()) net_receive(a, gotA);
        if (gotB.empty()) net_receive(b, gotB);
    }
    CHECK(got == "WATCH 2 Aoife");
    CHECK(gotA == "ROOM Aoife:-1:0" && gotB == "ROOM Aoife:-1:0");
    NetServer second;
    CHECK(!net_server_open(second, port));             // the port is taken while hosting
    net_close(a); net_close(b); net_close(inA); net_close(inB);
    net_server_close(server);
    CHECK(net_server_open(second, port));             // and free again after
    net_server_close(second);
    net_shutdown();

    // Bytes are saved
    Progress p;
    CHECK(p.bytes == START_BYTES);
    p.bytes = 160;
    std::string error, path = "test_progress_bytes.txt";
    CHECK(progress_save(path, p, error));
    Progress loaded;
    CHECK(progress_load(path, loaded, error));
    CHECK(loaded.bytes == 160);
    std::remove(path.c_str());
}

static void test_versus_looks()
{
    // The LOOK line both ways
    VsLook look;
    look.hat = "crown"; look.skin = "diamond"; look.pet = "dog";
    CHECK(vs_look_line(look) == "LOOK crown diamond dog");
    VsLook back = vs_parse_look(vs_parse(vs_look_line(look)).rest);
    CHECK(back.hat == "crown" && back.skin == "diamond" && back.pet == "dog");
    // Junk is cleaned, and anything missing is the default
    back = vs_parse_look("Cr;OWN! <script>");
    CHECK(back.hat == "crown" && back.skin == "script" && back.pet == "nopet");
    back = vs_parse_look("");
    CHECK(back.hat == "none" && back.skin == "classic" && back.pet == "nopet");
    CHECK(vs_look_line(VsLook()) == "LOOK none classic nopet");

    // A player's LOOK dresses their Mal on the other screen
    VsMatch m;
    vs_apply(m, "LOOK afro tux cat", 0);
    CHECK(m.them.look.hat == "afro" && m.them.look.skin == "tux" && m.them.look.pet == "cat");

    // Spectators get both players' looks through the host
    VsSpectate s;
    vs_spectate_apply(s, "P 0 LOOK mohawk glitch bird", 0);
    vs_spectate_apply(s, "P 1 LOOK crown slayer nopet", 0);
    CHECK(s.side[0].them.look.hat == "mohawk" && s.side[0].them.look.pet == "bird");
    CHECK(s.side[1].them.look.skin == "slayer");
    // ...and still have them once the match starts (starting resets the scores, not the outfits)
    vs_apply(m, "RAN 3 5 wrong", 0);
    vs_begin(m, "lab1", "q15_reverse_array", 10, 0);
    CHECK(m.them.look.hat == "afro" && m.them.runs == 0);
    vs_spectate_apply(s, "P 0 START lab1 q15_reverse_array 10", 0);
    CHECK(s.side[0].them.look.hat == "mohawk" && s.side[1].them.look.skin == "slayer");

    // The crowd: ROOM carries each spectator's look, and still reads the old short form
    std::vector<VsWatcher> room = { { "Aoife", 0, 40, {} }, { "Cian", -1, 0, {} } };
    room[0].look.hat = "sombrero";
    room[0].look.pet = "bird";
    std::string line = vs_room_line(room);
    CHECK(line == "ROOM Aoife:0:40:sombrero:classic:bird Cian:-1:0:none:classic:nopet");
    std::vector<VsWatcher> got = vs_parse_room(vs_parse(line).rest);
    CHECK(got.size() == 2 && got[0].name == "Aoife" && got[0].side == 0 && got[0].bytes == 40);
    CHECK(got[0].look.hat == "sombrero" && got[0].look.pet == "bird" && got[1].look.hat == "none");
    got = vs_parse_room("Aoife:1:25 Dara:-1:0");
    CHECK(got.size() == 2 && got[0].side == 1 && got[0].bytes == 25 && got[0].look.skin == "classic");
}

static void test_wardrobe_shop()
{
    Progress p;   // a new student: 100 bytes
    const WardrobeItem *duck = wardrobe_find("duck"), *neon = wardrobe_find("neon"), *crown = wardrobe_find("crown");
    CHECK(duck && neon && crown && duck->price == 60 && duck->kind == WearKind::Pet);
    UnlockInfo u;
    u.level = 20;   // a high level doesn't buy anything
    CHECK(!wardrobe_unlocked(*duck, u));
    CHECK(wardrobe_requirement(*duck) == "60 bytes");

    std::string why;
    CHECK(!wardrobe_buy(p, *crown, u, why) && why == "That's not in the shop.");
    u.level = 3;   // a level item: has to be reached before it can be bought
    CHECK(!wardrobe_buy(p, *wardrobe_find("hardhat"), u, why) && why == "Reach level 5 first." && p.bytes == 100);
    u.level = 20;
    CHECK(!wardrobe_buy(p, *neon, u, why) && why == "You need 20 more bytes." && p.bytes == 100);
    CHECK(wardrobe_buy(p, *duck, u, why) && p.bytes == 40 && p.owned.count("duck"));
    CHECK(!wardrobe_buy(p, *duck, u, why) && why == "You already have it." && p.bytes == 40);
    CHECK(wardrobe_buy(p, *wardrobe_find("drinkhat"), u, why) && p.bytes == 20);   // a level item, once reached
    u.owned = p.owned;
    CHECK(wardrobe_unlocked(*duck, u) && !wardrobe_unlocked(*neon, u));

    // Bought things are kept in progress.txt
    std::string path = "test_shop_progress.txt", error;
    CHECK(progress_save(path, p, error));
    Progress back;
    CHECK(progress_load(path, back, error));
    CHECK(back.bytes == 20 && back.owned.size() == 2 && back.owned.count("duck") && back.owned.count("drinkhat"));
    std::remove(path.c_str());
}

// The Duolingo loop: streak days, const freezes, daily quests and chests.
static void test_daily()
{
    Progress p;
    p.bytes = 100;

    // The streak counts any day something was learned, not just answers
    CHECK(streak_mark_active(p, "2026-11-10"));
    CHECK(!streak_mark_active(p, "2026-11-10"));      // only new the first time that day
    CHECK(streak_mark_active(p, "2026-11-11"));
    CHECK(progress_streak(p, "2026-11-11") == 2);
    CHECK(progress_streak(p, "2026-11-12") == 2);     // nothing yet today: still alive
    CHECK(progress_streak(p, "2026-11-13") == 0);     // missed a whole day: gone out

    // Answers from older saves still count as learning days
    progress_record(p, "pointers", true, "2026-11-09");
    CHECK(progress_day_active(p, "2026-11-09"));
    CHECK(progress_streak(p, "2026-11-11") == 3);

    // Freezes: bought with bytes, at most two held
    std::string why;
    CHECK(streak_buy_freeze(p, why));
    CHECK(p.freezes == 1 && p.bytes == 100 - FREEZE_PRICE);
    p.bytes = 1000;
    CHECK(streak_buy_freeze(p, why));
    CHECK(!streak_buy_freeze(p, why));                // already holding the most
    CHECK(p.freezes == FREEZE_MAX);
    Progress poor;
    poor.bytes = 10;
    CHECK(!streak_buy_freeze(poor, why) && poor.freezes == 0 && poor.bytes == 10);

    // Opening on the 13th: the 12th was missed, one freeze covers it
    CHECK(streak_use_freezes(p, "2026-11-13") == 1);
    CHECK(p.freezes == 1);
    CHECK(p.frozenDays.count("2026-11-12") == 1);
    CHECK(progress_streak(p, "2026-11-13") == 3);     // kept going, but the frozen day doesn't add
    CHECK(streak_use_freezes(p, "2026-11-13") == 0);  // opening again the same day uses nothing
    streak_mark_active(p, "2026-11-13");
    CHECK(progress_streak(p, "2026-11-13") == 4);

    // Missing 3 days with only 1 freeze: it goes out, and the freeze is kept
    CHECK(streak_use_freezes(p, "2026-11-17") == 0);
    CHECK(p.freezes == 1);
    CHECK(progress_streak(p, "2026-11-17") == 0);

    // The week row: learned, frozen, nothing
    std::vector<int> week = streak_week(p, "2026-11-13");
    CHECK(week.size() == 7);
    CHECK(week[6] == 1 && week[5] == 2 && week[4] == 1 && week[0] == 0);

    // Quests: three a day, the same for everyone that day, different days differ
    std::vector<Quest> q = daily_quests("2026-11-13");
    CHECK(q.size() == 3);
    CHECK(daily_quests("2026-11-13")[2].key == q[2].key);
    bool differs = false;
    for (int d = 0; d < 14 && !differs; d++) {
        std::vector<Quest> other = daily_quests(date_add_days("2026-11-13", d + 1));
        for (int i = 0; i < 3; i++) if (other[i].text != q[i].text) differs = true;
    }
    CHECK(differs);
    for (int d = 0; d < 60; d++) {   // never two "correct" quests on one day
        std::vector<Quest> day = daily_quests(date_add_days("2026-01-01", d));
        CHECK(!(day[0].key == "correct" && day[1].key == "correct"));
    }

    // Progress, finishing and opening chests
    const std::string today = "2026-11-20";
    Progress s;
    s.bytes = 0;
    std::vector<Quest> qs = daily_quests(today);
    int bytes = 0;
    std::string item;
    CHECK(!daily_claim(s, today, 0, bytes, item, why));   // not done yet
    for (int i = 0; i < 3; i++) {
        if (qs[i].best) daily_best(s, today, qs[i].key, qs[i].target - 1);
        else daily_add(s, today, qs[i].key, qs[i].target - 1);
        CHECK(!quest_done(s, qs[i], today));
        if (qs[i].best) { daily_best(s, today, qs[i].key, 1); CHECK(!quest_done(s, qs[i], today)); daily_best(s, today, qs[i].key, qs[i].target + 5); }
        else daily_add(s, today, qs[i].key, 1);
        CHECK(quest_done(s, qs[i], today));
        CHECK(quest_progress(s, qs[i], today) == qs[i].target);   // capped
    }
    CHECK(quests_done(s, today) == 3);
    int total = 0;
    for (int i = 0; i < 3; i++) {
        CHECK(daily_claim(s, today, i, bytes, item, why));
        CHECK(bytes == qs[i].reward);
        total += bytes;
        CHECK(!daily_claim(s, today, i, bytes, item, why));   // once only
    }
    CHECK(s.bytes == total);
    GoldenPrize prize = daily_golden(s, today);
    CHECK(daily_claim(s, today, GOLDEN_CHEST, bytes, item, why));
    CHECK(bytes == GOLDEN_BYTES && item == prize.itemId);
    if (!item.empty()) CHECK(s.owned.count(item) == 1);

    // Some day in a month gives an item, and only ones not owned yet
    bool anyItem = false;
    for (int d = 0; d < 30; d++) {
        GoldenPrize g = daily_golden(s, date_add_days(today, d));
        if (!g.itemId.empty()) { anyItem = true; CHECK(s.owned.count(g.itemId) == 0); }
    }
    CHECK(anyItem);

    // A new day starts the counters again
    daily_add(s, "2026-11-21", "game");
    CHECK(s.daily.date == "2026-11-21" && s.daily.claimed == 0 && s.daily.counts["game"] == 1);
    CHECK(quests_done(s, "2026-11-21") <= 1);

    // It all saves and loads back
    const char *path = "test_progress.txt";
    p.daily = s.daily;
    p.daily.claimed = 5;
    std::string error;
    CHECK(progress_save(path, p, error));
    Progress back;
    CHECK(progress_load(path, back, error));
    CHECK(back.activeDays == p.activeDays);
    CHECK(back.frozenDays == p.frozenDays);
    CHECK(back.freezes == p.freezes);
    CHECK(back.daily.date == "2026-11-21" && back.daily.claimed == 5 && back.daily.counts["game"] == 1);
    CHECK(progress_streak(back, "2026-11-13") == 4);
}

// Outputle: the daily puzzle, Wordle colours, guesses, streak and bytes.
static void test_outputle()
{
    const std::vector<OutPuzzle> &ps = outputle_puzzles();
    CHECK(ps.size() >= 30);
    for (const OutPuzzle &q : ps) {
        CHECK(!q.code.empty() && !q.answer.empty() && !q.why.empty());
        CHECK(q.answer.find(' ') == std::string::npos && q.answer.find('|') == std::string::npos);
    }
    // Every puzzle comes round once in a cycle, and the same date gives the same one
    std::set<int> seen;
    for (int d = 0; d < (int)ps.size(); d++) seen.insert(outputle_index(date_add_days(OUTPUTLE_START, d)));
    CHECK(seen.size() == ps.size());
    CHECK(outputle_number(OUTPUTLE_START) == 1 && outputle_number("2026-11-20") == 8);

    // Wordle colours, including repeated characters
    std::vector<int> g = outputle_grade("5,25", "7,49");
    CHECK(g[0] == 0 && g[1] == 2 && g[2] == 0 && g[3] == 0);
    g = outputle_grade("7,94", "7,49");
    CHECK(g[0] == 2 && g[1] == 2 && g[2] == 1 && g[3] == 1);
    g = outputle_grade("444", "014");
    CHECK(g[0] == 0 && g[1] == 0 && g[2] == 2);     // only one 4 in the answer, and it's green
    g = outputle_grade("441", "014");
    CHECK(g[0] == 1 && g[1] == 0 && g[2] == 1);

    std::string why;
    CHECK(!outputle_valid("7,4", "7,49", why));
    CHECK(!outputle_valid("7 49", "7,49", why));
    CHECK(outputle_valid("1234", "7,49", why));

    // Playing: wrong, wrong, right on the third try
    const std::string day = "2026-11-20";
    const std::string answer = ps[outputle_index(day)].answer;
    std::string wrong(answer.size(), '#');
    Progress p;
    p.bytes = 0;
    int won = 0;
    CHECK(outputle_guess(p, day, wrong, won, why) && won == 0);
    CHECK(!outputle_guess(p, day, wrong + "x", won, why));   // wrong length: not counted
    CHECK(outputle_guesses(p, day).size() == 1);
    CHECK(outputle_guess(p, day, wrong, won, why));
    CHECK(outputle_guess(p, day, answer, won, why));
    CHECK(outputle_won(p, day) && outputle_over(p, day));
    CHECK(won == outputle_reward(3) && p.bytes == won && won == 25);
    CHECK(!outputle_guess(p, day, answer, won, why));         // over for today
    CHECK(p.outputleStats.played == 1 && p.outputleStats.won == 1 && p.outputleStats.streak == 1 && p.outputleStats.dist[2] == 1);
    std::string share = outputle_share(p, day);
    CHECK(share.find("Outputle #8 3/6") == 0);
    CHECK(share.find(answer) == std::string::npos || answer.size() < 2);   // no spoilers

    // The next day, solved first time: the streak goes up. Then a lost day resets it.
    const std::string next = date_add_days(day, 1);
    CHECK(outputle_guess(p, next, ps[outputle_index(next)].answer, won, why));
    CHECK(won == 35 && p.outputleStats.streak == 2 && p.outputleStats.best == 2);
    const std::string third = date_add_days(day, 2);
    std::string miss(ps[outputle_index(third)].answer.size(), '#');
    for (int i = 0; i < OUTPUTLE_TRIES; i++) CHECK(outputle_guess(p, third, miss, won, why));
    CHECK(outputle_over(p, third) && !outputle_won(p, third) && won == 0);
    CHECK(p.outputleStats.streak == 0 && p.outputleStats.best == 2 && p.outputleStats.played == 3);

    // Saved and loaded back
    std::string error;
    CHECK(progress_save("test_progress.txt", p, error));
    Progress back;
    CHECK(progress_load("test_progress.txt", back, error));
    CHECK(back.outputle.date == third && back.outputle.guesses.size() == 6);
    CHECK(back.outputleStats.won == 2 && back.outputleStats.best == 2 && back.outputleStats.lastWin == next && back.outputleStats.dist[0] == 1);
}

// The C Codex: entries, unlocking with topics, stars and search.
static void test_codex()
{
    const std::vector<CodexEntry> &es = codex_entries();
    CHECK(es.size() >= 30);
    std::vector<Topic> topics;
    std::string error;
    CHECK(load_topics("content/topics.txt", topics, error));
    std::set<std::string> ids;
    for (const CodexEntry &e : es) {
        CHECK(ids.insert(e.id).second);   // no two the same
        bool known = false;
        for (const Topic &t : topics) if (t.id == e.topic) known = true;
        CHECK(known);
        CHECK(!e.example.empty() && !e.output.empty() && e.mistakes.size() == 2);
        CHECK(e.example.find("main") != std::string::npos);
    }
    Progress p;
    CHECK(codex_found(p) == 0);
    progress_record(p, "dynmem", false, "2026-11-13");   // started: the dynmem entries unlock
    const CodexEntry *malloc_e = nullptr;
    for (const CodexEntry &e : es) if (e.id == "malloc") malloc_e = &e;
    CHECK(malloc_e && codex_unlocked(p, *malloc_e) && codex_stars(p, *malloc_e) == 0);
    CHECK(codex_found(p) == 4);   // malloc, calloc, realloc, free
    p.points["dynmem"] = 100;
    CHECK(codex_stars(p, *malloc_e) == 3);
    CHECK(codex_search("").size() == es.size());
    CHECK(codex_search("STRING.H").size() == 4);
    CHECK(!codex_search("mal").empty());
}

// The Bugdex: catching by quick fixes, bosses and again.
static void test_bugdex()
{
    const std::vector<Bug> &bugs = bugdex_bugs();
    CHECK(bugs.size() >= 12);
    for (const Bug &b : bugs) {
        CHECK(b.fixes.size() == 3);
        for (const BugFix &f : b.fixes) CHECK(f.line >= 1 && f.line <= (int)f.code.size() && !f.why.empty());
    }
    // Every campaign boss has its bug
    std::vector<Region> regions;
    std::string error;
    CHECK(load_campaign("content/campaign.txt", regions, error));
    for (const Region &r : regions) CHECK(!bugdex_for_boss(r.bossLook).empty());

    Progress p;
    bool caught = false;
    const Bug *leak = bugdex_find("leak");
    CHECK(leak != nullptr);
    CHECK(!bugdex_answer_fix(p, "leak", 0, leak->fixes[0].line + 1, "2026-11-13", caught) && !caught);
    CHECK(bugdex_answer_fix(p, "leak", 0, leak->fixes[0].line, "2026-11-13", caught) && !caught);
    CHECK(bugdex_answer_fix(p, "leak", 2, leak->fixes[2].line, "2026-11-13", caught) && !caught);
    CHECK(!bug_caught(p, "leak") && bugdex_fix_done(p, "leak", 2) && !bugdex_fix_done(p, "leak", 1));
    CHECK(bugdex_answer_fix(p, "leak", 1, leak->fixes[1].line, "2026-11-14", caught) && caught);
    CHECK(bug_caught(p, "leak") && p.bugs["leak"].how == "practice" && p.bugs["leak"].date == "2026-11-14");
    CHECK(bugdex_answer_fix(p, "leak", 1, leak->fixes[1].line, "2026-11-14", caught) && !caught);   // all done already

    CHECK(bugdex_catch(p, "offbyone", "2026-11-15", "boss"));
    CHECK(!bugdex_catch(p, "offbyone", "2026-11-16", "boss"));   // caught again
    CHECK(p.bugs["offbyone"].caught == 2 && p.bugs["offbyone"].date == "2026-11-15");
    CHECK(!bugdex_catch(p, "nonsense", "2026-11-15", "boss"));
    CHECK(bugs_caught(p) == 2);

    CHECK(progress_save("test_progress.txt", p, error));
    Progress back;
    CHECK(progress_load("test_progress.txt", back, error));
    CHECK(bugs_caught(back) == 2 && back.bugs["offbyone"].caught == 2 && back.bugs["leak"].how == "practice");
    CHECK(bugdex_fix_done(back, "leak", 1));
}

// The Memory Farm: growing, wilting, harvesting and the drone.
static void test_farm()
{
    Progress p;
    p.bytes = 0;
    const std::string today = "2026-11-20";
    CHECK(crop_stage(p, "pointers") == 0 && crop_days_left(p, "pointers", today) == -1 && !crop_wilting(p, "pointers", today));

    progress_record(p, "pointers", true, "2026-11-19");   // attempted: a sprout, fresh for 2 days
    CHECK(crop_stage(p, "pointers") == 1);
    CHECK(crop_days_left(p, "pointers", today) == 1);
    CHECK(crop_wilting(p, "pointers", "2026-11-21"));
    CHECK(crop_harvest_value(p, "pointers", today) == 0);   // too small to harvest

    p.points["pointers"] = 100;   // mastered: in fruit, fresh for 8 days
    CHECK(crop_stage(p, "pointers") == 4);
    CHECK(!crop_wilting(p, "pointers", "2026-11-26"));
    CHECK(crop_wilting(p, "pointers", "2026-11-27"));
    CHECK(crop_harvest(p, "pointers", today) == 8 && p.bytes == 8);
    CHECK(crop_harvest(p, "pointers", today) == 0);                 // once every two days
    CHECK(crop_harvest(p, "pointers", "2026-11-21") == 0);
    CHECK(crop_harvest(p, "pointers", "2026-11-22") == 8 && p.bytes == 16);
    CHECK(crop_harvest(p, "pointers", "2026-11-28") == 0);          // wilting: review it first
    progress_record(p, "pointers", true, "2026-11-28");             // a quiz waters it
    CHECK(!crop_wilting(p, "pointers", "2026-11-28") && crop_harvest(p, "pointers", "2026-11-28") == 8);

    p.points["arith"] = 80;   // proficient, harvested for 4
    progress_record(p, "arith", true, today);
    p.points["arith"] = 80;
    CHECK(crop_stage(p, "arith") == 3 && crop_harvest_value(p, "arith", today) == 4);

    std::string why;
    CHECK(!farm_buy_drone(p, why) && !farm_has_drone(p));
    p.bytes = 200;
    CHECK(farm_buy_drone(p, why) && farm_has_drone(p) && p.bytes == 200 - DRONE_PRICE);
    CHECK(!farm_buy_drone(p, why));

    std::string error;
    CHECK(progress_save("test_progress.txt", p, error));
    Progress back;
    CHECK(progress_load("test_progress.txt", back, error));
    CHECK(back.harvested["pointers"] == "2026-11-28" && farm_has_drone(back));
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
    run("versus: messages, a whole match, who wins", test_versus);
    run("versus: a real connection on this computer", test_network);
    run("XP, levels and quiz combos", test_xp_and_combo);
    run("campaign: regions, clearing, bosses", test_campaign);
    run("quiz mastery from lives left", test_quiz_mastery_from_lives);
    run("MAL-OS: commands, files, unlocks, the dungeon", test_mal_os);
    run("wardrobe, snake and minigame scores", test_wardrobe_snake_scores);
    run("memory city: islands, stops and labels", test_memory_city);
    run("Versus spectators: predictions, payouts, watching", test_spectators);
    run("Versus: Mal's look for players and the crowd", test_versus_looks);
    run("the wardrobe shop: buying with bytes", test_wardrobe_shop);
    run("daily loop: streak, const freezes, quests, chests", test_daily);
    run("Outputle: puzzles, colours, guesses, streak", test_outputle);
    run("the C Codex: entries, unlocking, search", test_codex);
    run("the Bugdex: quick fixes, bosses, catching", test_bugdex);
    run("the Memory Farm: growing, wilting, harvest, drone", test_farm);

    std::printf("\n%d of %d tests passed\n", testsRun - testsFailed, testsRun);
    return testsFailed == 0 ? 0 : 1;
}
