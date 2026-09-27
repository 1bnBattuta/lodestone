# Lodestone build
#   make                debug build (ASan + UBSan)   -> build/debug/
#   make release        optimized build (-O2)        -> build/release/
#   make capture        only lodestone-capture
#   make test           build and run tests/*.c
#   make clean
#
# BUILD=debug|release selects the mode for any target, e.g. `make BUILD=release capture`.
# Each mode has its own build directory so switching never mixes objects.
#
# Binaries:
#   lodestone-capture   src/capture + src/common
#   lodestone-parse     src/parse   + src/common

CAPTURE_NAME := lodestone-capture
PARSE_NAME   := lodestone-parse

# mode
BUILD ?= debug
ifeq ($(BUILD),release)
  MODE_CFLAGS  := -O2 -g -DNDEBUG
  MODE_LDFLAGS :=
else ifeq ($(BUILD),debug)
  MODE_CFLAGS  := -O0 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined
  MODE_LDFLAGS := -fsanitize=address,undefined
else
  $(error BUILD must be 'debug' or 'release', got '$(BUILD)')
endif

BUILD_DIR := build/$(BUILD)

# toolchain
CC       := gcc
INC_DIRS := $(shell find src -type d)
CPPFLAGS := $(addprefix -I,$(INC_DIRS)) -MMD -MP
CFLAGS   := -Wall -Wextra -Wpedantic -Wshadow $(MODE_CFLAGS)
LDFLAGS  := $(MODE_LDFLAGS)

# sources
src_in = $(shell find $(1) -name '*.c' 2>/dev/null)
obj_of = $(patsubst %.c,$(BUILD_DIR)/%.o,$(1))

COMMON_SRCS  := $(call src_in,src/common)
CAPTURE_SRCS := $(call src_in,src/capture)
PARSE_SRCS   := $(call src_in,src/parse)
TEST_SRCS    := $(wildcard tests/*.c)

COMMON_OBJS  := $(call obj_of,$(COMMON_SRCS))
CAPTURE_OBJS := $(call obj_of,$(CAPTURE_SRCS))
PARSE_OBJS   := $(call obj_of,$(PARSE_SRCS))
TEST_OBJS    := $(call obj_of,$(TEST_SRCS))

CAPTURE_BIN := $(BUILD_DIR)/$(CAPTURE_NAME)
PARSE_BIN   := $(BUILD_DIR)/$(PARSE_NAME)
TEST_BINS   := $(patsubst tests/%.c,$(BUILD_DIR)/tests/%,$(TEST_SRCS))

# Tests link every object except the two entry points.
LIB_OBJS := $(filter-out %/main.o,$(COMMON_OBJS) $(CAPTURE_OBJS) $(PARSE_OBJS))

# lodestone-parse joins the default build as soon as src/parse has a .c file
BINS := $(CAPTURE_BIN)
ifneq ($(PARSE_SRCS),)
  BINS += $(PARSE_BIN)
endif

# targets
.DEFAULT_GOAL := all
.PHONY: all capture parse debug release test clean

all: $(BINS)

capture: $(CAPTURE_BIN)

ifneq ($(PARSE_SRCS),)
parse: $(PARSE_BIN)
else
parse:
	@echo "lodestone-parse: no sources in src/parse yet" >&2; exit 1
endif

debug:
	@$(MAKE) --no-print-directory BUILD=debug all

release:
	@$(MAKE) --no-print-directory BUILD=release all

$(CAPTURE_BIN): $(CAPTURE_OBJS) $(COMMON_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(PARSE_BIN): $(PARSE_OBJS) $(COMMON_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# static pattern rule: only applies to TEST_BINS, never to the .o files.
$(TEST_BINS): $(BUILD_DIR)/tests/%: $(BUILD_DIR)/tests/%.o $(LIB_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Compiles src/**/*.c and tests/*.c
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "== $$t"; \
		./$$t || exit 1; \
	done

clean:
	rm -rf build

-include $(CAPTURE_OBJS:.o=.d) $(PARSE_OBJS:.o=.d) $(COMMON_OBJS:.o=.d) $(TEST_OBJS:.o=.d)
