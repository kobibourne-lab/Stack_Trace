// mos.cpp - MAL-OS: the shell, its files, its programs and the adventure.

#include "mos.h"
#include <algorithm>
#include <sstream>

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

void mos_print(Mos &m, const std::string &text, MosTone tone)
{
    // One MosLine per line of text
    std::string line;
    for (char c : text) {
        if (c == '\n') { m.out.push_back({ line, tone }); line.clear(); }
        else if (c != '\r') line += c;
    }
    m.out.push_back({ line, tone });
    if (m.out.size() > 400) m.out.erase(m.out.begin(), m.out.begin() + (m.out.size() - 400));
}

static std::vector<std::string> split_words(const std::string &line)
{
    std::vector<std::string> words;
    std::istringstream in(line);
    std::string w;
    while (in >> w) words.push_back(w);
    return words;
}

static std::string lower(std::string s)
{
    for (char &c : s) c = (char)tolower((unsigned char)c);
    return s;
}

std::string mos_default_hello()
{
    return "#include <stdio.h>\n"
           "\n"
           "int main(void) {\n"
           "    int x = 5;\n"
           "    int *p = &x;\n"
           "    *p = 7;\n"
           "    printf(\"Hello from MAL-OS! x is %d\\n\", x);\n"
           "    return 0;\n"
           "}\n";
}

// ---------------------------------------------------------------------------
// Programs
// ---------------------------------------------------------------------------

const std::vector<MosProgram> &mos_programs()
{
    static const std::vector<MosProgram> programs = {
        { "snake",      "Linked-list Snake", "you're a linked list: eat new nodes, don't hit yourself", 2 },
        { "heaphero",   "Heap Hero",         "free the leaked blocks before the heap fills up", 3 },
        { "stacktower", "Stack Tower",       "stack recursive calls, then watch them return", 5 },
        { "dungeon",    "dungeon",           "a text adventure: you're a pointer, lost in memory", 7 },
    };
    return programs;
}

bool mos_program_unlocked(const Mos &m, const MosInfo &info, const MosProgram &p)
{
    return m.unlockAll || info.level >= p.level;
}

static const MosProgram *find_program(const std::string &name)
{
    for (const MosProgram &p : mos_programs()) if (p.name == name) return &p;
    return nullptr;
}

// ---------------------------------------------------------------------------
// The file system: made up, and partly unlocked by progress
// ---------------------------------------------------------------------------

struct MosFile {
    std::string path;      // full path, e.g. /home/mal/diary/day1.txt
    bool dir;
    int bossesNeeded;      // locked until this many bosses are beaten
    int levelNeeded;       // or until this level
};

static std::vector<MosFile> all_files()
{
    return {
        { "/", true, 0, 0 },
        { "/bin", true, 0, 0 },
        { "/home", true, 0, 0 },
        { "/home/mal", true, 0, 0 },
        { "/home/mal/readme.txt", false, 0, 0 },
        { "/home/mal/hello.c", false, 0, 0 },
        { "/home/mal/diary", true, 0, 0 },
        { "/home/mal/diary/day1.txt", false, 0, 0 },
        { "/home/mal/diary/day2.txt", false, 1, 0 },
        { "/home/mal/diary/day3.txt", false, 2, 0 },
        { "/home/mal/diary/day4.txt", false, 3, 0 },
        { "/home/mal/diary/day5.txt", false, 4, 0 },
        { "/home/mal/intel", true, 0, 0 },
        { "/home/mal/intel/bosses.txt", false, 0, 0 },
        { "/home/mal/.secrets", false, 0, 4 },
    };
}

static bool file_open(const MosFile &f, const Mos &m, const MosInfo &info)
{
    if (m.unlockAll) return true;
    return info.bossesBeaten >= f.bossesNeeded && info.level >= f.levelNeeded;
}

// Turns what was typed into a full path, from the current directory.
static std::string resolve(const Mos &m, const std::string &arg)
{
    if (arg.empty() || arg == "~") return "/home/mal";
    std::string path = arg[0] == '/' ? arg : (arg[0] == '~' ? "/home/mal" + arg.substr(1) : m.cwd + "/" + arg);
    std::vector<std::string> parts;
    std::string part;
    for (size_t i = 0; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/') {
            if (part == "..") { if (!parts.empty()) parts.pop_back(); }
            else if (!part.empty() && part != ".") parts.push_back(part);
            part.clear();
        } else {
            part += path[i];
        }
    }
    std::string out;
    for (const std::string &p : parts) out += "/" + p;
    return out.empty() ? "/" : out;
}

