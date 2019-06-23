CC=gcc
PROGRAM = control.out
CFLAGS = -g -Wall -O3 -o $(PROGRAM)
CLIBS = -lrt -lpthread -lncurses
HOST = localhost
PORT = 9999

compile: controlemanual.c
	$(CC) $(CFLAGS) $< $(CLIBS)

server:
	java -jar aquecedor2008_1.jar $(PORT)

run: compile
	./$(PROGRAM) $(HOST) $(PORT)

pdf: README.md
	pandoc README.md --to latex -o README.pdf

.PHONY: server run
