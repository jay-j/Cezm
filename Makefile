CFLAGS = -O0 -Wall -Wextra -g -Werror

# for Linux
LIB_BUILTIN = -lSDL2 -lm -lSDL2_ttf -lSDL2_image
LIB = schedule.h keyboard_bindings.h lib/hashtable.h lib/font_bitmap.h lib/profile_smoothdelay.h
LIBOBJ = lib/hashtable.o lib/font_bitmap.o lib/profile_smoothdelay.o

all: main.bin lib/font_example_render.bin lib/font_convert.bin
	./lib/font_convert.bin ./lib/Ubuntu-R.ttf 14
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
