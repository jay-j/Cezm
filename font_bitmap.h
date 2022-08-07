// these fonts are ascii characters 'space' (dec 32) through '~' (dec 126)
// accordingly, the arrays are indexed by ascii-32 so 'space'=0
// This system doesn't do any fancy kerning. Characters can be arbitray and different widths
// but those widths never overlap and do not change based on adjacent characters.

#include <assert.h>
#include <SDL2/SDL.h> // for the SDL_Rect struct

#define FONT_CHARACTERS 95
typedef struct FontMap{
  int max_width;
  int max_height;
  SDL_Rect character[FONT_CHARACTERS];
} FontMap;

typedef struct Font{
  FontMap map;
  SDL_Texture* texture;
} Font;

// create a FontMap. Nothing special except for forcing zeroing of the entire thing
FontMap fontmap_create();


// store the location of the graphic for this character in the overall map
void fontmap_set_char(FontMap* fm, char c, SDL_Rect rect);

// given a character, return a SDL_Rect describing where its graphic is found in the texture
SDL_Rect fontmap_get_char(FontMap* fm, char c);


// save FontMap struct to binary file
void fontmap_file_save(FontMap* fm, char* filename);

// load FontMap struct from binary file
Font fontmap_file_load(char* filename);


// render!
void fontmap_render_string(SDL_Renderer* renderer, SDL_Rect textbox, Font* font, char* string, size_t string_length);


