
pkgs = x11 xtst gtk+-3.0
CFLAGS = $(shell pkg-config --cflags $(pkgs))
LDFLAGS = $(shell pkg-config --libs $(pkgs))

main: main.o keylogger.o

all: main
