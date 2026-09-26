TARGET_EXEC := lodestone
BUILD_DIR := ./build
SRC_DIRS := ./src
TEST_DIR := ./tests

SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

LIB_OBJS := $(filter-out %/main.c.o,$(OBJS))

TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS := $(TEST_SRCS:%=$(BUILD_DIR)/%.o)
TEST_BINS := $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%)
TEST_DEPS := $(TEST_OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CC := gcc
CPPFLAGS := $(INC_FLAGS) -MMD -MP
CFLAGS := -Wall -Wextra -Wpedantic -Wshadow -g3 -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined

# The final build step.
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compiles both src/**/*.c and tests/*.c
$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_BINS): $(BUILD_DIR)/tests/%: $(BUILD_DIR)/tests/%.c.o $(LIB_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

.PHONY: test
test: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "== $$t"; \
		./$$t || exit 1; \
	done

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)

-include $(DEPS) $(TEST_DEPS)