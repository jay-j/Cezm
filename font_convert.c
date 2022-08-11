// Jay Jasper, 2022

// system standard libraries
#include <stdio.h>
#include <assert.h>
#include <string.h> // for memcpy
#include <time.h> // TODO random debug colors
#include <stdlib.h> // TODO random debug colors

// external dependencies
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

// custom parts
#include "font_bitmap.h"


int main(int argc, char* argv[]){
  if (argc < 3){
    printf("Useage: ./convert_ttf_to_bitmap.bin [ttf file] [fontsize]\n");
    printf("        ./convert_ttf_to_bitmap.bin font.ttf 14\n");
    return -1;
  }
  else{
    printf("TTF to Bitmap. Converting %s at size %d\n", argv[1], atoi(argv[2]));
  }
  
  // Init any SDL crap
  
  // Open the bitmap font file
  assert(TTF_Init() != -1);
  TTF_Font* font = TTF_OpenFont(argv[1], atoi(argv[2]));
  assert(font != NULL);
  
  int bitmap_width = 356;
  int bitmap_height = 356;
    
  uint32_t rmask, gmask, bmask, amask;
  #if SDL_BYTEORDER == SDL_BIG_ENDIAN
      rmask = 0xff000000;
      gmask = 0x00ff0000;
      bmask = 0x0000ff00;
      amask = 0x000000ff;
  #else
      rmask = 0x000000ff;
      gmask = 0x0000ff00;
      bmask = 0x00ff0000;
      amask = 0xff000000;
  #endif
    
  // create an image to write the data to. how big does this need to be? 
  SDL_Surface* surface_font = SDL_CreateRGBSurface(
      0, bitmap_width, bitmap_height, 32, rmask, gmask, bmask, amask);
  assert(surface_font != NULL);
  SDL_SetSurfaceBlendMode(surface_font, SDL_BLENDMODE_BLEND);

  // create data structure to record where characters are inside the texture
  FontMap font_map = fontmap_create();
  
  // render characters one by one to the image buffer
  // go through the ascii table  (increment integers) for faster later lookup
  // record their size/location in the image, store that in a useful way
  // TODO is there a standard for bitmap fonts?
  // Render as white so that later I can simply add a filter to the texture to get a different color
  srand(time(NULL));
  SDL_Color text_color = {
    .r = 0xFF,
    .g = 0xFF,
    .b = 0xFF,
    .a = 0xFF
  };

  uint8_t character[] = "a\0";
  character[0] = 32;
  printf("character first = %c\n", character[0]);

  SDL_Rect character_box;
  character_box.x = 0;
  character_box.y = 0;
  
  SDL_Rect bitmap_cursor;
  bitmap_cursor.x = 0;
  bitmap_cursor.y = 0;

  for(int i=32; i<127; ++i){
    SDL_Surface* surface_character = TTF_RenderText_Blended(font, (char*) character, text_color);
    if (surface_character == NULL){
      printf("Text '%s' texture render surface error: %s\n", character, SDL_GetError());
    }
    assert(surface_character != NULL);
  
    TTF_SizeText(font, (char*) character, &character_box.w, &character_box.h);
  
    // printf("This character '%c' (%u) is %d x %d\n", character[0], character[0], character_box.w, character_box.h);
  
    // write to the bigger texture
    bitmap_cursor.w = character_box.w;
    bitmap_cursor.h = character_box.h;
    if (bitmap_cursor.x + bitmap_cursor.w > bitmap_width){
      bitmap_cursor.x = 0;
      bitmap_cursor.y += bitmap_cursor.h; // TODO??
    }
    assert(bitmap_cursor.y + bitmap_cursor.h < bitmap_height);
    SDL_BlitSurface(surface_character, NULL, surface_font, &bitmap_cursor);
    
    // store where it is in the bigger map 
    fontmap_set_char(&font_map, (char) character[0], bitmap_cursor);

    // prep for next loop  
    SDL_FreeSurface(surface_character);
    bitmap_cursor.x += bitmap_cursor.w;
    character[0] += 1;
  }
  
  printf("Done creating the texture in memory. Saving...\n");
  
  // write the image to disk
  IMG_SavePNG(surface_font, "font.png");
  
  fontmap_file_save(&font_map, "font.dat");
  
  // TODO temp fontmap testing
  // char letter = 'J';
  // SDL_Rect test = fontmap_get_char(&font_map, letter);
  // printf("Letter '%c', can be found at x=%d, y=%d, w=%d, h=%d\n", letter, test.x, test.y, test.w, test.h);
  
  // cleanup
  SDL_FreeSurface(surface_font);
  TTF_CloseFont(font);
}
