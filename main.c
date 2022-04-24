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

#define FALSE 0
#define TRUE 1

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
uint8_t* task_editor_visited;

HashTable* task_names_ht;

void tasks_init(){
  tasks = (Task_Node*) malloc(task_allocation_total * sizeof(Task_Node));
  task_names_ht = hash_table_create(4096, HT_FREE_KEY); // TODO hard code because otherwise trigger frequent re-hash of the whole thing..

  for (size_t i=0; i<task_allocation_total; ++i){
    tasks[i].trash = TRUE;
  }
  task_editor_visited = (uint8_t*) malloc(task_allocation_total * sizeof(uint8_t));
  memset(task_editor_visited, 0, task_allocation_total);
  printf("Task init() complete for %ld tasks\n", task_allocation_total);

  status_color_init();
}


void tasks_free(){
  printf("[STATUS] FREEING TASK TABLE\n");
  hash_table_print(task_names_ht);
  hash_table_destroy(task_names_ht);
  free(tasks);
  free(task_editor_visited);
}


// grow memory as needed to hold allocated tasks
// don't shrink - avoid having to search and move active nodes into lower memory space in realtime
// if needed user could save and restart to reduce memory footprint
void task_memory_management(){
  if (task_allocation_used >= task_allocation_total){
    printf("[CAUTION] TASK MEMORY MANAGEMENT ACTIVATED, INCREASING MEMORY ALLOCATIONS\n");
    uint64_t task_allocation_old = task_allocation_total;
    task_allocation_total *= 1.5;
    tasks = (Task_Node*) realloc(tasks, task_allocation_total * sizeof(Task_Node));
    task_editor_visited = (uint8_t*) realloc(task_editor_visited, task_allocation_total * sizeof(uint8_t));

    for (size_t i=task_allocation_old; i<task_allocation_total; ++i){
      tasks[i].trash = TRUE;
      task_editor_visited[i] = FALSE;
    }
  }

  // TODO some way to update has task_names_ht size.. would need to re-index all tasks :(
}


Task_Node* task_create(char* task_name, size_t task_name_length){
  // find an empty slot to use for the task
  do {
    task_last_created = (task_last_created + 1) % task_allocation_total;
  } while (tasks[task_last_created].trash == FALSE);

  Task_Node* task = (tasks + task_last_created);

  // zero everything there. also brings mode out of trash mode
  memset((void*) task, 0, sizeof(Task_Node));
  task->trash = FALSE; // TODO just to be sure??
  
  // add to hash table
  char* name = (char*) malloc(task_name_length);
  memcpy(name, task_name, task_name_length);
  name[task_name_length] = '\0';
  hash_table_insert(task_names_ht, name, (void*) task);
  task->task_name = name;

  return task;
}


