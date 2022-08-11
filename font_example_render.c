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


// boilerplate SDL startup functions
void sdl_startup(SDL_Window** win, SDL_Renderer** renderer){
  if(SDL_Init(SDL_INIT_VIDEO) < 0){
    printf("SDL init failed! %s\n", SDL_GetError());
    assert(0);
  }
  
  *win = SDL_CreateWindow("hex plant", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1400, 1000, 
  SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
  assert(*win != NULL);
  
  *renderer = SDL_CreateRenderer(*win, -1, SDL_RENDERER_ACCELERATED);
  assert(*renderer != NULL);
  
  // TODO startup TTF and load a font
}

void sdl_cleanup(SDL_Window* win, SDL_Renderer* renderer){
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(win);
  SDL_Quit();
}


SDL_Texture* texture_load(SDL_Renderer* renderer, char* filename){
    
  // use SDL_Surface CPU rendering to send the texture to the GPU
  SDL_Surface* surf = IMG_Load(filename);
  if (surf == NULL){
    printf("Unable to load image at '%s' for texture. Error: %s\n", filename, IMG_GetError());
    assert(0);
  }
  
  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surf);
  if (texture == NULL){
    printf("Unable to create texture from image '%s'. Error: %s\n", filename, SDL_GetError());
    assert(0);
  }
  
  SDL_FreeSurface(surf); // don't need the SDL surface anymore
  return texture;
}


int main(){
    // startup SDL
    SDL_Window* window;
    SDL_Renderer* renderer;
    sdl_startup(&window, &renderer);
    
    // load bitmap font from file
    Font font = fontmap_file_load("font.dat");
    font.texture = texture_load(renderer, "font.png");
    
      
    // display some text in a performance testing way
    char text[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, \nsed do eiusmod tempor \nincididunt ut labore et dolore magna\n aliqua. Ut enim ad minim\n veniam, quis nostrud exercitation ullamco laboris \nnisi ut aliquip ex ea commodo consequat. \nDuis aute irure dolor in reprehenderit in voluptate velit esse cillum \ndolore eu fugiat nulla pariatur. Excepteur sint\n occaecat cupidatat non proident, \nsunt in culpa qui officia\n deserunt mollit anim id est laborum.\nLorem ipsum dolor sit amet, consectetur adipiscing elit, \nsed do eiusmod tempor \nincididunt ut labore et dolore magna\n aliqua. Ut enim ad minim\n veniam, quis nostrud exercitation ullamco laboris \nnisi ut aliquip ex ea commodo consequat. \nDuis aute irure dolor in reprehenderit in voluptate velit esse cillum \ndolore eu fugiat nulla pariatur. Excepteur sint\n occaecat cupidatat non proident, \nsunt in culpa qui officia\n deserunt mollit anim id est laborum.\n";
    size_t text_length = strlen(text);
    
    // loop rate control
    uint32_t timer_last_loop_start_ms = SDL_GetTicks();
    uint32_t timer_target_ms = 16;
    uint32_t timer_last_loop_duration_ms;    

    SDL_Event evt;
    
    uint8_t running = 1;

    while(running == 1){
        // rate control. for now [sim rate] == [frame rate]
        timer_last_loop_duration_ms = SDL_GetTicks() - timer_last_loop_start_ms;
        if (timer_last_loop_duration_ms < timer_target_ms){
          SDL_Delay(timer_target_ms - timer_last_loop_duration_ms);
        }
        timer_last_loop_start_ms = SDL_GetTicks();
        printf("Last Loop Duration: %u ms\n", timer_last_loop_duration_ms);        

        // sdl event poll look for exit
        while(SDL_PollEvent(&evt) != 0){
            if (evt.type == SDL_QUIT){
                running = 0;
            }
            if (evt.type == SDL_KEYDOWN){
                if (evt.key.keysym.sym == SDLK_ESCAPE){
                    running = 0;
                }
            }
        }

        // prepare to render
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderClear(renderer);
        
        SDL_Rect textbox;
        textbox.x = 10;
        textbox.y = 10;
        textbox.w = 1000;
        textbox.h = 500; // TODO has no effect
        
        SDL_Color color = {0, 128, 0, 255};
        
        // render the lots of text
        fontmap_render_string(renderer, textbox, &font, color, text, text_length, FONT_ALIGN_H_LEFT | FONT_ALIGN_V_TOP);
        
        // flip to complete rendering
        SDL_RenderPresent(renderer);

    }
    
    SDL_DestroyTexture(font.texture);
    sdl_cleanup(window, renderer);
    
    return 0;
}
