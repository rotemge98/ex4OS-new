# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++17

# Output library
LIBRARY = libVirtualMemory.a

# Source files
SOURCES = PhysicalMemory.cpp VirtualMemory.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Headers and other files to include in tar
HEADERS = MemoryConstants.h PhysicalMemory.h VirtualMemory.h
EXTRA_FILES = Makefile README

# Default target
all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create tar archive
tar:
	tar -cvf e4.tar $(SOURCES) $(HEADERS) $(EXTRA_FILES)

# Clean generated files
clean:
	rm -f $(OBJECTS) $(LIBRARY) e4.tar

.PHONY: all clean tar