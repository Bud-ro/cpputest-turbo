# cpputest-revibed — C11 core + thin C++ shim, POSIX make + gcc only.

CC       ?= gcc
CXX      ?= g++
AR       ?= ar

BUILD    := build
INCLUDES := -Iinclude

# CFLAGS/CXXFLAGS belong to the user (make CFLAGS=-O0 must not break the
# build); the flags the build REQUIRES are kept separate and always applied.
CFLAGS       ?= -O2 -g
CXXFLAGS     ?= -O2 -g
REQ_CFLAGS   := -std=c11 -Wall -Wextra -Werror $(INCLUDES)
REQ_CXXFLAGS := -std=c++11 -Wall -Wextra -Werror $(INCLUDES)

PREFIX   ?= /usr/local
DESTDIR  ?=

CORE_SRC := $(wildcard src/core/*.c)
CORE_OBJ := $(CORE_SRC:src/core/%.c=$(BUILD)/core/%.o)
LIB      := $(BUILD)/libCppUTest.a

# The C++ TUs: global operator new/delete replacements for leak tracking and
# the out-of-line SimpleString. C-only consumers: `make CPPUTEST_C_ONLY=1`
# skips them (the C core and TestHarness_c work fully without them).
ifndef CPPUTEST_C_ONLY
SHIM_SRC := $(wildcard src/shim/*.cpp)
SHIM_OBJ := $(SHIM_SRC:src/shim/%.cpp=$(BUILD)/shim/%.o)
endif

MOCK_SRC := $(wildcard src/mock/*.c)
MOCK_OBJ := $(MOCK_SRC:src/mock/%.c=$(BUILD)/mock/%.o)
EXT_LIB  := $(BUILD)/libCppUTestExt.a

.PHONY: all check conformance clean install uninstall

all: $(LIB) $(if $(MOCK_SRC),$(EXT_LIB))

$(BUILD)/core/%.o: src/core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(REQ_CFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/mock/%.o: src/mock/%.c
	@mkdir -p $(dir $@)
	$(CC) $(REQ_CFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/shim/%.o: src/shim/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(REQ_CXXFLAGS) $(CXXFLAGS) -c $< -o $@

$(LIB): $(CORE_OBJ) $(SHIM_OBJ)
	$(AR) rcs $@ $^

$(EXT_LIB): $(MOCK_OBJ)
	$(AR) rcs $@ $^

check: all
	@./scripts/run-unit-tests.sh

conformance: all
	@./scripts/run-conformance.sh

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include \
	         $(DESTDIR)$(PREFIX)/lib/pkgconfig
	cp $(LIB) $(EXT_LIB) $(DESTDIR)$(PREFIX)/lib/
	cp -R include/CppUTest include/CppUTestExt include/cpputest_core \
	      $(DESTDIR)$(PREFIX)/include/
	sed 's|@PREFIX@|$(PREFIX)|' cpputest.pc.in \
	    > $(DESTDIR)$(PREFIX)/lib/pkgconfig/cpputest.pc

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/libCppUTest.a \
	      $(DESTDIR)$(PREFIX)/lib/libCppUTestExt.a \
	      $(DESTDIR)$(PREFIX)/lib/pkgconfig/cpputest.pc
	rm -rf $(DESTDIR)$(PREFIX)/include/CppUTest \
	       $(DESTDIR)$(PREFIX)/include/CppUTestExt \
	       $(DESTDIR)$(PREFIX)/include/cpputest_core

clean:
	rm -rf $(BUILD)
