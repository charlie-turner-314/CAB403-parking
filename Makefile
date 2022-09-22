# name of executable
TARGET_EXEC ?= main.out

TEST_EXEC ?= test.out

# path to build to
BUILD_DIR ?= ./build

# path(s) to source files
SRC_DIRS ?= ./src ./include ./test

# path(s) to all the source files
SRCS := $(shell find $(SRC_DIRS) -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
CC = gcc
CFLAGS = -Wall -Wextra -Werror $(INC_FLAGS)


# DEFAULT
main : OBJS:=$(filter-out $(BUILD_DIR)/./test/%.o, $(OBJS))
main : $(BUILD_DIR)/$(TARGET_EXEC)
# make the a.out file
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	@echo $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Should be called by all others that need to compile
# make c source objects
$(BUILD_DIR)/%.c.o: %.c
	@echo "Compiling $<"
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: OBJS:=$(filter-out $(BUILD_DIR)/./src/main.c.o, $(OBJS))
test : $(BUILD_DIR)/$(TEST_EXEC)  

$(BUILD_DIR)/$(TEST_EXEC): $(OBJS)
	@echo $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p