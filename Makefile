# Makefile for Linux
CC = gcc
TARGET = moria_crawler
SRCS = main.c
CFLAGS = -Wall -O2 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lSDL2_mixer

all: $(TARGET)
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
