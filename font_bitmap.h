// these fonts are ascii characters 'space' (dec 32) through '~' (dec 126)
// accordingly, the arrays are indexed by ascii-32 so 'space'=0
// This system doesn't do any fancy kerning. Characters can be arbitray and different widths
// but those widths never overlap and do not change based on adjacent characters.

#include <assert.h>
#include <SDL2/SDL.h> // for the SDL_Rect struct

#define FONT_ALIGN_H_LEFT   (1<<1)
#define FONT_ALIGN_H_CENTER (1<<2)
#define FONT_ALIGN_H_RIGHT  (1<<3)
#define FONT_ALIGN_V_TOP    (1<<4)
#define FONT_ALIGN_V_CENTER (1<<5)
#define FONT_ALIGN_V_BOTTOM (1<<6)


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

// calculate the size in pixels of some text
SDL_Rect fontmap_calculate_size(Font* font, char* string, size_t string_length);

// render!
void fontmap_render_string(SDL_Renderer* renderer, SDL_Rect textbox, Font* font, SDL_Color color, char* string, size_t string_length, uint64_t properties);

