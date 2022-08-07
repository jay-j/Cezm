CFLAGS = -O0 -Wall -Wextra -g -Werror

# for Linux
LIB_BUILTIN = -lSDL2 -lm -lSDL2_ttf -lSDL2_image
LIB = schedule.h keyboard_bindings.h utilities-c/hash_lib/hashtable.h font_bitmap.h
LIBOBJ = utilities-c/hash_lib/hashtable.o font_bitmap.o

all: main.bin font_example_render.bin font_convert.bin
	./font_convert.bin Hack_Font.ttf 14
	./main.bin examples/demo1.json

%.bin: %.o $(LIBOBJ)
	gcc $< $(CFLAGS) $(LIBOBJ) $(LIB_BUILTIN) -o $@

.PRECIOUS: %.o
%.o: %.c $(LIB)
	gcc -c $< $(CFLAGS) -o $@


.PHONY: clean
clean:
	rm -f *.o
	rm -f *.bin
