// Jay Jasper, 2022

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h> // ? 
#include <string.h>
#include <assert.h>
#include <limits.h> // ?
#include <time.h>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

// TODO defines to adjust NK functionality here
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GL3_IMPLEMENTATION

#include "nuklear.h"
#include "nuklear_sdl_gl3.h"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

#define SDL_MAX_VERTEX_MEMORY 512 * 1024
#define SDL_MAX_ELEMENT_MEMORY 128 * 1024

int main(){
  // Platform Init Window
  SDL_Window *win;
  SDL_GLContext glContext;
  int win_width, win_height;
  int running = 1;

  // Nuklear 
  struct nk_context *ctx;
  // struct nk_colorf bg;

  
  // SDL Setup
  SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);


  win = SDL_CreateWindow("Jay Demo", 
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
    WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
  glContext = SDL_GL_CreateContext(win);
  SDL_GetWindowSize(win, &win_width, &win_height);


  // OpenGL Setup
  glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  glewExperimental = 1;
  if (glewInit() != GLEW_OK){
    fprintf(stderr, "Failed to setup GLEW\n");
    exit(1);
  }

  ctx = nk_sdl_init(win);
  {struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    nk_sdl_font_stash_end();
  }


  char box_buffer[512];
  memset(box_buffer, 97, 512);
  int box_len;

  while(running){

    // INPUT
    SDL_Event evt;
    nk_input_begin(ctx);
    while (SDL_PollEvent(&evt)){
      if (evt.type == SDL_QUIT){
        goto cleanup;
      }
      nk_sdl_handle_event(&evt);
    }
    nk_input_end(ctx);

    // TODO do useful things with the input
    // TODO be able to use keyboard shortcuts!

    // GUI
    if (nk_begin(ctx, "task editor", nk_rect(0, 0, 250, 600), 
      NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE)){
      nk_layout_row_static(ctx, 300, 200, 1);
      nk_edit_string(ctx, NK_EDIT_BOX, box_buffer, &box_len, 512, nk_filter_default);
      //nk_label(ctx, "Text Input:", NK_TEXT_LEFT);
      }
    nk_end(ctx);

    // some other gui thing
    if (nk_begin(ctx, "schedule display", nk_rect(250, 0, 750, 600), 
      NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |NK_WINDOW_TITLE )){
      nk_label(ctx, "Window here", NK_TEXT_LEFT);
      //nk_layout_row_static(ctx, 300, 100, 1);
      }
    nk_end(ctx);

    // DRAW
    SDL_GetWindowSize(win, &win_width, &win_height);
    glViewport(0, 0, win_width, win_height);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.8, 0.2, 0.2, 1.0);
    nk_sdl_render(NK_ANTI_ALIASING_ON, SDL_MAX_VERTEX_MEMORY, SDL_MAX_ELEMENT_MEMORY);
    SDL_GL_SwapWindow(win);

  }


  // have two panels; left for text, right for display



  // ux can type in text in the text box



  // button to push to cause it to parse the jzon



  // draw some boxes somewhere in the project timeline

  // TODO track which timeline element(s) the user currently has selected
  

  cleanup:
  nk_sdl_shutdown();
  SDL_GL_DeleteContext(glContext);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
