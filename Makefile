APP = mc_barf
CC = gcc
CFLAGS = -g3 -Wall -pedantic -std=c99
all: $(APP)

$(APP): main.c
	$(CC) $^ $(CFLAGS) -o $@

test: $(APP)
	./$(APP) ./microcode.bin

clean:
	$(RM) $(APP)