static const MosFile *find_file(const std::string &path)
{
    static std::vector<MosFile> files = all_files();
    for (const MosFile &f : files) if (f.path == path) return &f;
    return nullptr;
}

static bool is_program_path(const std::string &path)
{
    return path.rfind("/bin/", 0) == 0 && find_program(path.substr(5)) != nullptr;
}

static std::string file_text(const Mos &m, const MosInfo &info, const std::string &path)
{
    std::string b1 = info.bossNames.size() > 0 ? info.bossNames[0] : "the first boss";
    std::string b2 = info.bossNames.size() > 1 ? info.bossNames[1] : "the second boss";
    std::string b3 = info.bossNames.size() > 2 ? info.bossNames[2] : "the third boss";
    std::string b4 = info.bossNames.size() > 3 ? info.bossNames[3] : "the last boss";

    if (path == "/home/mal/readme.txt")
        return "Welcome to MAL-OS, " + info.user + ".\n"
               "This is my computer. I'm Mal: a heap block at 0x9000. I live here.\n"
               "\n"
               "Things to try:\n"
               "  ls /bin          programs (more unlock as you level up)\n"
               "  run snake        play one\n"
               "  cat diary/day1.txt\n"
               "  nano hello.c     write some C, then gcc hello.c and ./a.out\n"
               "  help             everything else\n";
    if (path == "/home/mal/hello.c") return m.helloSource;
    if (path == "/home/mal/diary/day1.txt")
        return "DAY 1\n"
               "malloc(sizeof(Mal)). That's me. Born at 0x9000, 80 bytes, no idea why.\n"
               "Somebody's pointer points at me, so I'm safe. Blocks nobody points at get\n"
               "lost forever. They call them leaks. I try not to think about it.";
    if (path == "/home/mal/diary/day2.txt")
        return "DAY 2\n"
               + b1 + " is gone. It kept stopping one block short of the end. Turns out\n"
               "the last index is size - 1, not size. Who knew. (You did.)";
    if (path == "/home/mal/diary/day3.txt")
        return "DAY 3\n"
               "Someone called free() on my neighbour. A pointer still pointed at him.\n"
               "That's " + b2 + "'s trick. We sorted it. I set my own pointer to\n"
               "nullptr every night now, just in case.";
    if (path == "/home/mal/diary/day4.txt")
        return "DAY 4\n"
               + b3 + " ate every node nobody pointed at. We walked the whole list,\n"
               "fixed every next and prev, and it starved. 0 leaks. I cried a little.";
    if (path == "/home/mal/diary/day5.txt")
        return "DAY 5\n"
               + b4 + " copied me. Then the copy freed my memory, and I nearly went\n"
               "with it. A deep copy saved me: my own new char[], my own strcpy.\n"
               "Memory is clean. Thanks for coming down here with me.";
    if (path == "/home/mal/intel/bosses.txt") {
        std::string s = "BOSS INTEL (classified, obviously)\n";
        std::vector<std::pair<std::string, std::string>> intel = {
            { b1, "loops that run one too far or one short. Check every < and <=." },
            { b2, "pointers left pointing at freed memory. nullptr after free." },
            { b3, "blocks nobody points at. Every new needs its delete." },
            { b4, "copies that share a pointer. Write a copy constructor." },
        };
        for (size_t i = 0; i < intel.size(); i++)
            s += "\n" + intel[i].first + (info.bossesBeaten > (int)i ? "  [DEFEATED]" : "") + "\n  weakness: " + intel[i].second;
        return s;
    }
    if (path == "/home/mal/.secrets")
        return "You found the secrets file. Commands nobody told you about:\n"
               "  sudo make me a sandwich\n"
               "  make coffee\n"
               "  free mal\n"
               "  segfault\n"
               "  tree\n"
               "  malsay <words>\n"
               "  sudo rm -rf /     (please don't)";
    return "";
}

// ---------------------------------------------------------------------------
// The adventure
// ---------------------------------------------------------------------------

