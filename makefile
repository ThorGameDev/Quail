# Define the compiler
CXX = clang++

# Define the source files
SOURCES = quail.cpp ./src/lexer.cpp ./src/externs.cpp ./src/parser.cpp ./src/logging.cpp ./src/BinopsData.cpp ./src/datatype.cpp ./src/output.cpp ./src/codegen.cpp ./src/codegen/optimizations.cpp ./src/codegen/constants.cpp ./src/codegen/other.cpp ./src/codegen/inblock.cpp ./src/codegen/BinOps.cpp ./src/codegen/functions.cpp ./src/codegen/core.cpp 

# Define the object files
OBJECTS = $(SOURCES:.cpp=.o)

# Define the flags
CXXFLAGS = -O0
FINALFLAGS = -fexceptions
LDFLAGS = -Xlinker --export-dynamic

# Get flags and libraries from llvm-config
LLVM_FLAGS = `llvm-config --cxxflags`
LLVM_LDFLAGS = `llvm-config --ldflags`
LLVM_SYSTEM_LIBS = `llvm-config --system-libs`
LLVM_LIBS = `llvm-config --libs core orcjit native`

# Combine all flags and libraries
FLAGS = $(CXXFLAGS) $(LLVM_FLAGS) $(FINALFLAGS)
LIBS = $(LLVM_LDFLAGS) $(LLVM_SYSTEM_LIBS) $(LLVM_LIBS)

# Define the target executable
TARGET = Quailpiler

# Default rule
all: $(TARGET)

# Rule to build the target
$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(TARGET)

# Pattern rule for building object files
%.o: %.cpp
	$(CXX) $(FLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean run
