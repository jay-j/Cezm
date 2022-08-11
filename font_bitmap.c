#include "font_bitmap.h"

// create a FontMap. Nothing special except for forcing zeroing of the entire thing
FontMap fontmap_create(){
  FontMap fm;
  memset(&fm, 0, sizeof(fm));
  return fm;
}


// store the location of the graphic for this character in the overall map
void fontmap_set_char(FontMap* fm, char c, SDL_Rect rect){
  assert(c >= 32);
  assert(c <= 126);
  uint8_t index = (uint8_t) c - 32;
  fm->character[index].h = rect.h;
  fm->character[index].w = rect.w;
  fm->character[index].x = rect.x;
  fm->character[index].y = rect.y;

  // store the size of the biggest character
  if (rect.h > fm->max_height){
    fm->max_height = rect.h;
  }
  if (rect.w > fm->max_width){
    fm->max_width = rect.w;
  }
}


// given a character, return a SDL_Rect describing where its graphic is found in the texture
SDL_Rect fontmap_get_char(FontMap* fm, char c){
  if (c < 32){
    printf("going to fail with character: %c (%d)\n", c, (int) c);
  }
  assert(c >= 32);
  assert(c <= 126);
  uint8_t index = (uint8_t) c - 32;
  return fm->character[index];
}


// save FontMap struct to binary file
void fontmap_file_save(FontMap* fm, char* filename){
  FILE* fd = fopen(filename, "wb");
  assert(fd != NULL);
  fwrite(fm, sizeof(*fm), 1, fd);
  fclose(fd);
}


// load FontMap struct from binary file
Font fontmap_file_load(char* filename){
  FILE* fd = fopen(filename, "rb");
  assert(fd != NULL);
  Font font;
  fread(&font.map, sizeof(FontMap), 1, fd);
  fclose(fd);
  return font;
}


// parse the entire string and calculate the required textbox size
SDL_Rect fontmap_calculate_size(Font* font, char* string, size_t string_length){
  SDL_Rect dimensions = {0};
  dimensions.h += font->map.max_height;

  int width_current = 0;
  for (size_t i=0; i<string_length; ++i){
    if (string[i] == '\0'){
      break;
    }
    if (string[i] == '\n'){
      if (width_current > dimensions.w){
        dimensions.w = width_current;
        width_current = 0;
        dimensions.h += font->map.max_height;
      }
      continue;
    }

    SDL_Rect char_dim = fontmap_get_char(&font->map, string[i]);
    width_current += char_dim.w;

  }

  // store the max width even if there is only a single line
  if (width_current > dimensions.w){
    dimensions.w = width_current;
  }
  
  return dimensions;
}


// render a single character. Don't call directly - call fontmap_render_string()
// cursor_x and cursor_y are to the top left of the character
// does not line wrap
void fontmap_render_character(SDL_Renderer* renderer, int* cursor_x_px, int* cursor_y_px, Font* font, char character){
  // get info
  SDL_Rect src = fontmap_get_char(&font->map, character);
  
  SDL_Rect dest;
  dest.h = src.h;
  dest.w = src.w;
  dest.x = *cursor_x_px;
  dest.y = *cursor_y_px;
  
  // render
  SDL_RenderCopyEx(renderer, font->texture, &src, &dest, 0, NULL, SDL_FLIP_NONE);
  
  // move the cursor accordingly
  *cursor_x_px += src.w;
}


// handle endlines
// specify a rect to draw inside of. overflow goes out the bottom
// prerequisite is the textbox background to be shaded
// relies on the overall render flip to be done after this function
void fontmap_render_string(SDL_Renderer* renderer, SDL_Rect textbox, Font* font, SDL_Color color, char* string, size_t string_length, uint64_t properties){
  // calculate the required size then perform alignment
  SDL_Rect size_required = fontmap_calculate_size(font, string, string_length);
  int align_x = 0;
  
  // horizontal alignments
  if ((properties & FONT_ALIGN_H_CENTER) > 0){ // TODO alignment for multiline text...
    align_x = textbox.w / 2;
    align_x -= (size_required.w / 2);
  }
  else if ((properties & FONT_ALIGN_H_RIGHT) > 0){
    align_x = textbox.w;
    align_x -= size_required.w;
  }
  else{
    assert((properties & FONT_ALIGN_H_LEFT) > 0);
  }
  
  // vertical alignments
  int align_y = 0;
  if ((properties & FONT_ALIGN_V_BOTTOM) > 0){
    align_y = textbox.h - size_required.h;
  }
  else if ((properties & FONT_ALIGN_V_CENTER) > 0){
    align_y = textbox.h / 2;
    align_y -= size_required.h / 2;
  }
  else{
    assert((properties & FONT_ALIGN_V_TOP) > 0);
  }
  
  // setup blend mode that will apply to all the text
  SDL_SetTextureBlendMode(font->texture, SDL_BLENDMODE_BLEND);
  int check = SDL_SetTextureColorMod(font->texture, color.r, color.g, color.b);
  assert(check == 0);
  
  // loop through each character and render!
  int cursor_x = textbox.x + align_x;
  int cursor_y = textbox.y + align_y;
  for(size_t i=0; i<string_length; ++i){
    // check for control flow characters
    if (string[i] == '\0'){
      break;
    }
    if (string[i] == '\n'){
      cursor_y += font->map.max_height;
      cursor_x = textbox.x + align_x;
      continue;
    }
    
    // TODO line wrapping based on textbox.w
    
    // render the character
    fontmap_render_character(renderer, &cursor_x, &cursor_y, font, string[i]);
  }
}
