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
#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 900
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

void tasks_init(Task_Memory* task_memory, User_Memory* user_memory){
  task_memory->allocation_total = 64;
  task_memory->allocation_used = 0;
  task_memory->tasks = (Task*) malloc(task_memory->allocation_total * sizeof(Task));
  task_memory->hashtable = hash_table_create(HT_TASKS_MAX, HT_FREE_KEY);

  for (size_t i=0; i<task_memory->allocation_total; ++i){
    task_memory->tasks[i].trash = TRUE;
  }
  task_memory->editor_visited = (uint8_t*) malloc(task_memory->allocation_total * sizeof(uint8_t));
  memset(task_memory->editor_visited, 0, task_memory->allocation_total);
  printf("Task init() complete for %ld tasks\n", task_memory->allocation_total);

  status_color_init();

  user_memory->allocation_total = 8;
  user_memory->allocation_used = 0;
  user_memory->users = (User*) malloc(user_memory->allocation_total * sizeof(User));
  for (size_t i=0; i<user_memory->allocation_total; ++i){
    user_memory->users[i].trash = TRUE;
  }
  user_memory->hashtable = hash_table_create(HT_USERS_MAX, HT_FREE_KEY);
  user_memory->editor_visited = (uint8_t*) malloc(user_memory->allocation_total * sizeof(uint8_t));
  memset(user_memory->editor_visited, 0, user_memory->allocation_total);
}


void tasks_free(Task_Memory* task_memory, User_Memory* user_memory){
  printf("[STATUS] FREEING TASK TABLE\n");
  hash_table_print(task_memory->hashtable);
  hash_table_destroy(task_memory->hashtable);
  free(task_memory->tasks);
  free(task_memory->editor_visited);

  hash_table_print(user_memory->hashtable);
  hash_table_destroy(user_memory->hashtable); 
  free(user_memory->users);
  free(user_memory->editor_visited);
}


// grow memory as needed to hold allocated tasks
// don't shrink - avoid having to search and move active nodes into lower memory space in realtime
// if needed user could save and restart to reduce memory footprint
void task_memory_management(Task_Memory* tm){
  if (tm->allocation_used >= tm->allocation_total){
    printf("[CAUTION] TASK MEMORY MANAGEMENT ACTIVATED, INCREASING MEMORY ALLOCATIONS\n");
    uint64_t task_allocation_old = tm->allocation_total;
    tm->allocation_total *= 1.5;
    tm->tasks = (Task*) realloc(tm->tasks, tm->allocation_total * sizeof(Task));
    tm->editor_visited = (uint8_t*) realloc(tm->editor_visited, tm->allocation_total * sizeof(uint8_t));

    for (size_t i=task_allocation_old; i<tm->allocation_total; ++i){
      tm->tasks[i].trash = TRUE;
      tm->editor_visited[i] = FALSE;
    }
  }

  // TODO some way to update has task_names_ht size.. would need to re-index all tasks :(
}


Task* task_create(Task_Memory* task_memory, char* task_name, size_t task_name_length){
  Task* tasks = task_memory->tasks;

  // find an empty slot to use for the task
  do {
    task_memory->last_created = (task_memory->last_created + 1) % task_memory->allocation_total;
  } while (tasks[task_memory->last_created].trash == FALSE);
  task_memory->allocation_used += 1;

  Task* task = (tasks + task_memory->last_created);

  // zero everything there. also brings mode out of trash mode
  memset((void*) task, 0, sizeof(Task));
  task->trash = FALSE;
  
  // add to hash table
  char* name = (char*) malloc(task_name_length+1);
  memcpy(name, task_name, task_name_length);
  name[task_name_length] = '\0';
  hash_table_insert(task_memory->hashtable, name, (void*) task);
  task->task_name = name;

  return task;
}


Task* task_get(Task_Memory* task_memory, char* task_name, int task_name_length){
  // use the hash table to find a pointer to the task based on the string name the user gives
  // return NULL if the task does not exist and needs to be created
  char name[128]; // TODO
  assert(task_name_length < 128);
  memcpy(name, task_name, task_name_length);
  name[task_name_length] = '\0';
  Task* task = (Task*) hash_table_get(task_memory->hashtable, name);
  return task;
}


