

CC=gcc
CCFLAGS=-Wall -I. -Og
DEBUG_FLAGS=-g -ggdb -DDEBUG

main:spectre

spectre: spectre.c
	$(CC) $(CCFLAGS) $< -o $@ 

debug: debug_spectre


debug_spectre: spectre.c
	$(CC) $(CCFLAGS) $(DEBUG_FLAGS) $< -o $@ 

.PHONY : clean
clean:
	rm -f spectre debug_spectre
