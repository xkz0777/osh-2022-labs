CC=g++
CCFLAGS=-std=c++17 -O2 -W -Wall -g
TARGET=shell
OBJ=shell.obj

$(TARGET):
	$(CC) $(CCFLAGS) -o $(TARGET).out $(TARGET).cpp

default: all

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -f *.out