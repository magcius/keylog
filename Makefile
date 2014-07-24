
pkgs = x11 xtst xdamage gtk+-3.0
CFLAGS = $(shell pkg-config --cflags $(pkgs)) -g -O0 -Wall -Werror
LDFLAGS = $(shell pkg-config --libs $(pkgs))

main: main.o keylogger.o drawer.o utils.o

all: main
