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

// global
#define FALSE 0
#define TRUE 1

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define FONTSIZE 16

// modal switching
#define VIEWPORT_EDITOR 0
#define VIEWPORT_DISPLAY 1

// viewport-editor related
#define LINE_MAX_LENGTH 512
#define EDITOR_BUFFER_LENGTH 1024
#define EDITOR_LINES_MAX 1024
TTF_Font* global_font = NULL;

// viewport-display related
#define DISPLAY_TASK_SELECTED (1)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// the main table of all tasks
Task_Node* tasks;
size_t task_allocation_used = 0;
size_t task_allocation_total = 128;
size_t task_last_created = 0;
uint8_t* task_editor_visited;

User* users;
size_t user_allocation_used = 0;
size_t user_allocation_total = 16;
size_t user_last_created = 0;
uint8_t* user_editor_visited;

HashTable* task_names_ht;
HashTable* users_ht;

void tasks_init(){
  tasks = (Task_Node*) malloc(task_allocation_total * sizeof(Task_Node));
  task_names_ht = hash_table_create(8192, HT_FREE_KEY); // TODO hard code because otherwise trigger frequent re-hash of the whole thing..

  for (size_t i=0; i<task_allocation_total; ++i){
    tasks[i].trash = TRUE;
  }
  task_editor_visited = (uint8_t*) malloc(task_allocation_total * sizeof(uint8_t));
  memset(task_editor_visited, 0, task_allocation_total);
  printf("Task init() complete for %ld tasks\n", task_allocation_total);

  status_color_init();

  users = (User*) malloc(user_allocation_total * sizeof(User));
  for (size_t i=0; i<user_allocation_total; ++i){
    users[i].trash = TRUE;
  }
  users_ht = hash_table_create(1024, HT_FREE_KEY);
  user_editor_visited = (uint8_t*) malloc(user_allocation_total * sizeof(uint8_t));
  memset(user_editor_visited, 0, user_allocation_total);
}


void tasks_free(){
  printf("[STATUS] FREEING TASK TABLE\n");
  hash_table_print(task_names_ht);
  hash_table_destroy(task_names_ht);
  hash_table_print(users_ht);
  hash_table_destroy(users_ht); 
  free(tasks);
  free(task_editor_visited);
  free(user_editor_visited);
  free(users);
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
  ++task_allocation_used;

  Task_Node* task = (tasks + task_last_created);

  // zero everything there. also brings mode out of trash mode
  memset((void*) task, 0, sizeof(Task_Node));
  task->trash = FALSE;
  
  // add to hash table
  char* name = (char*) malloc(task_name_length+1);
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
  assert(task_name_length < 128);
  memcpy(name, task_name, task_name_length);
  name[task_name_length] = '\0';
  Task_Node* task = (Task_Node*) hash_table_get(task_names_ht, name);
  return task;
}


// check if the given user is already assigned to the given task
uint8_t task_user_has(Task_Node* task, User* user){
  uint8_t result = FALSE;
  for(size_t i=0; i<task->user_qty; ++i){
    if (task->users[i] == user){
      result = TRUE;
    }
  }
  return result;
}


// add user to task if user is not already there
void task_user_add(Task_Node* task, User* user){
  assert(task->user_qty < TASK_USERS_MAX);
  assert(user->task_qty < USER_TASKS_MAX);

  if (task_user_has(task, user) == FALSE){
    task->users[task->user_qty] = user;
    task->user_qty += 1;

    user->tasks[user->task_qty] = task;
    user->task_qty += 1;
  }
}