static void dungeon_look(Mos &m)
{
    Dungeon &d = m.dungeon;
    if (d.room == "main") {
        mos_print(m, "main()'s stack frame.", MosTone::Accent);
        mos_print(m, "You are p, a pointer. The frame is warm and orderly. A note is stuck to\n"
                     "the wall. Your pointer field, 'next', leads down into the heap.");
        if (d.freed) mos_print(m, "Everything is clean. You could 'return' now and end this.", MosTone::Good);
    } else if (d.room == "node1") {
        mos_print(m, "A heap node.  data = 7", MosTone::Accent);
        mos_print(m, "Quiet and tidy. Its 'next' leads on. " +
                     std::string(d.hasKey ? "" : "A small key lies here, stamped 'free'."));
    } else if (d.room == "node2") {
        mos_print(m, "A heap node.  data = 42", MosTone::Accent);
        mos_print(m, "Two pointers lead out of here. 'next' flickers, like it's pointing at\n"
                     "something that isn't there any more. 'other' looks solid.");
    } else if (d.room == "block") {
        mos_print(m, "A huge block. 4096 bytes.", MosTone::Accent);
        if (d.freed) mos_print(m, "Empty now. Freed. The Leaker is gone.", MosTone::Good);
        else mos_print(m, "Nothing points here but you. Something big is feeding on it: the Leaker,\n"
                          "growing fat on memory nobody will ever free.", MosTone::Bad);
    }
}

static void dungeon_go(Mos &m, const std::string &room)
{
    m.dungeon.trail.push_back(m.dungeon.room);
    m.dungeon.room = room;
    dungeon_look(m);
}

static void dungeon_command(Mos &m, MosInfo const &info, const std::vector<std::string> &w)
{
    (void)info;
    Dungeon &d = m.dungeon;
    d.moves++;
    std::string verb = w.empty() ? "" : lower(w[0]);
    std::string obj = w.size() > 1 ? lower(w[1]) : "";

    if (verb == "help" || verb == "?") {
        mos_print(m, "look  follow <pointer>  return  take <thing>  read <thing>  free <thing>\n"
                     "inventory  quit", MosTone::Dim);
    } else if (verb == "look" || verb == "l") {
        dungeon_look(m);
    } else if (verb == "read" && obj == "note" && d.room == "main") {
        mos_print(m, "The note says: 'The Leaker hides at the end of the list. Don't trust a\n"
                     "pointer that flickers. You'll need permission to free anything.'", MosTone::Lore);
    } else if (verb == "follow" || verb == "deref" || verb == "go") {
        if (obj.empty()) { mos_print(m, "Follow what? (e.g. follow next)", MosTone::Dim); return; }
        if (d.room == "main" && obj == "next") dungeon_go(m, "node1");
        else if (d.room == "node1" && obj == "next") dungeon_go(m, "node2");
        else if (d.room == "node2" && obj == "other") dungeon_go(m, "block");
        else if (d.room == "node2" && obj == "next") {
            mos_print(m, "You follow the flickering pointer into memory that was already freed...", MosTone::Bad);
            mos_print(m, "Segmentation fault (core dumped)", MosTone::Bad);
            mos_print(m, "You wake up back in main()'s stack frame. That was a dangling pointer.", MosTone::Dim);
            d.room = "main";
            d.trail.clear();
        } else {
            mos_print(m, "There's no pointer called '" + obj + "' here.", MosTone::Dim);
        }
    } else if (verb == "return" || verb == "back") {
        if (d.room == "main") {
            if (d.freed) {
                d.won = true;
                mos_print(m, "return 0;", MosTone::Accent);
                mos_print(m, "You climb out of main() with zero leaks, in " + std::to_string(d.moves) + " moves.\n"
                             "The Leaker is gone. Well done, pointer.", MosTone::Good);
                mos_print(m, "[dungeon complete: type anything to go back to the shell]", MosTone::Dim);
            } else {
                mos_print(m, "You can't return yet. Something down there is leaking.", MosTone::Dim);
            }
        } else {
            d.room = d.trail.empty() ? "main" : d.trail.back();
            if (!d.trail.empty()) d.trail.pop_back();
            dungeon_look(m);
        }
    } else if (verb == "take" || verb == "get") {
        if (d.room == "node1" && obj == "key" && !d.hasKey) {
            d.hasKey = true;
            mos_print(m, "You take the key. It's stamped 'free'. Permission granted.", MosTone::Good);
        } else {
            mos_print(m, "There's nothing like that to take here.", MosTone::Dim);
        }
    } else if (verb == "free" || verb == "delete") {
        if (d.room != "block" || d.freed) { mos_print(m, "There's nothing here that needs freeing.", MosTone::Dim); return; }
        if (!d.hasKey) { mos_print(m, "You try, but you don't have permission. Somewhere there's a key.", MosTone::Bad); return; }
        d.freed = true;
        mos_print(m, "free(block);", MosTone::Accent);
        mos_print(m, "The 4096 bytes go back to the heap. The Leaker shrieks, shrinks to\n"
                     "nothing and is gone. Now head back to main() and return.", MosTone::Good);
    } else if (verb == "inventory" || verb == "i") {
        mos_print(m, d.hasKey ? "You have: a key stamped 'free'." : "You have nothing but your own address.", MosTone::Dim);
    } else if (verb == "quit" || verb == "exit") {
        m.inDungeon = false;
        mos_print(m, "You leave the dungeon. It'll still be leaking when you come back.", MosTone::Dim);
    } else {
        mos_print(m, "You can't do that here. Type help.", MosTone::Dim);
    }
}

