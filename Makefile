# I got this file from (https://makefiletutorial.com/#makefile-cookbook)

EXEC ?= clox
BUILD_DIR := build
SRC_DIR := src

# Find all the C source files
SRCS := $(shell find $(SRC_DIR) -name *.c)
# Stripping 'src/' prefix
SRCS := $(notdir $(SRCS))

# String substitution for every source file
# For eg: hello.c -> ./build/hello.c.o
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

#String substitution for dependencies
DEPS := $(OBJS:.o=.d)

# We don't really need this right now coz we don't have any folders in ./src
# but keepin for futture reference
#
# Every folder in ./src will need to be passed to GCC so that it can find header files
INC_DIRS := $(shell find $(SRC_DIR) -type d)

# Add a prefix to INC_DIRS. So moduleA would become -ImoduleA. GCC understands this -I flag
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# These flags will tell the compiler to generate Makefiles for us
# The files will have .d instead of .o as the output
CPPFLAGS := $(INC_FLAGS) -MMD -MP

# General purpose flags for compiler
CFLAGS := -Wall  -Wextra -Wpedantic -g

# Final build step
$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Build step for C source
$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
	rm $(EXEC)

# Include the .d makefiles. The - at the front suppresses errors of missing
# Makefiles since initially all the files will be missing.
-include $(DEPS)
