# ------------------------------------------------
# Generic Makefile (based on gcc/g++)
# ------------------------------------------------

######################################
# target
######################################
TARGET = demo

######################################
# unit test target (separate binary, sources stay out of demo C_SOURCES)
######################################
TEST_TARGET = test_cjson_configur

######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Og

#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

######################################
# source files
######################################
# C sources
C_SOURCES :=
C_SOURCES += main.c
C_SOURCES += libs/queue/queue.c
C_SOURCES += libs/log.c/log.c
C_SOURCES += libs/unity/unity.c
C_SOURCES += libs/cjson/cJSON.c
C_SOURCES += libs/cjson/cJSON_Utils.c
C_SOURCES += libs/cjson_configur/cjson_configur.c

# unit test sources (own binary with own main, never part of C_SOURCES)
TEST_SOURCES :=
TEST_SOURCES += libs/cjson_configur/tests/test_cjson_configur.c

# test objects (flat build dir, resolved via vpath)
TEST_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(TEST_SOURCES:.c=.o)))


#######################################
# binaries
#######################################
# either it can be added to the PATH environment variable.
PREFIX := 
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size

#######################################
# CFLAGS/CXXFLAGS
#######################################
# C defines
C_DEFS := 

# C includes
C_INCLUDES :=
C_INCLUDES += -Iinclude
C_INCLUDES += -Ilibs
C_INCLUDES += -Ilibs/queue
C_INCLUDES += -Ilibs/log.c
C_INCLUDES += -Ilibs/unity
C_INCLUDES += -Ilibs/cjson
C_INCLUDES += -Ilibs/cjson_configur

# Common flags for both C and C++
COMMON_FLAGS := $(OPT) -Wall -fdata-sections -ffunction-sections

# C specific flags
CFLAGS := $(C_DEFS) $(C_INCLUDES) $(COMMON_FLAGS)
CFLAGS += -pthread

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# Generate dependency information
CFLAGS += -MMD -MP

#######################################
# LDFLAGS
#######################################
LIBS = -lc -lm -lpthread 
LIBDIR = 
LDFLAGS = $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# Linker flags for the unit test binary (own map file, does not touch demo.map)
TEST_LDFLAGS = $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TEST_TARGET).map,--cref -Wl,--gc-sections

# Use g++ as linker to ensure C++ standard library is linked correctly
LD = $(CC)

# default action: build all
all: $(BUILD_DIR)/$(TARGET)

#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o))) \

# Dependency files (demo + test objects)
DEPS = $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

# VPATH to find source files (demo + test)
vpath %.c $(sort $(dir $(C_SOURCES) $(TEST_SOURCES)))

# Compile C files
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "Compiling C: $<"
	@$(CC) -c $(CFLAGS) -MF"$(@:%.o=%.d)" -MT"$@" $< -o $@


# Link all objects
$(BUILD_DIR)/$(TARGET): $(OBJECTS) Makefile
	@echo "Linking target: $@"
	@$(LD) $(OBJECTS) $(LDFLAGS) -o $@
	@$(SZ) $@
	@echo "Build complete: $@"

# Create build directory
$(BUILD_DIR):
	@mkdir -p $@

#######################################
# build & run the unit tests
#######################################
# test objects: reuse the library objects, drop the demo main, add the test objects
TEST_LINK_OBJECTS = $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) $(TEST_OBJECTS)

$(BUILD_DIR)/$(TEST_TARGET): $(TEST_LINK_OBJECTS) Makefile
	@echo "Linking target: $@"
	@$(LD) $(TEST_LINK_OBJECTS) $(TEST_LDFLAGS) -o $@
	@$(SZ) $@
	@echo "Build complete: $@"

# runs the suite; non-zero exit code on any test failure
test: $(BUILD_DIR)/$(TEST_TARGET)
	@echo "Running unit tests: $(BUILD_DIR)/$(TEST_TARGET)"
	$(BUILD_DIR)/$(TEST_TARGET)

#######################################
# clean up
#######################################
clean:
ifeq ($(OS),Windows_NT)
	@rd /s /q $(BUILD_DIR) 2>nul || echo "Clean completed"
else
	@rm -rf $(BUILD_DIR) 2>/dev/null || true
	@echo "Clean completed"
endif

#######################################
# dependencies
#######################################
-include $(DEPS)

# Helper target to show variables
show:
	@echo "C_SOURCES: $(C_SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"

.PHONY: all clean test show

# *** EOF ***
