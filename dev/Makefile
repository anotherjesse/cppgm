TARGETS = \
	pptoken \
	posttoken \
	ctrlexpr \
	macro \
	preproc \
	recog \
	nsdecl \
	nsinit \
	cy86

CXX ?= g++
CXXFLAGS ?= -g -O2 -std=gnu++11 -Wall
CXXFLAGS += -Wl,--wrap=main
CPPFLAGS += -Isrc
DEPFLAGS = -MMD -MP
OBJ = ../obj/release

SHARED_SRCS = $(wildcard src/*.cpp)
SHARED_OBJS = $(patsubst src/%.cpp,$(OBJ)/src/%.o,$(SHARED_SRCS))
ENTRY_OBJS = $(addprefix $(OBJ)/,$(addsuffix .o,$(TARGETS)))

.PHONY: all clean

all: $(TARGETS)

$(TARGETS): %: $(OBJ)/%.o $(SHARED_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ)/%.o: %.cpp
	@mkdir -p $(OBJ)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJ)/src/%.o: src/%.cpp
	@mkdir -p $(OBJ)/src
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) $(ENTRY_OBJS) $(ENTRY_OBJS:.o=.d) \
		$(SHARED_OBJS) $(SHARED_OBJS:.o=.d)

-include $(ENTRY_OBJS:.o=.d)
-include $(SHARED_OBJS:.o=.d)
