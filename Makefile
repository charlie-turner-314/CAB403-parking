
# path to build to
BUILD_DIR ?= ./build

# path to object files
OBJ_DIR ?= $(BUILD_DIR)/obj

# path to store the executables
MAIN_EXEC_DIR = $(BUILD_DIR)/bin
TEST_EXEC_DIR = $(BUILD_DIR)/test

# directories with source files
SRC_DIRS ?= src libs test

# path(s) to literally all the source files
SRCS := $(shell find $(SRC_DIRS) -name *.c)
# path(s) to all the object files (may not actually exist at this time)
OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)
# path(s) to all the dependency files (may not actually exist at this time)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
CC = gcc
CFLAGS = -g -Wall -Wextra -Werror $(INC_FLAGS) 

# ---------------- NEED THESE ON LINUX I THINK --------------------------
LDFLAGS = -lrt -lpthread -lm

all: main test

# compile executables for C files in src directory
main: $(foreach SRC, $(filter src/%, $(SRCS)), $(MAIN_EXEC_DIR)/$(notdir $(SRC:.c=)))

# Ensure objs doesn't contain the other objects in the src directory or the test directory

$(MAIN_EXEC_DIR)/% : SRCS := $(filter-out src/%, $(filter-out test/%, $(SRCS)))
$(MAIN_EXEC_DIR)/% : OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)
# add the current object to the list of objects, as it was removed above
$(MAIN_EXEC_DIR)/% : $(OBJS)
	$(MKDIR_P) $(MAIN_EXEC_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(OBJ_DIR)/src/$*.o $(LDFLAGS)


# test: basically same as main but set srcs to test files instead of main files
test: $(foreach SRC, $(filter test/%, $(SRCS)), $(TEST_EXEC_DIR)/$(notdir $(SRC:.c=)))

$(TEST_EXEC_DIR)/% : SRCS := $(filter-out src/%, $(filter-out test/%, $(SRCS)))
$(TEST_EXEC_DIR)/% : OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)
$(TEST_EXEC_DIR)/% : $(OBJS)
	@echo "OBJS $(OBJS)"
	$(MKDIR_P) $(TEST_EXEC_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(OBJ_DIR)/test/$*.o $(LDFLAGS)

# Build an object file (OBJS)
$(OBJ_DIR)/%.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

runtests: EXECS := $(shell find $(TEST_EXEC_DIR) -type f)
runtests: 	
	@for exec in $(EXECS); do \
		echo "Running $$exec"; \
		$$exec; \
	done


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p

# keep obj files around for a rainy day
.SECONDARY: $(OBJS)