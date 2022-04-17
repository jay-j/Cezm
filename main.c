// Jay Jasper, 2022

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define FONTSIZE 20

TTF_Font* global_font = NULL;

void sdl_startup(SDL_Window** win, SDL_Renderer** render){

  if( SDL_Init(SDL_INIT_VIDEO) < 0){
    printf("SDL init failed! %s\n", SDL_GetError());
    assert(0);
  }
  
  *win = SDL_CreateWindow("Jay Demo", 
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
    WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
  assert(*win != NULL);

  *render = SDL_CreateRenderer(*win, -1, SDL_RENDERER_ACCELERATED);
  assert(*render != NULL);

  // startup TTF and load in a font
  assert(TTF_Init() != -1);
  global_font = TTF_OpenFont("Ubuntu-R.ttf", FONTSIZE);
  assert(global_font != NULL);
}


void sdl_cleanup(SDL_Window* win, SDL_Renderer* render){
  SDL_DestroyRenderer(render);
  SDL_DestroyWindow(win);
  SDL_Quit();
}


typedef struct TextBox{
  SDL_Texture* texture;
  SDL_Color color;
  int width;
  int height;
  int width_max;
} TextBox;


void sdlj_textbox_render(SDL_Renderer* render, TextBox* textbox, char* text){
  if (textbox->texture != NULL){
    SDL_DestroyTexture(textbox->texture);
  }

  // TODO TTF_RenderText_Blended_Wrapped() doesn't work / is undocumented
  // TODO write manual and multiline wrapping work
  SDL_Surface* surface = TTF_RenderText_Blended(global_font, text, textbox->color);
  if (surface == NULL){
    printf("texture render surface error: %s\n", SDL_GetError());
  }
  assert(surface != NULL);

  textbox->texture = SDL_CreateTextureFromSurface(render, surface);
  assert(textbox->texture != NULL);
  //SDL_SetTextureBlendMode(textbox->texture, SDL_BlendMode

  //textbox->width = surface->w;
  //textbox->height = surface->h;
  TTF_SizeText(global_font, text, &textbox->width, &textbox->height);
  printf("width: %d (of %d),  height: %d\n", textbox->width, textbox->width_max, textbox->height);

  SDL_FreeSurface(surface);

  // TODO how to do wrapping??
}


// SDL_SetTextureBlendMode();
// SDL_RenderCopyEx(render, text_texture, SDL_Rect* clip, SDL_Rect render_quad, angle, center, flip);
// SDL_RenderCopy(render, text_texture, SDL_Rect* src_rect, SDL_Rect* dst_rect);

int main(){
  // Platform Init Window
  SDL_Window* win;
  SDL_Renderer* render;
  sdl_startup(&win, &render);

  int win_width, win_height;
  int running = 1;

  char text_buffer[512];
  int text_buffer_length = 8;
  // initialize with a bunch of 'a's
  memset(text_buffer, 97, text_buffer_length);
  text_buffer[text_buffer_length] = '\0';

  TextBox editor_textbox;
  editor_textbox.color.r = 0; editor_textbox.color.g = 0; editor_textbox.color.b = 0; editor_textbox.color.a = 0xFF;
  editor_textbox.width_max = WINDOW_WIDTH / 4;

  SDL_StartTextInput();
  // causes some overhead. can control with SDL_StopTextInput()

  sdlj_textbox_render(render, &editor_textbox, text_buffer);
  while(running == 1){
    int render_text = 0;

    // INPUT
    SDL_Event evt;
    while (SDL_PollEvent(&evt) != 0){
      if (evt.type == SDL_QUIT){
        goto cleanup;
      }
      
      // special key input
      // TODO make use of keydown since sometimes otherwise two events are reported? 
//      else if (evt.type == SDL_KEYDOWN){
        // backspace
      if (evt.type == SDL_KEYDOWN){
        if (evt.key.keysym.sym == SDLK_BACKSPACE && text_buffer_length > 0){
          text_buffer[text_buffer_length-1] = '\0';
          --text_buffer_length;
          render_text = 1;
        }
        // handle copy?
        else if( evt.key.keysym.sym == SDLK_c && SDL_GetModState() & KMOD_CTRL){
          printf("copy!\n");
          ///SDL_SetClipboardText(text_buffer);
        }
        //handle paste
        else if( evt.key.keysym.sym == SDLK_v && SDL_GetModState() & KMOD_CTRL){
          // text_buffer = SDL_GetClipboardText();
          //render_text = 1;
          // TODO need to update length!
          printf("paste!\n");
        }

      }
        if( evt.type == SDL_TEXTINPUT){
          // TODO double check not copying or pasting??
          strcat(text_buffer, evt.text.text);
          ++text_buffer_length;
          render_text = 1;
          printf("typing...\n");
        }
        // TODO handle SDL_TEXTEDITING for a temporary buffer to allow for more complex languages/character sets 
 //    }
    } // end processing events

    // TODO do useful things with the input
    // TODO be able to use keyboard shortcuts!


    // DRAW
    SDL_GetWindowSize(win, &win_width, &win_height);

    // clear screen
    SDL_SetRenderDrawColor(render, 0xFF, 0xFF, 0xFF, 0xFF); // chooose every frame now
    SDL_RenderClear(render);


    // viewport for left half - editor
    SDL_Rect viewport_editor;
    viewport_editor.x = 0;
    viewport_editor.y = 0;
    viewport_editor.w = WINDOW_WIDTH / 4;
    viewport_editor.h = WINDOW_HEIGHT;
    SDL_RenderSetViewport(render, &viewport_editor);

    if (render_text == 1){
      if (text_buffer_length > 0){
        sdlj_textbox_render(render, &editor_textbox, text_buffer);
      }
      else{
        sdlj_textbox_render(render, &editor_textbox, " ");
      }
    }

    //gPromptTextTexture.render( (viewport_editor.w - gPromptTextTexture.getWidth())/2, 0);
    
    SDL_Rect src = {0, 0, editor_textbox.width, editor_textbox.height};
    SDL_Rect dst = {0, 0, editor_textbox.width, editor_textbox.height};
    assert(SDL_RenderCopy(render, editor_textbox.texture, &src, &dst) == 0);
//    gInputTextTexture.render( (viewport_editor.w - gInputTextTexture.getWidth())/2, gPromptTextTexture.getHeight());

    // SDL_RenderFillRect(render, &fill_rect);
    // SDL_RenderDrawRect(render, &outline);
    // SDL_RenderDrawLine(render, 0, WINDOW_HEIGHT / 2, WINDOW_WIDTH, WINDOW_HEIGHT / 2);
    // SDL_RenderDrawPoint(render, WINDOW_WIDTH / 2, i);

    // viewport for right half - display
    SDL_Rect viewport_display;
    viewport_display.x = viewport_editor.w;
    viewport_display.y = 0;
    viewport_display.w = WINDOW_WIDTH - viewport_editor.w;
    viewport_display.h = WINDOW_HEIGHT;
    SDL_RenderSetViewport(render, &viewport_display);

    SDL_Rect fill_white = {0, 0, viewport_display.w, viewport_display.h};
    SDL_SetRenderDrawColor(render, 0x80, 0x80, 0x80, 0xFF);
    SDL_RenderFillRect(render, &fill_white);

    // update screen
    SDL_RenderPresent(render);

  }


  // have two panels; left for text, right for display



  // ux can type in text in the text box



  // button to push to cause it to parse the jzon



  // draw some boxes somewhere in the project timeline

  // TODO track which timeline element(s) the user currently has selected
  

  cleanup:
  sdl_cleanup(win, render);

 return 0;
}