// check if the given user is already assigned to the given task
uint8_t task_user_has(Task* task, User* user){
  uint8_t result = FALSE;
  for(size_t i=0; i<task->user_qty; ++i){
    if (task->users[i] == user){
      result = TRUE;
    }
  }
  return result;
}


// add user to task if user is not already there
void task_user_add(Task* task, User* user){
  assert(task->user_qty < TASK_USERS_MAX);
  assert(user->task_qty < USER_TASKS_MAX);

  if (task_user_has(task, user) == FALSE){
    task->users[task->user_qty] = user;
    task->user_qty += 1;

    user->tasks[user->task_qty] = task;
    user->task_qty += 1;
  }
}


void task_user_remove(Task* task, User* user){
  size_t id = 0;
  uint8_t found = 0;
  for(size_t i=0; i<task->user_qty; ++i){
    if (task->users[i] == user){
      id = i;
      found = 1;
      break;
    }
  }

  // only continue with removal process if you can find the user
  if (found == 1){
    printf("need to remove user %s from task %s. userid in that task is %ld\n", user->name, task->task_name, id);
    
    // now shuffle down all the remaining users, update the qty
    for (size_t i=id; i<task->user_qty-1; ++i){
      task->users[i] = task->users[i+1];
    }
    task->user_qty -= 1;
  }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void user_memory_management(User_Memory* um){
  if (um->allocation_used >= um->allocation_total){
    printf("[CAUTION] USER MEMORY MANAGEMENT ACTIVATED, INCREASING MEMORY ALLOCATIONS\n");
    uint64_t user_allocation_old = um->allocation_total;
    um->allocation_total *= 1.5;
    um->users = (User*) realloc(um->users, um->allocation_total * sizeof(User));

    for (size_t i=user_allocation_old; i<um->allocation_total; ++i){
      um->users[i].trash = TRUE;
      um->editor_visited[i] = FALSE;
    }
  }
}


User* user_create(User_Memory* user_memory, char* user_name, size_t name_length){
  user_memory_management(user_memory);
  // find an empty user slot to use
  do {
    user_memory->last_created = (user_memory->last_created + 1) % user_memory->allocation_total;
  } while(user_memory->users[user_memory->last_created].trash == FALSE);
  User* user = user_memory->users + user_memory->last_created;
  user_memory->allocation_used += 1;

  // zero everything there
  memset((void*) user, 0, sizeof(User));
  user->trash = FALSE; 

  char* name = (char*) malloc(name_length+1);
  memcpy(name, user_name, name_length);
  name[name_length] = '\0';
  hash_table_insert(user_memory->hashtable, name, (void*) user);
  user->name = name;

  return user;
}


User* user_get(User_Memory* user_memory, char* user_name, int user_name_length){
  // use the hash table to find a pointer to the user based on the string given
  // return NULL if does not exist and needs to be created
  char name[128]; // TODO
  memcpy(name, user_name, user_name_length);
  name[user_name_length] = '\0';
  User* user = (User*) hash_table_get(user_memory->hashtable, name);
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


void editor_tasks_cleanup(Task_Memory* task_memory){
  Task* tasks = task_memory->tasks;
  // scrub through tasks, remove any that you expected to see but did not
  for (size_t i=0; i<task_memory->allocation_total; ++i){
    if (tasks[i].trash == FALSE){ // if node is NOT trash
      if (tasks[i].mode_edit == TRUE){

        // if we did not visit the node this time parsing the text
        if (task_memory->editor_visited[i] == FALSE){ 
          tasks[i].trash = TRUE; 
          if (task_memory->allocation_used > 0){
            task_memory->allocation_used -= 1;
          }
          printf("REMOVING tasks[%ld].name=%s..\n", i, tasks[i].task_name);
          hash_table_remove(task_memory->hashtable, tasks[i].task_name);
        }
      }
    }
  }

  hash_table_print(task_memory->hashtable);
}


// TODO this has a bug.. not removing tasks from users properly when that user is removed from one task but still has other valid tasks
// don't track seen user? track seen task.user? 
void editor_users_cleanup(User_Memory* user_memory){
  User* users = user_memory->users;

  // scrub through users, remove any that you expected to see but did not
  for (size_t i=0; i<user_memory->allocation_total; ++i){
    if(users[i].trash == FALSE){
      if (user_memory->editor_visited[i] == FALSE){

        if (users[i].mode_edit == TRUE){
          // this user was expected to be seen upon parsing but was not; delete them!!
          users[i].trash = TRUE;
          if (user_memory->allocation_used > 0){
            user_memory->allocation_used -= 1;
          }
          printf("REMOVING users[%ld].name=%s..\n", i, users[i].name);

          // user deleted, remove them from all their tasks
          for(size_t j=0; j<users[i].task_qty; ++j){
            task_user_remove(users[i].tasks[j], users+i);
          }

          hash_table_remove(user_memory->hashtable, users[i].name);
        }
      }
    }
  }
}


void editor_parse_task_detect(Task_Memory* task_memory, char* text_start, size_t text_length){
  printf("[STATUS] PASS 1 editor_parse_task_detect()\n");
  char* text_end = text_start + text_length;
  char* line_start = text_start;
  char* line_end;
  int line_working_length = 0;
  Task* task;
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
      // TODO check, prevent duplicate task names
      int task_name_length;
      char* task_name = string_strip(&task_name_length, line_start, line_working_length);
      printf("TASK, name '%.*s'\n", task_name_length, task_name);

      // now get a pointer to the task
      task = task_get(task_memory, task_name, task_name_length);
      if (task == NULL){
        task = task_create(task_memory, task_name, task_name_length); // TODO is create the right action? maybe parse and then decide? 
        printf("created task. allocations: %ld of %ld\n", task_memory->allocation_used, task_memory->allocation_total);
      }

      // mark task as visited
      task_memory->editor_visited[task - task_memory->tasks] = TRUE;
      task->mode_edit = TRUE; // TODO this is a hack since this should be set by the DISPLAY VIEWPORT
    }

    line_start = line_end + 1;
  }
}


