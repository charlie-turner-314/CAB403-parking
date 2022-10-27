
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

MKDIR_P ?= mkdir -p
# ---------------- NEED THESE ON LINUX I THINK --------------------------
LDFLAGS = -lrt -lpthread -lm


# compile executables for C files in src directory
main: $(foreach SRC, $(filter src/%, $(SRCS)), $(MAIN_EXEC_DIR)/$(notdir $(SRC:.c=)))
	@echo "Run the executables with:"
	@echo "\033[0;32m Simulator: ./build/bin/simulator\033[0m"
	@echo "\033[0;34m   Manager: ./build/bin/manager\033[0m"
	@echo "\033[0;31m Firealarm: ./build/bin/firealarm\033[0m\n"
	@echo "Make and Run tests with: \033[0;33mmake all\033[0m"

all: main runtests

# Ensure objs doesn't contain the other objects in the src directory or the test directory
$(MAIN_EXEC_DIR)/% : SRCS := $(filter-out src/%, $(filter-out test/%, $(SRCS)))
$(MAIN_EXEC_DIR)/% : OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)
# add the current object to the list of objects, as it was removed above
$(MAIN_EXEC_DIR)/% : $(OBJS)
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(OBJ_DIR)/src/$*.o $(LDFLAGS)

runtests: test
runtests: 	
	@for exec in $(shell find $(TEST_EXEC_DIR) -type f); do \
		echo "Running $$exec"; \
		$$exec; \
	done

# test: basically same as main but set srcs to test files instead of main files
test: $(foreach SRC, $(filter test/%, $(SRCS)), $(TEST_EXEC_DIR)/$(notdir $(SRC:.c=)))

$(TEST_EXEC_DIR)/% : SRCS := $(filter-out src/%, $(filter-out test/%, $(SRCS)))
$(TEST_EXEC_DIR)/% : OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)
$(TEST_EXEC_DIR)/% : $(OBJS)
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(OBJ_DIR)/test/$*.o $(LDFLAGS)



# Build an object file (OBJS)
$(OBJ_DIR)/%.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@



.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)


# keep obj files around for a rainy day
.SECONDARY: $(OBJS)