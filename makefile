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
    LDFLAGS = -lraylib -lopengl32 -lgdi32 -lwinmm
    TARGET  = stacktrace.exe
    TESTS   = run_tests.exe
else
    CXX     = c++
    BREW   := $(shell brew --prefix 2>/dev/null)
    ifneq ($(BREW),)
        CXXFLAGS += -I$(BREW)/include
        LDFLAGS   = -L$(BREW)/lib -lraylib
    else
        LDFLAGS   = -lraylib
    endif
    TARGET  = stacktrace
    TESTS   = run_tests
endif

# The tests only use the files that don't draw, so they need no raylib and no window
TEST_SRC = tests/test_core.cpp content.cpp quiz.cpp progress.cpp practice.cpp editbuf.cpp

$(TARGET): $(SRC) $(wildcard *.h)
	$(CXX) $(CXXFLAGS) $(SRC) $(LDFLAGS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

# make test: build and run the automatic tests in tests/test_core.cpp
test: $(TESTS)
	./$(TESTS)

$(TESTS): $(TEST_SRC) content.h quiz.h progress.h practice.h editbuf.h
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $(TESTS)

clean:
	rm -f $(TARGET) $(TESTS) test_progress.txt
	rm -rf practice_build

.PHONY: run test clean
