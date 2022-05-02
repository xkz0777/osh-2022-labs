CC = g++
CCFLAGS := -std=c++17 -O2 -W -Wall
DBGFLAGS := -g

all: strace.cpp
	$(CC) $(CCFLAGS) -o strace strace.cpp

debug: strace.cpp
	$(CC) $(CCFLAGS) $(DBGFLAGS) -o strace strace.cpp