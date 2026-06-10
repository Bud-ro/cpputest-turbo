# cpputest-revibed — C11 core + thin C++ shim, POSIX make + gcc only.

CC       ?= gcc
CXX      ?= g++
AR       ?= ar

BUILD    := build
INCLUDES := -Iinclude

CFLAGS   ?= -O2 -g
CFLAGS   += -std=c11 -Wall -Wextra -Werror $(INCLUDES)
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++11 -Wall -Wextra -Werror $(INCLUDES)

CORE_SRC := $(wildcard src/core/*.c)
CORE_OBJ := $(CORE_SRC:src/core/%.c=$(BUILD)/core/%.o)
LIB      := $(BUILD)/libCppUTest.a

# The single C++ TU: global operator new/delete replacements for leak
# tracking. C-only consumers: `make CPPUTEST_C_ONLY=1` skips it (the C core
# and TestHarness_c work fully without it).
ifndef CPPUTEST_C_ONLY
SHIM_SRC := $(wildcard src/shim/*.cpp)
SHIM_OBJ := $(SHIM_SRC:src/shim/%.cpp=$(BUILD)/shim/%.o)
endif

MOCK_SRC := $(wildcard src/mock/*.c)
MOCK_OBJ := $(MOCK_SRC:src/mock/%.c=$(BUILD)/mock/%.o)
EXT_LIB  := $(BUILD)/libCppUTestExt.a

.PHONY: all check conformance clean

all: $(LIB) $(if $(MOCK_SRC),$(EXT_LIB))

$(BUILD)/core/%.o: src/core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mock/%.o: src/mock/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/shim/%.o: src/shim/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB): $(CORE_OBJ) $(SHIM_OBJ)
	$(AR) rcs $@ $^

$(EXT_LIB): $(MOCK_OBJ)
	$(AR) rcs $@ $^

check: all
	@./scripts/run-unit-tests.sh

conformance: all
	@./scripts/run-conformance.sh

clean:
	rm -rf $(BUILD)
