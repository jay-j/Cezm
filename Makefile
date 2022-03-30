CFLAGS = -O2 -Wall -Wextra

# for Linux
LIB_BUILTIN = -lSDL2 -lGL -lm -lGLU -lGLEW
LIB = nuklear.h nuklear_sdl_gl3.h
LIBOBJ = 

all: main.bin

%.bin: %.o $(LIBOBJ)
	gcc $< $(CFLAGS) $(LIBOBJ) $(LIB_BUILTIN) -o $@

.PRECIOUS: %.o
%.o: %.c $(LIB)
	gcc -c $< $(CFLAGS) -o $@


.PHONY: clean
clean:
	rm -f *.o
	rm -f *.bin