void task_user_remove(Task_Node* task, User* user){
  size_t id = 0;
  for(size_t i=0; i<task->user_qty; ++i){
    if (task->users[i] == user){
      id = i;
      break;
    }
  }
  printf("need to remove user %s from task %s\n", user->name, task->task_name);
  
  // now shuffle down all the remaining users, update the qty
  for (size_t i=id; i<task->user_qty-1; ++i){
    task->users[i] = task->users[i+1];
  }
  task->user_qty -= 1;

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void user_memory_management(){
  if (user_allocation_used >= user_allocation_total){
    printf("[CAUTION] USER MEMORY MANAGEMENT ACTIVATED, INCREASING MEMORY ALLOCATIONS\n");
    uint64_t user_allocation_old = user_allocation_total;
    user_allocation_total *= 1.5;
    users = (User*) realloc(users, user_allocation_total * sizeof(User));

    for (size_t i=user_allocation_old; i<user_allocation_total; ++i){
      users[i].trash = TRUE;
      user_editor_visited[i] = FALSE;
    }
  }
}


User* user_create(char* user_name, size_t name_length){
  user_memory_management();
  // find an empty user slot to use
  do {
    user_last_created = (user_last_created + 1) % user_allocation_total;
  } while(users[user_last_created].trash == FALSE);
  User* user = users + user_last_created;
  ++user_allocation_used;

  // zero everything there
  memset((void*) user, 0, sizeof(User));
  user->trash = FALSE; 

  char* name = (char*) malloc(name_length+1);
  memcpy(name, user_name, name_length);
  name[name_length] = '\0';
  hash_table_insert(users_ht, name, (void*) user);
  user->name = name;

  return user;
}


User* user_get(char* user_name, int user_name_length){
  // use the hash table to find a pointer to the user based on the string given
  // return NULL if does not exist and needs to be created
  char name[128]; // TODO
  memcpy(name, user_name, user_name_length);
  name[user_name_length] = '\0';
  User* user = (User*) hash_table_get(users_ht, name);
  return user;
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
  if (value_str_length == 0){
    return;
  }

  if (memcmp(property_str, "user", 4) == 0){
    printf("user property familiar!\n");
    // TODO, list comprehension
    // TODO pointer linking to user list? for relational database kinds of stuff at some other point? 

    // split on ','
    char* property_split_start = value_str;
    char* property_split_end = value_str;

    while(property_split_start < line_end){
      property_split_end = memchr(property_split_start, (int) ',', line_end - property_split_start);
      if (property_split_end == NULL){
        property_split_end = line_end;
      }

      // parse what you find
      int value_length;
      char* value = string_strip(&value_length, property_split_start, property_split_end - property_split_start);
      if (value_length > 0){
        User* user = user_get(value, value_length);
        if (user == NULL){
          printf("user: '%.*s' NEW!\n", value_length, value);
          user = user_create(value, value_length); // TODO actually create this user
          // track which line the cursor is on? interact with those values differently?
          // periodic cleanup function? index matters, rely on the structure of the text, there can't be extra users!
          // mark ones in editor as untrustworthy, check they still exist? 
          // does this problem also apply to activity names?
        }
        else{
          printf("user: '%.*s'\n", value_length, value);
        }
        user->trash = FALSE;
        user->mode_edit = TRUE;
        user_editor_visited[user - users] = TRUE;

        // assign to the task, if it is not already there
        // TODO later need to scrub to remove users!!
        task_user_add(task, user);
      }

      property_split_start = property_split_end + 1;
    }


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
  for (size_t i=0; i<user_allocation_total; ++i){
    user_editor_visited[i] = FALSE;
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
        printf("created task. allocations: %ld of %ld\n", task_allocation_used, task_allocation_total);
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
          if (task_allocation_used > 0){
            --task_allocation_used;
          }
          printf("REMOVING tasks[%ld].name=%s..\n", i, tasks[i].task_name);
          hash_table_remove(task_names_ht, tasks[i].task_name);
        }
      }
    }
  }
  hash_table_print(task_names_ht);
  printf("[STATUS] Finished parsing text this round\n");

  // scrub through users, remove any that you expected to see but did not
  for (size_t i=0; i<user_allocation_total; ++i){
    if(users[i].trash == FALSE){
      if (user_editor_visited[i] == FALSE){

        if (users[i].mode_edit == TRUE){
          // this user was expected to be seen upon parsing but was not; delete them!!
          users[i].trash = TRUE;
          if (user_allocation_used > 0){
            --user_allocation_used;
          }
          printf("REMOVING users[%ld].name=%s..\n", i, users[i].name);

          // TODO need to remove references to those users
          for(size_t j=0; j<users[i].task_qty; ++j){
            task_user_remove(users[i].tasks[j], users+i);
          }

          hash_table_remove(users_ht, users[i].name);
        }
      }
    }
  }


  // TODO add better error handling warning stuff
}