uint64_t editor_parse_date(char* value_str, int value_str_length){
  struct tm timeinfo;

  char* line_end = value_str + value_str_length;

  // separate year from month
  char* split1 = memchr(value_str, (int) '-', value_str_length);
  if (split1 != NULL){
    timeinfo.tm_year = strtol(value_str, &split1, 10);

    // separate month from day
    char* split2 = memchr(split1+1, (int) '-', value_str_length - (split1 - value_str) - 1);
    if (split2 != NULL){
      timeinfo.tm_mon = strtol(split1+1, &split2, 10);
      timeinfo.tm_mday = strtol(split2+1, &line_end, 10);
    }
    // can't find a day, use start of the month as default
    else{
      timeinfo.tm_mon = strtol(split1+1, &line_end, 10);
      timeinfo.tm_mday = 1;
    }
  }

  // can't find a month (or day), use start as default
  else{
    timeinfo.tm_year = strtol(value_str, &line_end, 10);
    timeinfo.tm_mon = 1;
    timeinfo.tm_mday = 1;
  }

  // now convert that into epoch time..
  timeinfo.tm_sec = 0;
  timeinfo.tm_min = 0;
  timeinfo.tm_hour = 0;
  time_t date_epoch = mktime(&timeinfo);

  uint64_t day = (uint64_t) date_epoch / 86400;

  //printf("Year: %d\n", timeinfo.tm_year);
  //printf("Month: %d\n", timeinfo.tm_mon);
  //printf("Day: %d\n", timeinfo.tm_mday);

  return day;
}

