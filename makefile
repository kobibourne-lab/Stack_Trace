# Stack Trace - the same makefile setup as the Hospital project, switched to C++.
#
#   Mac:      make run       (raylib from Homebrew: brew install raylib)
#   Windows:  mingw32-make run   (raylib's w64devkit, which has g++)

SRC       = $(wildcard *.cpp)
CXXFLAGS  = -std=c++17 -Wall -Wextra

ifeq ($(OS),Windows_NT)
    RAYLIB_BIN ?= C:/raylib/w64devkit/bin
    export PATH := $(RAYLIB_BIN);$(PATH)
    CXX     = g++
    NETLIBS = -lws2_32
    LDFLAGS = -lraylib -lopengl32 -lgdi32 -lwinmm $(NETLIBS)
    TARGET  = stacktrace.exe
    TESTS   = run_tests.exe
    # The .exe's own icon (File Explorer, a pinned taskbar button), from logo.ico
    ICON_RES = stacktrace_res.o
else
    CXX     = c++
    BREW   := $(shell brew --prefix 2>/dev/null)
    ifneq ($(BREW),)
        CXXFLAGS += -I$(BREW)/include
        LDFLAGS   = -L$(BREW)/lib -lraylib
    else
        LDFLAGS   = -lraylib
    endif
    NETLIBS =
    ICON_RES =
    TARGET  = stacktrace
    TESTS   = run_tests
endif

# The tests only use the files that don't draw, so they need no raylib and no window
TEST_SRC = tests/test_core.cpp content.cpp quiz.cpp progress.cpp practice.cpp editbuf.cpp versus.cpp net.cpp campaign.cpp citymap.cpp \
           mos.cpp wardrobe.cpp minigames.cpp daily.cpp

$(TARGET): $(SRC) $(wildcard *.h) $(ICON_RES)
	$(CXX) $(CXXFLAGS) $(SRC) $(ICON_RES) $(LDFLAGS) -o $(TARGET)

# Windows only: turn the icon into something the linker can put in the .exe
stacktrace_res.o: stacktrace.rc logo.ico
	windres stacktrace.rc -O coff -o stacktrace_res.o

run: $(TARGET)
	./$(TARGET)

# make test: build and run the automatic tests in tests/test_core.cpp
test: $(TESTS)
	./$(TESTS)

$(TESTS): $(TEST_SRC) content.h quiz.h progress.h practice.h editbuf.h versus.h net.h campaign.h citymap.h mos.h wardrobe.h minigames.h daily.h
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(NETLIBS) -o $(TESTS)

clean:
	rm -f $(TARGET) $(TESTS) test_progress.txt stacktrace_res.o
	rm -rf practice_build

.PHONY: run test clean
