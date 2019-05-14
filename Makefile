CC=gcc
PROGRAM = control.out
CFLAGS = -g -O3 -o $(PROGRAM)
CLIBS = -lrt -lpthread
HOST = localhost
PORT = 9999

compile: controlemanual.c
	$(CC) $(CFLAGS) $< $(CLIBS)

server:
	java -jar aquecedor2008_1.jar $(PORT)

run: compile
	./$(PROGRAM) $(HOST) $(PORT)


.PHONY: server run
