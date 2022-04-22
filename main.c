// Jay Jasper, 2022

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <ctype.h> // for isalnum()

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "schedule.h"
#include "utilities-c/hash_lib/hashtable.h"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define FONTSIZE 16

#define VIEWPORT_EDITOR 0
#define VIEWPORT_DISPLAY 1
#define LINE_MAX_LENGTH 512

TTF_Font* global_font = NULL;

#define EDITOR_BUFFER_LENGTH 1024

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// the main table of all tasks
Task_Node* tasks;
uint64_t task_allocation_used = 0;
uint64_t task_allocation_total = 128;
uint64_t task_last_created = 0;

HashTable* task_names_ht;

void tasks_init(){
  tasks = (Task_Node*) malloc(task_allocation_total * sizeof(Task_Node));
  task_names_ht = hash_table_create(4096); // TODO hard code because otherwise trigger frequent re-hash of the whole thing..

  for (size_t i=0; i<task_allocation_total; ++i){
    tasks[i].mode = TASK_MODE_TRASH;
  }
  printf("Task init() complete for %ld tasks\n", task_allocation_total);
}


void tasks_free(){
  printf("[STATUS] FREEING TASK TABLE\n");
  hash_table_print(task_names_ht);
  hash_table_destroy(task_names_ht, HT_KEY);
  free(tasks);
}


// grow memory as needed to hold allocated tasks
// don't shrink - avoid having to search and move active nodes into lower memory space in realtime
// if needed user could save and restart to reduce memory footprint
void task_memory_management(){
  if (task_allocation_used >= task_allocation_total){
    uint64_t task_allocation_old = task_allocation_total;
    task_allocation_total *= 1.5;
    tasks = (Task_Node*) realloc(tasks, task_allocation_total * sizeof(Task_Node));

    for (size_t i=task_allocation_old; i<task_allocation_total; ++i){
      tasks[i].mode = TASK_MODE_TRASH;
    }
  }

  // TODO some way to update has task_names_ht size.. would need to re-index all tasks :(
}