// ---------------------------------------------------------------------------
// The shell
// ---------------------------------------------------------------------------

void mos_boot(Mos &m, const MosInfo &info)
{
    m.out.clear();
    if (m.helloSource.empty()) m.helloSource = mos_default_hello();
    mos_print(m, "MAL-OS 1.0  (c) Mal, 0x9000", MosTone::Accent);
    mos_print(m, "checking memory........ 640K OK", MosTone::Dim);
    mos_print(m, "loading pointers....... OK", MosTone::Dim);
    mos_print(m, "scanning for leaks..... 0 found", MosTone::Dim);
    mos_print(m, "");
    mos_print(m, "Welcome, " + info.user + ". You're level " + std::to_string(info.level) + ". Type help, or cat readme.txt.");
}

std::string mos_prompt(const Mos &m, const MosInfo &info)
{
    if (m.inDungeon) return "> ";
    std::string dir = m.cwd;
    if (dir.rfind("/home/mal", 0) == 0) dir = "~" + dir.substr(9);
    return info.user + "@mal-os:" + dir + "$ ";
}

static void list_dir(Mos &m, const MosInfo &info, const std::string &path, bool all)
{
    if (path == "/bin") {
        for (const MosProgram &p : mos_programs()) {
            bool open = mos_program_unlocked(m, info, p);
            std::string row = p.name;
            row.resize(12, ' ');
            if (open) mos_print(m, row + p.about, MosTone::Good);
            else mos_print(m, row + "[locked: level " + std::to_string(p.level) + "]", MosTone::Dim);
        }
        return;
    }
    std::string prefix = path == "/" ? "/" : path + "/";
    bool any = false;
    for (const MosFile &f : all_files()) {
        if (f.path.size() <= prefix.size() || f.path.rfind(prefix, 0) != 0) continue;
        std::string rest = f.path.substr(prefix.size());
        if (rest.find('/') != std::string::npos) continue;   // only this directory, not below
        if (rest[0] == '.' && !all) continue;                  // hidden files need ls -a
        any = true;
        bool open = file_open(f, m, info);
        if (f.dir) mos_print(m, rest + "/", MosTone::Accent);
        else if (open) mos_print(m, rest);
        else if (f.bossesNeeded > 0) mos_print(m, rest + "   [locked: beat " + std::to_string(f.bossesNeeded) + (f.bossesNeeded == 1 ? " boss]" : " bosses]"), MosTone::Dim);
        else mos_print(m, rest + "   [locked: level " + std::to_string(f.levelNeeded) + "]", MosTone::Dim);
    }
    if (path == "/home/mal" && m.compiled) { mos_print(m, "a.out", MosTone::Good); any = true; }
    if (!any) mos_print(m, "(empty)", MosTone::Dim);
}

static const char *MAL_QUOTES[] = {
    "I'm 80 bytes of pure enthusiasm.",
    "Every time you forget a delete, a heap block gets lonely.",
    "Pointers are just addresses with confidence.",
    "If you can draw it in boxes and arrows, you can code it.",
    "I set my pointers to nullptr before bed. Safety first.",
    "Off-by-one errors are the most common kind of off-by-one error.",
};

static const char *FORTUNES[] = {
    "A pointer that isn't set to anything points somewhere. Set it to nullptr.",
    "sizeof(arr) / sizeof(arr[0]) only works where arr is still an array.",
    "new goes with delete. new[] goes with delete[]. malloc goes with free.",
    "Strings end with '\\0'. Always leave room for it.",
    "A function gets a copy of what you pass, unless you pass its address.",
    "In a loop over n items, the last index is n - 1.",
    "If your class has a pointer member, it probably needs a copy constructor.",
};

