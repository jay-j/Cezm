#ifndef TEXTBOX_H
#define TEXTBOX_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

typedef struct Bitmap_Font {
  SDL_Texture* texture;
  int texture_width;
  int texture_height;

  SDL_Rect characters[256];
} Bitmap_Font;


Bitmap_Font* textbox_font_create(SDL_Renderer* renderer){
  Bitmap_Font* bf = (Bitmap_Font*) malloc(sizeof(Bitmap_Font));

  
  // somehow ...
  textbox_font_size_set(renderer, bf);

}

void textbox_font_size_set(SDL_Renderer* renderer, Bitmap_Font* bf){
  // what is the correct texture access parameter? streaming is for frequenly changing
  bf->texture_width = 1024; // TODO do better than hardcoding...
  bf->texture_height = 1024;
  bf->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, bf->texture_width, bf->texture_height);
  if (bf->texture == NULL){
    printf("[ERROR] SDL failed to create font texture. Error %s\n", SDL_GetError());
  }
  assert(0);
  // TODO generate the tilemap at a certain resolution appropriate to the font size
  // then update the pixel offsets
}

void textbox_font_render(Bitmap_Font* bf){
  // get char array
  // for each value...
  // convert value to ascii
  // lookup location and dimensions of that ascii character in bt->characters
  // copy/tile/render that region of bt->texture
      // using SDL_RenderCopyEx()
  // advance horizontal by the bt->characters.width + 1

}


void textbox_font_destroy(Bitmap_Font* bf){
  SDL_DestroyTexture(bf->texture);
  free(bf);
  // TODO more / interior allocated stuff.
}

#endif /* TEXTBOX_H */


/* references
https://lazyfoo.net/tutorials/SDL/39_tiling/index.php
https://lazyfoo.net/tutorials/SDL/41_bitmap_fonts/index.php

*/