Task_Node* task_create(char* task_name, size_t task_name_length){
  // find an empty slot to use for the task
  do {
    task_last_created = (task_last_created + 1) % task_allocation_total;
  } while ((tasks[task_last_created].mode & TASK_MODE_TRASH) == 0);

  Task_Node* task = (tasks + task_last_created);

  // zero everything there
  memset((void*) task, 0, sizeof(Task_Node));
  task->mode |= TASK_MODE_ACTIVE;
  
  // add to hash table
  char* name = (char*) malloc(task_name_length);
  memcpy(name, task_name, task_name_length);
  name[task_name_length] = '\0';
  hash_table_insert(task_names_ht, name, (void*) task);
  task->task_name = name;

  return task;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* string_strip(int* result_length, char* str, int str_length){
  // look at the front, cut off bad characters
  char* result = str; 
  *result_length = str_length;
  while (isalnum(result[0]) == 0){
    ++result;
    *result_length -= 1;
    assert(*result_length > 0);
  }

  // seek to the end, search backwards to cut off bad characters
  char* end = result + *result_length - 1;
  while (isalnum(end[0]) == 0){
    --end;
    *result_length -= 1;
    assert(*result_length > 0);
  }

  return result;
}

// in edit mode, lock the Activity_Node ids that are being shown in the edit pane.
// TODO how does this function return information?
// TODO need to be compatible with replacing portions of the ground truth bigger text
// or... just modify the full network directly. and then from the full network export the full text

// automatically delete/re-add everything being edited in edit mode? assume all those nodes selected are trashed and revised. 
// how to balance reparsing everything at 100Hz and only reparsing what is needed? maybe use the cursor to direct efforts? only reparse from scratch the node the cursor is in
void node_from_text(char* text_start, size_t text_length){
  char* text_end = text_start + text_length;

  // assume starting at top level

  // read one line at a time
  char* line_start = text_start;
  char* line_end;
  int line_working_length = 0;
  Task_Node* task;
  while (line_start < text_end){
    line_end = memchr(line_start, (int) '\n', text_end - line_start);
    if (line_end == NULL){
      break;
    }
    line_working_length = line_end - line_start;
    if (line_working_length == 0){
      ++line_start;
      continue;
    }

    if (memchr(line_start, (int) '{', line_working_length) != NULL){
      // name is text with exterior spaces, brackets stripped out
      // TODO check, prevent duplicate task names
      int task_name_length;
      char* task_name = string_strip(&task_name_length, line_start, line_working_length);
      printf("NEW TASK, name '%.*s'\n", task_name_length, task_name);
      task = task_create(task_name, task_name_length); // TODO is create the right action? maybe parse and then decide? 
    }
    else if(memchr(line_start, (int) '}', line_working_length) != NULL){
      printf("line '%.*s' ends a task\n", line_working_length, line_start);
    }
    else if(memchr(line_start, (int) ':', line_working_length) != NULL){
      // split into property and value parts. split on ':'
      char* split = memchr(line_start, (int) ':', line_working_length);
      int property_str_length;
      char* property_str = string_strip(&property_str_length, line_start, split - line_start);
      int value_str_length;
      char* value_str = string_strip(&value_str_length, split, line_end - split);
      // TODO split values on ','

      printf("(task %s) add property='%.*s'  value='%.*s'\n", task->task_name, property_str_length, property_str, value_str_length, value_str);

      // parse.. how to connect parts of struct with strings? 
      // if strcmp(property_str, "dependent_on") == .... etc. a big huge list of conditionals
      // then with in each can have custom logic for parsing. lists, dates parsing, string vs numeric values

      // TODO need to track what the cursor is currently editing? 
    }

    // advance to the next line
    line_start = line_end + 1;
  }
  // text at the top level creates activities with that name
  // open bracket increases to the next level
  // text at the next level causes a lookup for a struct member. colon separator
  // then input type specific. comma to separate list items.

  // TODO add better error handling warning stuff
}


void node_to_text(){
  // flag to do optioanl stuff? 
  // how to denote cursor? 

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


typedef struct TextBox{
  SDL_Texture* texture;
  SDL_Color color;
  int width;
  int height;
  int width_max;
} TextBox;


void sdlj_textbox_render(SDL_Renderer* render, TextBox* textbox, char* text){
  //printf("rendering %s...\n", text);
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
  // printf("width: %d (of %d),  height: %d\n", textbox->width, textbox->width_max, textbox->height);

  SDL_FreeSurface(surface);

  // TODO how to do wrapping??
}


// SDL_SetTextureBlendMode();
// SDL_RenderCopyEx(render, text_texture, SDL_Rect* clip, SDL_Rect render_quad, angle, center, flip);
// SDL_RenderCopy(render, text_texture, SDL_Rect* src_rect, SDL_Rect* dst_rect);


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int main(){
  // Platform Init Window
  SDL_Window* win;
  SDL_Renderer* render;
  sdl_startup(&win, &render);

  int win_width, win_height;
  int running = 1;
  int viewport_active = VIEWPORT_EDITOR;

  tasks_init();

  char text_buffer[EDITOR_BUFFER_LENGTH];
  int text_buffer_length = -1;
  // TODO temporary just import a demo project file
  FILE* fd = fopen("examples/demo1.json", "r");
  assert(fd != NULL);
  char* text_cursor = text_buffer;
  do {
    *text_cursor = fgetc(fd);
    ++text_cursor;
    ++text_buffer_length;
  } while(*(text_cursor - 1) != EOF);
  fclose(fd);
  text_buffer[text_buffer_length] = '\0';
  printf("loaded text of length %d\n", text_buffer_length);

  // do a test parse of the text description
  node_from_text(text_buffer, text_buffer_length);


  // TODO difference between full/raw source and the buffer being shown? need to be able to turn Task_Nodes[] back into text
  // TODO cursor system
  // TODO smooth scroll system
  // TODO error flagging / colors system; live syntax parsing
  TextBox editor_textbox;
  editor_textbox.color.r = 0; editor_textbox.color.g = 0; editor_textbox.color.b = 0; editor_textbox.color.a = 0xFF;
  editor_textbox.width_max = WINDOW_WIDTH / 4;

  // causes some overhead. can control with SDL_StopTextInput()
  SDL_StartTextInput();


  // TODO display viewport
  // layout of nodes
  // navigation among nodes, cursor system, selection system
  
  uint32_t timer_last_loop_start_ms = SDL_GetTicks();
  uint32_t timer_target_ms = 10;
  uint32_t timer_last_loop_duration_ms;

  printf("starting main loop!\n");

  while(running == 1){
    // rate control, delay as much as needed
    timer_last_loop_duration_ms = SDL_GetTicks() - timer_last_loop_start_ms;
    if (timer_last_loop_duration_ms < timer_target_ms){
      SDL_Delay(timer_target_ms - (timer_last_loop_duration_ms));
    }
    timer_last_loop_start_ms = SDL_GetTicks();

    int render_text = 0;

    // INPUT
    SDL_Event evt;
    while (SDL_PollEvent(&evt) != 0){
      if (evt.type == SDL_QUIT){
        goto cleanup;
      }
      // TODO modal switching!
      if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE){
        goto cleanup;
      }
      
      if (viewport_active == VIEWPORT_EDITOR){
        
        // special key input
        // TODO cursor management, underline corner style
        if (evt.type == SDL_KEYDOWN){
          // backspace
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
          else if (evt.key.keysym.sym == SDLK_e && SDL_GetModState() & KMOD_CTRL){
            viewport_active = VIEWPORT_DISPLAY;
            printf("switch to display viewport\n");
            SDL_StopTextInput();
          }
          else if (evt.key.keysym.sym == SDLK_RETURN){
            text_buffer[text_buffer_length] = '\n';
            ++text_buffer_length;
            render_text = 1;
          }

        }
        else if( evt.type == SDL_TEXTINPUT){
          assert(text_buffer_length < EDITOR_BUFFER_LENGTH);
          // TODO double check not copying or pasting??
          // TODO fix first new character.. it needs a 
          text_buffer[text_buffer_length] = evt.text.text[0];
          ++text_buffer_length;
          render_text = 1;

          // auto insert close brackets
          if (evt.text.text[0] == '{'){
            text_buffer[text_buffer_length] = '}';
            ++text_buffer_length;
          }

        }
        else if(evt.type == SDL_TEXTEDITING){
          // TODO handle SDL_TEXTEDITING for a temporary buffer to allow for more complex languages/character sets 
          assert(0);
        }

      } // viewport editor

      else if (viewport_active == VIEWPORT_DISPLAY){
        if (evt.key.keysym.sym == SDLK_e && evt.type == SDL_KEYDOWN){
          viewport_active = VIEWPORT_EDITOR;
          printf("switch to viewport editor\n");
          SDL_StartTextInput();
        }
        // TODO navigate around the displayed nodes

         
      } // viewport display

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
    if (viewport_active == VIEWPORT_EDITOR){
      SDL_SetRenderDrawColor(render, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    else{
      SDL_SetRenderDrawColor(render, 0xE0, 0xE0, 0xE0, 0xFF);
    }
    SDL_RenderFillRect(render, &viewport_editor);
    
    // text rendering

    if (1 == 1){ // TODO if (render_text == 1)
      if (text_buffer_length > 0){
        // figure out how many lines there are to render
        int text_lines = 1;
        char* line_start = text_buffer;
        char* line_end = NULL; 
        char* text_buffer_end = text_buffer + text_buffer_length; 
        char line[LINE_MAX_LENGTH];
        int line_height_offset = 0;

        while (1){
          line_end = memchr(line_start, (int) '\n', text_buffer_end - line_start);
          if (line_end == NULL){
            line_end = text_buffer_end;
          }

          // prepare the line to be rendered
          int line_length = line_end - line_start;
          assert(line_length < LINE_MAX_LENGTH);
          memcpy(line, line_start, line_length);
          line[line_length] = '\0';

          // don't try to render if it is a blank line. TODO make sure the height gets more offset
          if (line_start != line_end){
            
            // need to just make a big texture, since this has to be SDL_RenderCopy() every frame TODO
            // https://stackoverflow.com/questions/40886350/how-to-connect-multiple-textures-in-the-one-in-sdl2
            // https://gamedev.stackexchange.com/questions/46238/rendering-multiline-text-with-sdl-ttf

            // render the line!
            sdlj_textbox_render(render, &editor_textbox, line);
            SDL_Rect src = {0, 0, editor_textbox.width, editor_textbox.height};
            SDL_Rect dst = {0, line_height_offset, editor_textbox.width, editor_textbox.height};
            assert(SDL_RenderCopy(render, editor_textbox.texture, &src, &dst) == 0);
            line_height_offset += editor_textbox.height;
          }

          // advance to the next line
          line_start = line_end + 1;
          if (line_start >= text_buffer_end){
            break;
          }

          ++text_lines;
          assert(text_lines < 100);
        }

        //sdlj_textbox_render(render, &editor_textbox, text_buffer);
      }
      else{
        sdlj_textbox_render(render, &editor_textbox, " ");
      }
    }
    
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

    SDL_Rect viewport_display_local = {0, 0, viewport_display.w, viewport_display.h};
    if (viewport_active == VIEWPORT_EDITOR){
      SDL_SetRenderDrawColor(render, 0x80, 0x80, 0x80, 0xFF);
    }
    else{
      SDL_SetRenderDrawColor(render, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    SDL_RenderFillRect(render, &viewport_display_local);

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
  tasks_free();

 return 0;
}