void node_to_text(){
  // flag to do optioanl stuff? 
  // how to denote cursor? 

  // for each ndoe
  // if node selected for edit..
  // go through properties, write stuff

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
  global_font = TTF_OpenFont("Hack_Font.ttf", FONTSIZE); // Ubuntu-R
  assert(global_font != NULL);
}


void sdl_cleanup(SDL_Window* win, SDL_Renderer* render){
  SDL_DestroyRenderer(render);
  SDL_DestroyWindow(win);
  SDL_Quit();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void draw_box(SDL_Renderer* render, int x, int y, int flags, Task_Node* task){

  int border = 2;

  SDL_Rect rect;
  // width, height
  rect.w = 180;
  rect.h = 40;
  // upper left corner position
  rect.x = x - rect.w/2; 
  rect.y = y;

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

  // for performance don't want to do this every frame https://forums.libsdl.org/viewtopic.php?t=10303
  SDL_Texture* texture = SDL_CreateTextureFromSurface(render, surface);
  assert(texture != NULL);

  SDL_Rect textbox;
  textbox.x = x + border - rect.w/2;
  textbox.y = y + border;
  TTF_SizeText(global_font, task->task_name, &textbox.w, &textbox.h);

  SDL_Rect src = {0, 0, textbox.w, textbox.h};
  assert(SDL_RenderCopy(render, texture, &src, &textbox) == 0);
  
  // TODO find a different way to do this that doesn't involve so much memory alloc and dealloc!!!
  SDL_DestroyTexture(texture);
  SDL_FreeSurface(surface);
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
  // TODO this has a crashing problem... seems to show up if I rapidly edit a single line? or edit over and over and over?
  // can crash even on a line very different than the long one? but seems to most often show if there is A line long line somewhere
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
  if (textbox->texture == NULL){
    printf("text surface->texture creation erro: %s\n", SDL_GetError());
  }
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

typedef struct TextBuffer{
  char* text;
  int length;
  int* line_length; // [pointer to lineA start] + [line_length A] = [pointer to lineB start]
  int lines;
} TextBuffer;

typedef struct TextCursor{
  int pos;
  int x;
  int y;
} TextCursor;

enum TEXTCURSOR_MOVE_DIR {
  TEXTCURSOR_MOVE_DIR_RIGHT,
  TEXTCURSOR_MOVE_DIR_LEFT,
  TEXTCURSOR_MOVE_DIR_UP,
  TEXTCURSOR_MOVE_DIR_DOWN
};


TextBuffer* editor_buffer_init(){
  TextBuffer* tb = (TextBuffer*) malloc(sizeof(TextBuffer));

  tb->text = (char*) malloc(EDITOR_BUFFER_LENGTH * sizeof(char));
  tb->length = 0;

  tb->line_length = (int*) malloc(EDITOR_LINES_MAX * sizeof(*tb->line_length));
  tb->lines = 0;
  for (size_t i=0; i<EDITOR_LINES_MAX; ++i){
    tb->line_length[i] = 0;
  }
  return tb;
}

void editor_bufffer_destroy(TextBuffer* tb){
  free(tb->text);
  free(tb->line_length);
}


void editor_find_line_lengths(TextBuffer* tb){
  char* line_start = tb->text;
  char* line_end = NULL; 
  char* text_buffer_end = tb->text + tb->length;

  tb->lines = 0;
  while (tb->lines < EDITOR_LINES_MAX){
    line_end = memchr(line_start, (int) '\n', text_buffer_end - line_start);
    if (line_end == NULL){
      line_end = text_buffer_end;
    }   
    else{
      line_end += 1; // move past the \n
    }

    tb->line_length[tb->lines] = line_end - line_start;

    tb->lines += 1;
    line_start = line_end;
    if (line_end == text_buffer_end){
      break;
    }
  }

  for(int i=tb->lines; i<EDITOR_LINES_MAX; ++i){
    tb->line_length[i] = 0;
  }
}


void editor_cursor_move(TextBuffer* tb, TextCursor* tc, int movedir){
  if (movedir == TEXTCURSOR_MOVE_DIR_RIGHT){
    if (tc->pos < tb->length){
      // TODO if allowed by length
      tc->pos += 1;
      tc->x += 1;
      
      // wrap to next line
      if (tc->x == tb->line_length[tc->y]){
        tc->x = 0;
        tc->y += 1;
      }
    }
  }
  else if (movedir == TEXTCURSOR_MOVE_DIR_LEFT){
    if (tc->pos > 0){
      tc->pos -= 1;
      tc->x -= 1;

      // wrap to previous line
      if(tc->x  < 0){
        tc->y -= 1;
        tc->x = tb->line_length[tc->y] - 1;
      }
    }
  }
  else if (movedir == TEXTCURSOR_MOVE_DIR_UP){
    if (tc->y > 0){
      tc->y -= 1;

      int x_delta = tc->x;
      // moving on to a shorter line
      if (tc->x > tb->line_length[tc->y]){
        tc->x = tb->line_length[tc->y]-1;
        tc->pos -= x_delta + 1;
      }
      // moving onto a longer line
      else{
        x_delta += tb->line_length[tc->y] - tc->x;
        tc->pos -= x_delta;
      }
    }
  }
  else if (movedir == TEXTCURSOR_MOVE_DIR_DOWN){
    if (tc->y < tb->lines){
      int x_delta = tb->line_length[tc->y] - tc->x;
      tc->y += 1;

      // moving onto a shorter line
      if (tc->x > tb->line_length[tc->y]){
        tc->x = tb->line_length[tc->y]-1;
      }
        
      tc->pos += x_delta + tc->x;
    }
  }

  printf("move(%d): %d = (%d, %d)\n", movedir, tc->pos, tc->x, tc->y);

}

void editor_load_text(TextBuffer* text_buffer, const char* filename){

  // TODO temporary just import a demo project file
  // FILE* fd = fopen("examples/demo1.json", "r");
  FILE* fd = fopen(filename, "r");
  assert(fd != NULL);
  char* text_cursor_loading = text_buffer->text;
  do {
    *text_cursor_loading = fgetc(fd);
    ++text_cursor_loading;
    text_buffer->length += 1;
  } while(*(text_cursor_loading - 1) != EOF);
  fclose(fd);
  text_buffer->length -= 1;
  //text_buffer->text[text_buffer->length] = '\0';
  printf("loaded text of length %d\n", text_buffer->length);
  printf("text is '%.*s'\n", text_buffer->length, text_buffer->text);

  // do a test parse of the text description
  editor_parse_text(text_buffer->text, text_buffer->length);
  editor_find_line_lengths(text_buffer);
}


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

  TextBuffer* text_buffer = editor_buffer_init();
  editor_load_text(text_buffer, "examples/demo1.json");


  // TODO difference between full/raw source and the buffer being shown? need to be able to turn Task_Nodes[] back into text
  // TODO smooth scroll system
  // TODO error flagging / colors system; live syntax parsing
  TextBox editor_textbox;
  editor_textbox.color.r = 0; editor_textbox.color.g = 0; editor_textbox.color.b = 0; editor_textbox.color.a = 0xFF;
  editor_textbox.width_max = WINDOW_WIDTH / 4;
  editor_textbox.texture = NULL;
  TextCursor* text_cursor = (TextCursor*) malloc(sizeof(TextCursor));
  text_cursor->pos = 0;
  text_cursor->x  = 0;
  text_cursor->y = 0;

  TextBox name_textbox;
  name_textbox.color.r = 0; name_textbox.color.g = 0; name_textbox.color.b = 0; name_textbox.color.a = 0xFF;
  name_textbox.width_max = WINDOW_WIDTH * 0.75;
  name_textbox.texture = NULL;

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
          if (evt.key.keysym.sym == SDLK_BACKSPACE && text_buffer->length > 0){
            char* text_dst = text_buffer->text + text_cursor->pos - 1;
            char* text_src = text_dst + 1;
            char* text_end = text_buffer->text + text_buffer->length;
            memmove(text_dst, text_src, text_end-text_src); 

            --text_buffer->length;
            text_buffer->text[text_buffer->length] = '\0';
            text_buffer->line_length[text_cursor->y] -= 1; // TODO what if line length is already zero??
            editor_cursor_move(text_buffer, text_cursor, TEXTCURSOR_MOVE_DIR_LEFT);
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
            char* text_src = text_buffer->text + text_cursor->pos;
            char* text_dst = text_src + 1;
            char* text_end = text_buffer->text + text_buffer->length;
            memmove(text_dst, text_src, text_end - text_src);

            // actually add the character
            text_buffer->text[text_cursor->pos] = '\n';
            ++text_buffer->length;
            editor_cursor_move(text_buffer, text_cursor, TEXTCURSOR_MOVE_DIR_RIGHT);
            render_text = 1;
            parse_text = 1;

            printf("[INSERT] RETURN\n"); 
          }
          else if(evt.key.keysym.sym == SDLK_LEFT){
            editor_cursor_move(text_buffer, text_cursor, TEXTCURSOR_MOVE_DIR_LEFT);
          }
          else if(evt.key.keysym.sym == SDLK_RIGHT){
            editor_cursor_move(text_buffer, text_cursor, TEXTCURSOR_MOVE_DIR_RIGHT);
          }
          else if (evt.key.keysym.sym == SDLK_UP){
            editor_cursor_move(text_buffer, text_cursor, TEXTCURSOR_MOVE_DIR_UP);
          }
          else if (evt.key.keysym.sym == SDLK_DOWN){
            editor_cursor_move(text_buffer, text_cursor, TEXTCURSOR_MOVE_DIR_DOWN);
          }

        } // keypress
        else if( evt.type == SDL_TEXTINPUT){
          assert(text_buffer->length < EDITOR_BUFFER_LENGTH);
          // TODO double check not copying or pasting??
          // TODO fix first new character.. it needs a 

          // move text to make space for inserting characters
          // TODO verify adding text at the end
          char* text_src = text_buffer->text + text_cursor->pos;
          char* text_dst = text_src + 1;
          char* text_end = text_buffer->text + text_buffer->length;
          memmove(text_dst, text_src, text_end - text_src);

          // actually add the character
          text_buffer->text[text_cursor->pos] = evt.text.text[0];
          ++text_buffer->length;
          text_buffer->line_length[text_cursor->y] += 1;
          editor_cursor_move(text_buffer, text_cursor, TEXTCURSOR_MOVE_DIR_RIGHT);
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

    // TODO be able to use keyboard shortcuts!

    if (parse_text == 1){
      // extract property changes from the text
      editor_parse_text(text_buffer->text, text_buffer->length);

      // PERFORM SCHEDULING! TODO can get fancy....


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
      if (text_buffer->length > 0){
        // figure out how many lines there are to render
        editor_find_line_lengths(text_buffer);

        char* line_start = text_buffer->text;
        char* line_end = NULL; 
        char* text_buffer_end = text_buffer->text + text_buffer->length;
        char line[LINE_MAX_LENGTH];
        int line_height_offset = 0;

        for(int line_number=0; line_number<text_buffer->lines; ++line_number){
          line_end = line_start + text_buffer->line_length[line_number];
          // assert(line_end != line_start);

          // prepare the line to be rendered
          assert(text_buffer->line_length[line_number] < LINE_MAX_LENGTH);
          memcpy(line, line_start, text_buffer->line_length[line_number]);
          line[text_buffer->line_length[line_number]-1] = '\0';

          // cursor drawing!
          if (viewport_active == VIEWPORT_EDITOR){
            if ((text_cursor->pos >= line_start - text_buffer->text) && (text_cursor->pos < line_end - text_buffer->text)){
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
              // editor_cursor_pos_x = text_cursor->pos - (line_start - text_buffer->text);
              // editor_cursor_pos_y = text_lines - 1; // -1 for zero indexing

              SDL_Rect cursor_draw = {
                .x = text_cursor->x * 10,
                .y = line_height_offset,
                .w = 3,
                .h = 20
              };
              SDL_SetRenderDrawColor(render, 50, 50, 80, 255);
              SDL_RenderFillRect(render, &cursor_draw);

            } 
          } // cursor drawing

          // don't try to render if it is a blank line. TODO make sure the height gets more offset
          if (text_buffer->line_length[line_number] > 1){
            
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
          line_start = line_end;
          if (line_start >= text_buffer_end){
            break;
          }

        } // (close) while going through all lines

      } // (close) if there is ANY text to display
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


    //// DRAW USER NAMES
    // TODO what is the right way to later connect user name to a column location (eventually, in pixels)

    int user_column_increment = viewport_display.w / (user_allocation_used);
    int user_column_loc = user_column_increment / 2;
    size_t user_column_count = 0;
    for (size_t i=0; i<user_allocation_total; ++i){
      if (users[i].trash == FALSE){
        //printf("draw column for user %s\n", users[i].name);
        users[i].column_index = user_column_count;
        users[i].column_center_px = user_column_loc - name_textbox.width/2;
        sdlj_textbox_render(render, &name_textbox, users[i].name);
        SDL_Rect src = {0, 0, name_textbox.width, name_textbox.height};
        SDL_Rect dst = {
          users[i].column_center_px,
          5, 
          name_textbox.width, name_textbox.height};
        assert(SDL_RenderCopy(render, name_textbox.texture, &src, &dst) == 0);

        user_column_loc += user_column_increment;
        user_column_count += 1;
      }
    }

    // TODO a temporary way to see task distribution amongst users. not scheduled... :( 
    int column_usage[user_allocation_used];
    for(size_t i=0; i<user_allocation_used; ++i){
      column_usage[i] = 0;
    }

    //// DRAW TASK BOXES IN DISPLAY VIEWPORT
    // TODO time scheduling function; how to make a grid of time and render some sensible view of that (let time drive position of things)
    // TODO stretch tasks that correspond to multi users. make some kind of faded shadow indicator to dive underneath others?
    int locx = 10;
    int locy = 50;

    for (size_t n=0; n<task_allocation_total; ++n){
      if (tasks[n].trash == FALSE){
        Task_Node* task = tasks + n;

        for (size_t u=0; u<task->user_qty; ++u){
          User* user = task->users[u];
          locx = user->column_center_px;
          locy = column_usage[user->column_index]*50 + 50;
          
          draw_box(render, locx, locy, 0, tasks+n);

          column_usage[user->column_index] += 1;
        }

        // TODO need to know the expected width to make sure it doesn't go offscreen? or don't care
        // TODO camera coordinate system; make layout somewhat independent of shown pixels
        // TODO an actual layout engine, show properties of the nodes and such
        //locx = locx + 120;
        // if (locx > viewport_display.w){
        //  locx = 10;
        //  locy += 100;
        // }
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
  editor_bufffer_destroy(text_buffer);

 return 0;
}
