CXX = g++
CXXFLAGS = -g -std=gnu++11 -Wall -I../dev -MMD -MP
OBJDIR = ../obj

PROG = cy86

# Entry point for this assignment
MAIN_CPP = ../dev/assignments/$(PROG).cpp

# Automatically find all shared .cpp files in ../dev (excluding the assignments folder)
SHARED_CPPS = $(wildcard ../dev/*.cpp)

# The objects to link are the current assignment entry point, plus all discovered shared objects
OBJS = $(OBJDIR)/assignments/$(PROG).o $(patsubst ../dev/%.cpp,$(OBJDIR)/%.o,$(SHARED_CPPS))

all: $(PROG)

# build $(PROG) application
$(PROG): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJDIR)/assignments/%.o: ../dev/assignments/%.cpp
	@mkdir -p $(OBJDIR)/assignments
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: ../dev/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

# test cy86 application
test: all
	scripts/run_all_tests.pl cy86 my tests
	scripts/compare_results.pl ref my tests
	@if [ -d ../cppgm.tests/course/pa9 ]; then \
		scripts/run_all_tests.pl cy86 my ../cppgm.tests/course/pa9; \
		scripts/compare_results.pl ref my ../cppgm.tests/course/pa9; \
	fi
# regenerate reference test output
ref-test:
	scripts/run_all_tests.pl cy86-ref ref tests