// comma to separate values in a list
void editor_parse_propertyline(Task_Memory* task_memory, User_Memory* user_memory, Task* task, char* line_start, int line_working_length){
  char* line_end = line_start + line_working_length;
  // split into property and value parts. split on ':'
  char* split = memchr(line_start, (int) ':', line_working_length);
  int property_str_length;
  char* property_str = string_strip(&property_str_length, line_start, split - line_start);
  int value_str_length;
  char* value_str = string_strip(&value_str_length, split, line_end - split);

  printf("(task %s) add property='%.*s'  value='%.*s'\n", task->task_name, property_str_length, property_str, value_str_length, value_str);
  if (value_str_length == 0){
    return;
  }

  if (memcmp(property_str, "user", 4) == 0){
    printf("user property familiar!\n");

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
        User* user = user_get(user_memory, value, value_length);
        if (user == NULL){
          printf("user: '%.*s' NEW!\n", value_length, value);
          user = user_create(user_memory, value, value_length); 
        }
        else{
          printf("user: '%.*s'\n", value_length, value);
        }
        user->trash = FALSE;
        user->mode_edit = TRUE;
        user_memory->editor_visited[user - user_memory->users] = TRUE;

        // assign to the task, if it is not already there
        task_user_add(task, user);
      }

      property_split_start = property_split_end + 1;
    }
  }

  else if(memcmp(property_str, "dependent_on", 12) == 0){
    printf("parsing dependencies\n");
    char* property_split_start = value_str;
    char* property_split_end = value_str;

    while (property_split_start < line_end){
      property_split_end = memchr(property_split_start, (int) ',', line_end - property_split_start);
      if (property_split_end == NULL){
        property_split_end = line_end;
      }

      // parse what you find
      int value_length;
      char* value = string_strip(&value_length, property_split_start, property_split_end - property_split_start);
      if (value_length > 0){
        Task* dep = task_get(task_memory, value, value_length);
        if (dep != NULL){
          task->dependents[task->dependent_qty] = dep;
          task->dependent_qty += 1;
        }
        else{
          // TODO something when dependencies don't exist
        }
      }

      property_split_start = property_split_end + 1;
    }
  }

  else if(memcmp(property_str, "duration", 8) == 0){
    int duration = atoi(value_str);
    task->day_duration = duration;
    task->schedule_constraints |= SCHEDULE_CONSTRAINT_DURATION;
  }
  else if(memcmp(property_str, "fixed_start", 11) == 0){
    task->schedule_constraints |= SCHEDULE_CONSTRAINT_START;
    task->day_start = editor_parse_date(value_str, value_str_length);
  }
  else if(memcmp(property_str, "fixed_end", 9) == 0){
    task->schedule_constraints |= SCHEDULE_CONSTRAINT_END;
    task->day_end = editor_parse_date(value_str, value_str_length);
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
// modify the full network directly. automatically delete/re-add everything being edited in edit mode. assume all those nodes selected are trashed and revised. 
// how to balance reparsing everything at 100Hz and only reparsing what is needed? maybe use the cursor to direct efforts? only reparse from scratch the node the cursor is in
void editor_parse_text(Task_Memory* task_memory, User_Memory* user_memory, char* text_start, size_t text_length){
  char* text_end = text_start + text_length;
  Task* tasks = task_memory->tasks;

  // track difference betweeen seen [tasks, users] and expected to see tasks
  // if you don't see items that you expect to.. need to remove those!
  for (size_t i=0; i<task_memory->allocation_total; ++i){
    task_memory->editor_visited[i] = FALSE;
  }
  for (size_t i=0; i<user_memory->allocation_total; ++i){
    user_memory->editor_visited[i] = FALSE;
  }
  for (size_t i=0; i<task_memory->allocation_total; ++i){
    if (tasks[i].mode_edit == TRUE){
      tasks[i].dependent_qty = 0;
      tasks[i].user_qty = 0;
    }
  }

  // PASS 1 - just add/remove tasks 
  editor_parse_task_detect(task_memory, text_start, text_length);

  // PASS 2 - all task properties, now you can scrub dependencies TODO
  // read one line at a time
  printf("[STATUS] PASS 2 working through the properties\n");
  char* line_start = text_start;
  char* line_end;
  int line_working_length = 0;
  Task* task;
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
      int task_name_length;
      char* task_name = string_strip(&task_name_length, line_start, line_working_length);
      task = task_get(task_memory, task_name, task_name_length);
      assert( task != NULL);
    }

    else if (memchr(line_start, (int) '}', line_working_length) != NULL){
      printf("line '%.*s' ends a task\n", line_working_length, line_start);
    }

    else if(memchr(line_start, (int) ':', line_working_length) != NULL){
      editor_parse_propertyline(task_memory, user_memory, task, line_start, line_working_length);
    }

    // advance to the next line
    line_start = line_end + 1;
  } // done going through lines
  // text at the top level creates activities with that name
  // open bracket increases to the next level
  // text at the next level causes a lookup for a struct member. colon separator

  editor_tasks_cleanup(task_memory);
  editor_users_cleanup(user_memory);

  printf("[STATUS] Finished parsing text this round\n");

  // TODO add better error handling warning stuff
}


