CFLAGS = -O0 -Wall -Wextra -g

# for Linux
LIB_BUILTIN = -lSDL2 -lm -lSDL2_ttf
LIB = schedule.h utilities-c/hash_lib/hashtable.h
LIBOBJ = utilities-c/hash_lib/hashtable.o

all: main.bin
	./main.bin

%.bin: %.o $(LIBOBJ)
	gcc $< $(CFLAGS) $(LIBOBJ) $(LIB_BUILTIN) -o $@

.PRECIOUS: %.o
%.o: %.c $(LIB)
	gcc -c $< $(CFLAGS) -o $@


.PHONY: clean
clean:
	rm -f *.o
	rm -f *.bin