MosResult mos_run(Mos &m, const MosInfo &info, const std::string &rawLine)
{
    MosResult r;
    if (m.helloSource.empty()) m.helloSource = mos_default_hello();
    std::string line = rawLine;
    while (!line.empty() && line.back() == ' ') line.pop_back();
    mos_print(m, mos_prompt(m, info) + line, MosTone::Echo);
    if (!line.empty()) m.history.push_back(line);
    std::vector<std::string> w = split_words(line);
    if (w.empty()) return r;

    if (m.inDungeon) {
        if (m.dungeon.won) {   // finished: any key goes back to the shell
            m.inDungeon = false;
            r.action = MosAction::RunGame;
            r.arg = "dungeon-won";
            return r;
        }
        dungeon_command(m, info, w);
        return r;
    }

    std::string cmd = w[0];
    std::string arg = w.size() > 1 ? w[1] : "";
    std::string rest = line.size() > cmd.size() ? line.substr(cmd.size() + 1) : "";

    if (cmd == "help") {
        mos_print(m, "ls [-a] [dir]   list files          cd <dir>        change directory\n"
                     "cat <file>      show a file         pwd             where am I\n"
                     "run <program>   play a program      ./<program>     same thing\n"
                     "nano hello.c    edit some C         gcc hello.c     compile it (then ./a.out)\n"
                     "wardrobe        dress Mal up        scores          your best scores\n"
                     "whoami  level   about you           mal  fortune    wisdom\n"
                     "history clear   the usual           exit            stand up from the desk", MosTone::Dim);
    } else if (cmd == "ls") {
        bool all = false;
        std::string target;
        for (size_t i = 1; i < w.size(); i++) { if (w[i][0] == '-') all = all || w[i].find('a') != std::string::npos; else target = w[i]; }
        std::string path = target.empty() ? m.cwd : resolve(m, target);
        const MosFile *f = find_file(path);
        if (path == "/bin" || (f && f->dir)) list_dir(m, info, path, all);
        else if (f) mos_print(m, target);
        else mos_print(m, "ls: " + target + ": No such file or directory", MosTone::Bad);
    } else if (cmd == "cd") {
        std::string path = resolve(m, arg);
        const MosFile *f = find_file(path);
        if (f && f->dir) m.cwd = path;
        else if (f) mos_print(m, "cd: " + arg + ": Not a directory", MosTone::Bad);
        else mos_print(m, "cd: " + arg + ": No such file or directory", MosTone::Bad);
    } else if (cmd == "pwd") {
        mos_print(m, m.cwd);
    } else if (cmd == "cat" || cmd == "less" || cmd == "more") {
        if (arg.empty()) { mos_print(m, "cat: which file? (try cat readme.txt)", MosTone::Dim); return r; }
        std::string path = resolve(m, arg);
        const MosFile *f = find_file(path);
        if (is_program_path(path)) mos_print(m, "@#!%&*... that's a program, not text. Try run " + path.substr(5), MosTone::Dim);
        else if (!f) mos_print(m, "cat: " + arg + ": No such file or directory", MosTone::Bad);
        else if (f->dir) mos_print(m, "cat: " + arg + ": Is a directory", MosTone::Bad);
        else if (!file_open(*f, m, info)) mos_print(m, "cat: " + arg + ": Permission denied (locked)", MosTone::Bad);
        else mos_print(m, file_text(m, info, path), path.find("diary") != std::string::npos ? MosTone::Lore : MosTone::Normal);
    } else if (cmd == "run" || cmd.rfind("./", 0) == 0 || find_program(cmd) != nullptr) {
        std::string name = cmd == "run" ? arg : (cmd.rfind("./", 0) == 0 ? cmd.substr(2) : cmd);
        if (name.rfind("/bin/", 0) == 0) name = name.substr(5);
        if (name == "a.out") {
            if (!m.compiled) mos_print(m, "./a.out: not found. Compile something first: gcc hello.c", MosTone::Bad);
            else r.action = MosAction::RunProgram;
            return r;
        }
        const MosProgram *p = find_program(name);
        if (name.empty()) mos_print(m, "run what? Try ls /bin", MosTone::Dim);
        else if (!p) mos_print(m, name + ": command not found", MosTone::Bad);
        else if (!mos_program_unlocked(m, info, *p))
            mos_print(m, name + ": locked. Reach level " + std::to_string(p->level) + " to run it (you're level " +
                         std::to_string(info.level) + ").", MosTone::Bad);
        else if (p->name == "dungeon") {
            m.inDungeon = true;
            m.dungeon = Dungeon();
            mos_print(m, "DUNGEON: a text adventure in memory. Type help for the verbs.", MosTone::Accent);
            mos_print(m, "");
            dungeon_look(m);
        } else {
            mos_print(m, "Starting " + p->title + "...", MosTone::Good);
            r.action = MosAction::RunGame;
            r.arg = p->name;
        }
    } else if (cmd == "nano" || cmd == "vim" || cmd == "vi" || cmd == "edit" || cmd == "code") {
        if (cmd == "vim" || cmd == "vi") mos_print(m, "vim is too powerful for MAL-OS. Opening nano instead.", MosTone::Dim);
        std::string path = resolve(m, arg.empty() ? "hello.c" : arg);
        if (path != "/home/mal/hello.c") { mos_print(m, "nano: you can only edit ~/hello.c here", MosTone::Bad); return r; }
        r.action = MosAction::Edit;
        r.arg = "hello.c";
    } else if (cmd == "gcc" || cmd == "g++" || cmd == "cc" || cmd == "make") {
        if (cmd == "make" && arg == "coffee") {
            mos_print(m, "make: *** No rule to make target 'coffee'. Mal suggests tea.", MosTone::Bad);
            return r;
        }
        std::string path = resolve(m, arg.empty() ? "hello.c" : arg);
        if (path != "/home/mal/hello.c") { mos_print(m, cmd + ": " + arg + ": No such file", MosTone::Bad); return r; }
        mos_print(m, "compiling hello.c ...", MosTone::Dim);
        r.action = MosAction::Compile;
        r.arg = "hello.c";
    } else if (cmd == "wardrobe" || cmd == "closet") {
        r.action = MosAction::Wardrobe;
    } else if (cmd == "scores") {
        for (const MosProgram &p : mos_programs()) {
            if (p.name == "dungeon") continue;
            auto it = info.scores.find(p.name);
            std::string row = p.title;
            row.resize(20, ' ');
            mos_print(m, row + (it == info.scores.end() ? "-" : std::to_string(it->second)));
        }
        auto dg = info.scores.find("dungeon");
        mos_print(m, std::string("dungeon             ") + (dg == info.scores.end() ? "not finished" : "escaped"));
    } else if (cmd == "whoami") {
        mos_print(m, info.user);
    } else if (cmd == "level" || cmd == "xp") {
        mos_print(m, "Level " + std::to_string(info.level) + ", " + std::to_string(info.xp) + " XP. " +
                     std::to_string(info.topicsMastered) + " of " + std::to_string(info.topicsTotal) + " topics mastered, " +
                     std::to_string(info.bossesBeaten) + (info.bossesBeaten == 1 ? " boss" : " bosses") + " beaten.");
    } else if (cmd == "mal") {
        mos_print(m, std::string("Mal: ") + MAL_QUOTES[m.malQuote++ % 6], MosTone::Lore);
    } else if (cmd == "fortune") {
        mos_print(m, FORTUNES[m.history.size() % 7], MosTone::Lore);
    } else if (cmd == "malsay" || cmd == "cowsay") {
        std::string words = rest.empty() ? "Moo? I'm a heap block, not a cow." : rest;
        mos_print(m, " " + std::string(words.size() + 2, '_') + "\n< " + words + " >\n " + std::string(words.size() + 2, '-'), MosTone::Normal);
        mos_print(m, "    \\    ^\n     \\  [o o]\n        [ v ]   0x9000", MosTone::Good);
    } else if (cmd == "echo") {
        mos_print(m, rest);
    } else if (cmd == "date") {
        mos_print(m, info.today);
    } else if (cmd == "history") {
        for (size_t i = 0; i < m.history.size(); i++) mos_print(m, "  " + std::to_string(i + 1) + "  " + m.history[i], MosTone::Dim);
    } else if (cmd == "clear" || cmd == "cls") {
        m.out.clear();
    } else if (cmd == "tree") {
        mos_print(m, "        *\n       /|\\       your code tree grows outside the window.\n"
                     "      / | \\      " + std::to_string(info.topicsMastered) + " leaves fully grown.\n"
                     "        |\n      __|__", MosTone::Good);
    } else if (cmd == "free" && lower(arg) == "mal") {
        mos_print(m, "free(mal);", MosTone::Echo);
        mos_print(m, "Mal: WAIT WAIT WAIT. I still have a pointer pointing at me! That's a\n"
                     "dangling pointer waiting to happen! ...Nice try.", MosTone::Lore);
    } else if (cmd == "segfault") {
        mos_print(m, "Segmentation fault (core dumped)", MosTone::Bad);
        mos_print(m, "...just kidding. MAL-OS never crashes. Mostly.", MosTone::Dim);
    } else if (cmd == "rm") {
        if (rest.find("-rf /") != std::string::npos)
            mos_print(m, "rm: refusing to delete Mal's whole world. Also, it's not yours.", MosTone::Bad);
        else
            mos_print(m, "rm: Permission denied. Mal lives here.", MosTone::Bad);
    } else if (cmd == "sudo") {
        std::string what = lower(rest);
        if (what == "unlock-all" || what == "unlock all") {
            m.unlockAll = true;
            mos_print(m, "[sudo] password for " + info.user + ": ********", MosTone::Dim);
            mos_print(m, "Every program, file and wardrobe item is unlocked until you leave the room.", MosTone::Good);
        } else if (what.find("rm -rf /") != std::string::npos) {
            mos_print(m, "Mal: NO. NO NO NO. I live here!! You want ME in the recycle bin?!", MosTone::Bad);
            mos_print(m, "Mal: ...I'm telling the Leaker you did that.", MosTone::Lore);
        } else if (what == "make me a sandwich") {
            mos_print(m, "Okay.", MosTone::Good);
        } else if (what.empty()) {
            mos_print(m, "usage: sudo <command>", MosTone::Dim);
        } else {
            mos_print(m, info.user + " is not in the sudoers file. This incident will be reported to Mal.", MosTone::Bad);
        }
    } else if (cmd == "exit" || cmd == "logout" || cmd == "quit") {
        r.action = MosAction::Exit;
    } else if (cmd == "man") {
        mos_print(m, arg.empty() ? "What manual page do you want? (MAL-OS only has one: help)"
                                 : "No manual entry for " + arg + ". Try help.", MosTone::Dim);
    } else {
        mos_print(m, cmd + ": command not found. Type help.", MosTone::Bad);
    }
    return r;
}