void editor_generate_text(TextBuffer* text_buffer){
  text_buffer->length = 1; // TODO
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


void draw_box(SDL_Renderer* render, int x, int y, int flags, Task* task){

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

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TextBuffer* editor_buffer_init(){
  TextBuffer* tb = (TextBuffer*) malloc(sizeof(TextBuffer));

  tb->text = (char*) malloc(EDITOR_BUFFER_LENGTH * sizeof(char));
  memset(tb->text, 0, EDITOR_BUFFER_LENGTH);
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

void editor_load_text(Task_Memory* task_memory, User_Memory* user_memory, TextBuffer* text_buffer, const char* filename){

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
  editor_parse_text(task_memory, user_memory, text_buffer->text, text_buffer->length);
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

  Task_Memory  task_memory_object;
  Task_Memory* task_memory = &task_memory_object;
  User_Memory  user_memory_object;
  User_Memory* user_memory = &user_memory_object;
  tasks_init(task_memory, user_memory);

  TextBuffer* text_buffer = editor_buffer_init();
  // if not loading a text buffer.. make it a space and length one
  //text_buffer->text[0] = ' ';
  //text_buffer->length = 1;
  editor_load_text(task_memory, user_memory, text_buffer, "examples/demo1.json"); // TODO temporary.. later use filename


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
          // F2 - rename symbol in edit mode
          // SHIFT+F2 - rename symbol even if not in edit mode
          // F3 - search stuff! can be smart scoping?

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
      editor_parse_text(task_memory, user_memory, text_buffer->text, text_buffer->length);

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


    if ((user_memory->allocation_used > 0) && (task_memory->allocation_used > 0)){
      User* users = user_memory->users;
      //// DRAW USER NAMES
      // TODO what is the right way to later connect user name to a column location (eventually, in pixels)

      int user_column_increment = viewport_display.w / (user_memory->allocation_used);
      int user_column_loc = user_column_increment / 2;
      size_t user_column_count = 0;
      for (size_t i=0; i<user_memory->allocation_total; ++i){
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
      int column_usage[user_memory->allocation_used];
      for(size_t i=0; i<user_memory->allocation_used; ++i){
        column_usage[i] = 0;
      }

      //// DRAW TASK BOXES IN DISPLAY VIEWPORT
      // TODO time scheduling function; how to make a grid of time and render some sensible view of that (let time drive position of things)
      // TODO stretch tasks that correspond to multi users. make some kind of faded shadow indicator to dive underneath others?
      int locx = 10;
      int locy = 50;

      for (size_t n=0; n<task_memory->allocation_total; ++n){
        if (task_memory->tasks[n].trash == FALSE){
          Task* task = task_memory->tasks + n;

          for (size_t u=0; u<task->user_qty; ++u){
            User* user = task->users[u];
            locx = user->column_center_px;
            locy = column_usage[user->column_index]*50 + 50;
            
            draw_box(render, locx, locy, 0, task_memory->tasks+n);

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
    }
    // TODO graveyard for orphaned tasks (no users, improper dependencies to be plotted, etc.)
        

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
  tasks_free(task_memory, user_memory);
  editor_bufffer_destroy(text_buffer);

 return 0;
}