Task_Node* task_get(char* task_name, int task_name_length){
  // use the hash table to find a pointer to the task based on the string name the user gives
  // return NULL if the task does not exist and needs to be created
  char name[128]; // TODO
  memcpy(name, task_name, task_name_length);
  name[task_name_length] = '\0';
  Task_Node* task = (Task_Node*) hash_table_get(task_names_ht, name);
  return task;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// return NULL, result_length=0 
char* string_strip(int* result_length, char* str, int str_length){
  // look at the front, cut off bad characters
  char* result = str; 
  *result_length = str_length;
  while (isalnum(result[0]) == 0){
    ++result;
    *result_length -= 1;
    if (*result_length == 0){
      return NULL;
    }
  }

  // seek to the end, search backwards to cut off bad characters
  char* end = result + *result_length - 1;
  while (isalnum(end[0]) == 0){
    --end;
    *result_length -= 1;
    if (*result_length == 0){
      return NULL;
    }
  }

  return result;
}


void editor_parse_propertyline(Task_Node* task, char* line_start, int line_working_length){
  char* line_end = line_start + line_working_length;
  // split into property and value parts. split on ':'
  char* split = memchr(line_start, (int) ':', line_working_length);
  int property_str_length;
  char* property_str = string_strip(&property_str_length, line_start, split - line_start);
  int value_str_length;
  char* value_str = string_strip(&value_str_length, split, line_end - split);
  // TODO split values on ','

  printf("(task %s) add property='%.*s'  value='%.*s'\n", task->task_name, property_str_length, property_str, value_str_length, value_str);

  if (memcmp(property_str, "user", 4) == 0){
    printf("user property familiar!\n");
    // TODO, list comprehension
    // TODO pointer linking to user list? for relational database kinds of stuff at some other point? 
  }
  else if(memcmp(property_str, "dependent_on", 12) == 0){
    // TODO list comprehension
    // TODO linking with pointers.. hash lookup stuff
    // TODO update the quantity of dependents in the list also 

  }
  else if(memcmp(property_str, "duration", 8) == 0){
    int duration = atoi(value_str);
    task->day_duration = duration;
    task->schedule_constraints |= SCHEDULE_CONSTRAINT_DURATION;
  }
  else if(memcmp(property_str, "fixed_start", 11) == 0){
    // TODO date comprehension, storage
    task->schedule_constraints |= SCHEDULE_CONSTRAINT_START;
  }
  else if(memcmp(property_str, "color", 5) == 0){
    int color = atoi(value_str);
    if ((color > 9) || (color < 0)){
      color = 0;
    }
    task->status_color = color;
  }
  else{
    printf("[WARNING] PROPERTY %.*s NOT RECOGNIZED\n", property_str_length, property_str);
  }

}
 

// in edit mode, lock the Activity_Node ids that are being shown in the edit pane.
// TODO how does this function return information?
// TODO need to be compatible with replacing portions of the ground truth bigger text
// or... just modify the full network directly. and then from the full network export the full text

// automatically delete/re-add everything being edited in edit mode? assume all those nodes selected are trashed and revised. 
// how to balance reparsing everything at 100Hz and only reparsing what is needed? maybe use the cursor to direct efforts? only reparse from scratch the node the cursor is in
void editor_parse_text(char* text_start, size_t text_length){
  char* text_end = text_start + text_length;

  // assume starting at top level

  // track difference betweeen seen tasks and expected to see tasks
  // if you don't see tasks that you expect to.. need to remove those!
  //memset(task_editor_visited, 0, task_allocation_total);
  for (size_t i=0; i<task_allocation_total; ++i){
    task_editor_visited[i] = FALSE;
  }

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
      // figure out the name
      int task_name_length;
      char* task_name = string_strip(&task_name_length, line_start, line_working_length);
      printf("TASK, name '%.*s'\n", task_name_length, task_name);

      // now get a pointer to the task
      task = task_get(task_name, task_name_length);
      if (task == NULL){
        task = task_create(task_name, task_name_length); // TODO is create the right action? maybe parse and then decide? 
      }

      // mark task as visited
      task_editor_visited[task - tasks] = TRUE;
      task->mode |= TASK_MODE_EDIT; // TODO this is a hack since this should be set by the DISPLAY VIEWPORT
    }

    else if(memchr(line_start, (int) '}', line_working_length) != NULL){
      printf("line '%.*s' ends a task\n", line_working_length, line_start);
    }

    else if(memchr(line_start, (int) ':', line_working_length) != NULL){
      editor_parse_propertyline(task, line_start, line_working_length);

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

  // scrub through tasks, remove any that you expected to see but did not
  for (size_t i=0; i<task_allocation_total; ++i){
    if (tasks[i].trash == FALSE){ // if node is NOT trash
      if (task_editor_visited[i] == FALSE){ // if we did not visit the node this time parsing the text
        if ((tasks[i].mode & TASK_MODE_EDIT) > 0){ // TODO if this runs so much.. better to have a faster cache lookup?
          tasks[i].trash = TRUE; 
          hash_table_remove(task_names_ht, tasks[i].task_name);
        }
      }
    }
  }

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


#define DISPLAY_TASK_SELECTED (1)

void draw_box(SDL_Renderer* render, int x, int y, int flags, Task_Node* task){

  int border = 2;

  SDL_Rect rect;
  // upper left corner position
  rect.x = x; 
  rect.y = y;
  // width, height
  rect.w = 80;
  rect.h = 40;

  // draw outline if selected
  if ((flags & DISPLAY_TASK_SELECTED) > 0){
    SDL_Rect outline;
    outline.x = x-border;
    outline.y = y-border;
    outline.w = rect.w+2*border;
    outline.h = rect.h+2*border;

    SDL_SetRenderDrawColor(render, 200, 100, 0, 255); // orange
    SDL_RenderFillRect(render, &outline);
  }

  
  // draw the base box
  int sc = task->status_color;
  SDL_SetRenderDrawColor(render, status_colors[sc].r, status_colors[sc].g, status_colors[sc].b, status_colors[sc].a);
  //SDL_SetRenderDrawColor(render, 220, 220, 220, 255); // light grey? 
  SDL_RenderFillRect(render, &rect);
  
  // draw the text on top
  SDL_Color text_color = {
    .r = 0,
    .g = 0,
    .b = 0,
    .a = 0
  };

  SDL_Surface* surface = TTF_RenderText_Blended(global_font, task->task_name, text_color);
  if (surface == NULL){
    printf("text texture render surface error: %s\n", SDL_GetError());
  }
  assert(surface != NULL);

  SDL_Texture* texture = SDL_CreateTextureFromSurface(render, surface);
  assert(texture != NULL);

  SDL_Rect textbox;
  textbox.x = x + border;
  textbox.y = y + border;
  TTF_SizeText(global_font, task->task_name, &textbox.w, &textbox.h);

  SDL_Rect src = {0, 0, textbox.w, textbox.h};
  assert(SDL_RenderCopy(render, texture, &src, &textbox) == 0);
  
  // TODO find a different way to do this that doesn't involve so much memory alloc and dealloc!!!
  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
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
  editor_parse_text(text_buffer, text_buffer_length);


  // TODO difference between full/raw source and the buffer being shown? need to be able to turn Task_Nodes[] back into text
  // TODO cursor system
  // TODO smooth scroll system
  // TODO error flagging / colors system; live syntax parsing
  TextBox editor_textbox;
  editor_textbox.color.r = 0; editor_textbox.color.g = 0; editor_textbox.color.b = 0; editor_textbox.color.a = 0xFF;
  editor_textbox.width_max = WINDOW_WIDTH / 4;
  int editor_cursor_pos = text_buffer_length / 2;
  int editor_cursor_pos_x;
  int editor_cursor_pos_y;
  // TODO track length of each line in the text buffer, to enable jumps (home, end, up/down arrows...)

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
    int parse_text = 0;

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
            char* text_dst = text_buffer + editor_cursor_pos - 1;
            char* text_src = text_dst + 1;
            char* text_end = text_buffer + text_buffer_length;
            memmove(text_dst, text_src, text_end-text_src); 

            --text_buffer_length;
            text_buffer[text_buffer_length] = '\0';
            --editor_cursor_pos;
            render_text = 1;
            parse_text = 1;
          }
          // handle copy?
          else if( evt.key.keysym.sym == SDLK_c && SDL_GetModState() & KMOD_CTRL){
            printf("copy!\n");
            ///SDL_SetClipboardText(text_buffer);
            // TODO need to have a mark system for effective copying
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
            // move text to make space for inserting characters
            // TODO verify adding text at the end
            char* text_src = text_buffer + editor_cursor_pos;
            char* text_dst = text_src + 1;
            char* text_end = text_buffer + text_buffer_length;
            memmove(text_dst, text_src, text_end - text_src);

            // actually add the character
            text_buffer[editor_cursor_pos] = '\n';
            ++text_buffer_length;
            ++editor_cursor_pos;
            render_text = 1;
            parse_text = 1;

            printf("[INSERT] RETURN\n"); 
          }
          else if(evt.key.keysym.sym == SDLK_LEFT){
            if (editor_cursor_pos > 0){
              --editor_cursor_pos;
            }
          }
          else if(evt.key.keysym.sym == SDLK_RIGHT){
            if (editor_cursor_pos < text_buffer_length){
              ++editor_cursor_pos;
            }
          }

        }
        else if( evt.type == SDL_TEXTINPUT){
          assert(text_buffer_length < EDITOR_BUFFER_LENGTH);
          // TODO double check not copying or pasting??
          // TODO fix first new character.. it needs a 

          // move text to make space for inserting characters
          // TODO verify adding text at the end
          char* text_src = text_buffer + editor_cursor_pos;
          char* text_dst = text_src + 1;
          char* text_end = text_buffer + text_buffer_length;
          memmove(text_dst, text_src, text_end - text_src);

          // actually add the character
          text_buffer[editor_cursor_pos] = evt.text.text[0];
          ++text_buffer_length;
          ++editor_cursor_pos;
          render_text = 1;
          parse_text = 1;

          /*
          // auto insert close brackets
          if (evt.text.text[0] == '{'){
            text_buffer[text_buffer_length++] = '\n';
            text_buffer[text_buffer_length++] = '}';
          }
          */

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

    if (parse_text == 1){
      editor_parse_text(text_buffer, text_buffer_length);
    }

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
    
    // text rendering, and figure out where the cursor is

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

          // cursor drawing!
          if (viewport_active == VIEWPORT_EDITOR){
            if ((editor_cursor_pos >= line_start - text_buffer) && (editor_cursor_pos <= line_end - text_buffer)){
              // draw a shaded background
              SDL_Rect cursor_line_background = {
                .x = 0,
                .y = line_height_offset,
                .w = viewport_editor.w, 
                .h = 20
              }; // TODO need to have a font line height as independent variable!
              SDL_SetRenderDrawColor(render, 230, 230, 230, 255);
              SDL_RenderFillRect(render, &cursor_line_background);

              // find the location within the line?
              // TODO need to reference this to actual glyph widths!!
              editor_cursor_pos_x = editor_cursor_pos - (line_start - text_buffer);
              editor_cursor_pos_y = text_lines - 1; // -1 for zero indexing

              SDL_Rect cursor_draw = {
                .x = editor_cursor_pos_x * 7,
                .y = line_height_offset,
                .w = 3,
                .h = 20
              };
              SDL_SetRenderDrawColor(render, 50, 50, 80, 255);
              SDL_RenderFillRect(render, &cursor_draw);

            } 
          } // cursor drawing

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
          else{ // put gaps where lines are blank
            line_height_offset += editor_textbox.height; // TODO check validity?? can this variable be unset?
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
      else{ // empty render if no text is there
        sdlj_textbox_render(render, &editor_textbox, " ");
      }
    } // endif request re-render text


    //printf("cursor pos: %d (%d, %d)\n", editor_cursor_pos, editor_cursor_pos_x, editor_cursor_pos_y);

    
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

    // background color shows mode select
    SDL_Rect viewport_display_local = {0, 0, viewport_display.w, viewport_display.h};
    if (viewport_active == VIEWPORT_EDITOR){
      SDL_SetRenderDrawColor(render, 0x80, 0x80, 0x80, 0xFF);
    }
    else{
      SDL_SetRenderDrawColor(render, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    SDL_RenderFillRect(render, &viewport_display_local);



    //// DRAW TASK BOXES IN DISPLAY VIEWPORT
    int locx = 10;
    int locy = 10;

    for (size_t n=0; n<task_allocation_total; ++n){
      if (tasks[n].trash == 0){
        // TODO need to know the expected width to make sure it doesn't go offscreen? or don't care
        // TODO camera coordinate system; make layout somewhat independent of shown pixels
        // TODO an actual layout engine, show properties of the nodes and such
        draw_box(render, locx, locy, 0, tasks+n);
        locx = locx + 120;
        if (locx > viewport_display.w){
          locx = 10;
          locy += 100;
        }
      }
    }
        

    //// UPDATE SCREEN
    SDL_RenderPresent(render);

  } // while forever


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