std::string mos_complete(const Mos &m, const MosInfo &info, const std::string &line)
{
    (void)info;
    size_t space = line.find_last_of(' ');
    std::string start = space == std::string::npos ? "" : line.substr(0, space + 1);
    std::string word = space == std::string::npos ? line : line.substr(space + 1);

    std::vector<std::string> options;
    if (space == std::string::npos) {
        const char *cmds[] = { "help", "ls", "cd", "cat", "pwd", "run", "nano", "gcc", "wardrobe", "scores", "whoami",
                               "level", "mal", "fortune", "history", "clear", "exit", "./a.out" };
        for (const char *c : cmds) options.push_back(c);
        for (const MosProgram &p : mos_programs()) options.push_back(p.name);
    } else if (start.rfind("run ", 0) == 0) {
        for (const MosProgram &p : mos_programs()) options.push_back(p.name);
    } else {
        // Files in the directory the word is heading into
        size_t slash = word.find_last_of('/');
        std::string dirPart = slash == std::string::npos ? "" : word.substr(0, slash + 1);
        std::string dir = resolve(m, dirPart.empty() ? "." : dirPart);
        std::string prefix = dir == "/" ? "/" : dir + "/";
        for (const MosFile &f : all_files()) {
            if (f.path.size() <= prefix.size() || f.path.rfind(prefix, 0) != 0) continue;
            std::string name = f.path.substr(prefix.size());
            if (name.find('/') != std::string::npos) continue;
            options.push_back(dirPart + name + (f.dir ? "/" : ""));
        }
    }

    std::vector<std::string> matches;
    for (const std::string &o : options) if (o.rfind(word, 0) == 0) matches.push_back(o);
    if (matches.empty()) return line;
    std::string common = matches[0];
    for (const std::string &o : matches) {
        size_t i = 0;
        while (i < common.size() && i < o.size() && common[i] == o[i]) i++;
        common = common.substr(0, i);
    }
    if (matches.size() == 1 && common.back() != '/') common += " ";
    return start + common;
}
