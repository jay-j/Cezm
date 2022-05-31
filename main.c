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
#include "keyboard_bindings.h"
#include "utilities-c/hash_lib/hashtable.h"

// global
#define WINDOW_WIDTH_INIT 1600
#define WINDOW_HEIGHT_INIT 1000
#define FONTSIZE 16

// modal switching
enum VIEWPORT_TYPES {
  VIEWPORT_EDITOR,
  VIEWPORT_DISPLAY,
  VIEWPORT_RENAME
};

// viewport-editor related
#define LINE_MAX_LENGTH 512
#define EDITOR_BUFFER_LENGTH 1024
#define EDITOR_LINES_MAX 1024
TTF_Font* global_font = NULL;

// viewport-display related
#define TASK_DISPLAY_LIMIT 1024

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
  task_memory->temp_status = (uint8_t*) malloc(task_memory->allocation_total * sizeof( *task_memory->temp_status));
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
  free(task_memory->temp_status);

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
    tm->temp_status = (uint8_t*) realloc(tm->temp_status, tm->allocation_total * sizeof( *tm->temp_status));

    for (size_t i=task_allocation_old; i<tm->allocation_total; ++i){
      tm->tasks[i].trash = TRUE;
      tm->editor_visited[i] = FALSE;
    }
  }

  // TODO some way to update has task_names_ht size.. would need to re-index all tasks :(
}


// only allowed to create a task in edit mode
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
  task->task_name_length = task_name_length;
  task->mode_edit = TRUE; 
  
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


// edits task to remove user, and user to remove task
void task_user_remove(Task* task, User* user){
  size_t id = 0;
  uint8_t found = 0;
  for(size_t i=0; i<task->user_qty; ++i){ // TODO don't re-do the search.. we have the offset
    if (task->users[i] == user){
      id = i;
      found = 1;
      break;
    }
  }

  // only continue with removal process if you can find the user
  if (found == 1){
    
    // now shuffle down all the remaining users, update the qty
    for (size_t i=id; i<task->user_qty-1; ++i){
      task->users[i] = task->users[i+1];
    }
    task->user_qty -= 1;
  }

  // now remove the task from the user struct
  uint8_t found2 = 0;
  for(size_t i=0; i<user->task_qty; ++i){
    if (user->tasks[i] == task){
      id = i;
      found2 = 1;
      break;
    }
  }

  if (found2 == 1){
    // and shuffle down the remaining users
    for (size_t i=id; i<user->task_qty; ++i){
      user->tasks[i] = user->tasks[i+1];
    }
    user->task_qty -= 1;
  }

  assert(found == found2);
}


void task_user_remove_unvisited(Task* task, User_Memory* user_memory){
  if (task->user_qty > 0){
    for (size_t u=task->user_qty; u>0; u--){
      User* user = task->users[u-1];
      size_t uindex = user - user_memory->users;
      if (user_memory->editor_visited[uindex] == FALSE){
        task_user_remove(task, user);
      }
    }
  }
}
 

void task_destroy(Task_Memory* task_memory, Task* task){
  assert(task->trash == FALSE); // don't try and remove already-removed tasks
  task->trash = TRUE;
  if (task_memory->allocation_used > 0){
    task_memory->allocation_used -= 1;
  }
  printf("REMOVING tasks.name=%s..\n", task->task_name);
  hash_table_remove(task_memory->hashtable, task->task_name);

  for (size_t u=0; u<task->user_qty; ++u){
    task_user_remove(task, task->users[u]);
  }
}


void task_dependents_find_all(Task_Memory* task_memory){
  Task* tasks = task_memory->tasks;

  // clear previous info
  for (size_t t=0; t<task_memory->allocation_total; ++t){
    tasks[t].dependent_qty = 0;
  }

  // search and add
  for( size_t t=0; t<task_memory->allocation_total; ++t){
    if (tasks[t].trash == FALSE){
      for (size_t i=0; i<tasks[t].prereq_qty; ++i){
        Task* prereq = tasks[t].prereqs[i];
        prereq->dependents[prereq->dependent_qty] = tasks+t;
        prereq->dependent_qty += 1;
      }
    }
  }
} 


void task_name_generate(Task_Memory* task_memory, Task* base, char* result_name, int* result_length){
  int i = 0;
  Task* exists = NULL;
  do {
    ++i;
    *result_length = sprintf(result_name, "%s%d", base->task_name, i);
    exists = task_get(task_memory, result_name, *result_length);
    if (i > 999){
      printf("[ERROR] COULD NOT FIND A VALID NEW TASK NAME\n");
      assert(0);
    }
  } while (exists != NULL);
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
  user->name_length = name_length;

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
          task_destroy(task_memory, tasks+i);
        }
      }
    }
  }

  hash_table_print(task_memory->hashtable);
}


// scrub through users, remove any that have zero assigned tasks (user->task_qty = 0)
void editor_users_cleanup(User_Memory* user_memory){
  User* users = user_memory->users;

  for (size_t i=0; i<user_memory->allocation_total; ++i){
    if(users[i].trash == FALSE){
      if (users[i].task_qty == 0){

        users[i].trash = TRUE;
        if (user_memory->allocation_used > 0){
          user_memory->allocation_used -= 1;
        }
        printf("REMOVING users[%ld].name=%s..\n", i, users[i].name);

        hash_table_remove(user_memory->hashtable, users[i].name);
      }
    }
  }
  hash_table_print(user_memory->hashtable);
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
      if (text_end - line_start > 0){
        line_end = text_end;
      }
      else{
        break;
      }
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
      if (task_name_length > 0){
        printf("detected task: '%.*s'\n", task_name_length, task_name);

        // now get a pointer to the task
        task = task_get(task_memory, task_name, task_name_length);
        if (task == NULL){
          task = task_create(task_memory, task_name, task_name_length); // TODO is create the right action? maybe parse and then decide? 
          printf("created task. allocations: %ld of %ld\n", task_memory->allocation_used, task_memory->allocation_total);
        }

        // mark task as visited
        task_memory->editor_visited[task - task_memory->tasks] = TRUE;
      }
    }

    line_start = line_end + 1;
  }
}


uint64_t editor_parse_date(char* value_str, int value_str_length){
  // get the current time to initialize some values of timeinfo with time zone and stuff
  time_t now; 
  time(&now);
  struct tm* timeinfo = gmtime(&now);

  char* line_end = value_str + value_str_length;

  // separate year from month
  char* split1 = memchr(value_str, (int) '-', value_str_length);
  if (split1 != NULL){
    timeinfo->tm_year = strtol(value_str, &split1, 10);

    // separate month from day
    char* split2 = memchr(split1+1, (int) '-', value_str_length - (split1 - value_str) - 1);
    if (split2 != NULL){
      timeinfo->tm_mon = strtol(split1+1, &split2, 10);
      timeinfo->tm_mday = strtol(split2+1, &line_end, 10);
    }
    // can't find a day, use start of the month as default
    else{
      timeinfo->tm_mon = strtol(split1+1, &line_end, 10);
      timeinfo->tm_mday = 1;
    }
  }

  // can't find a month (or day), use start as default
  else{
    timeinfo->tm_year = strtol(value_str, &line_end, 10);
    timeinfo->tm_mon = 1;
    timeinfo->tm_mday = 1;
  }

  // now convert that into epoch time..
  timeinfo->tm_year -= 1900; // years are 1900-indexed
  timeinfo->tm_mon -= 1; // months are zero-indexed
  timeinfo->tm_sec = 0;
  timeinfo->tm_min = 0;
  timeinfo->tm_hour = 0;
  time_t date_epoch = mktime(timeinfo);
  assert(date_epoch != -1);

  // date_epoch is seconds since epoch. divide to get days since epoch
  uint64_t day = (uint64_t) date_epoch / 86400;

  //printf("Year: %d\n", timeinfo.tm_year);
  //printf("Month: %d\n", timeinfo.tm_mon);
  //printf("Day: %d\n", timeinfo.tm_mday);

  return day;
}


char* text_append_date(char* text_output, uint64_t day){
  // convert days since epoch to second since epoch
  time_t date_epoch = day * 86400;

  // GM time is UTC (GMT timezome - ignoring timezones)
  struct tm* timeinfo = gmtime(&date_epoch);

  size_t result = strftime(text_output, 11, "%F", timeinfo);
  assert(result != 0);

  return text_output+10;
}


// comma to separate values in a list
void editor_parse_propertyline(Task_Memory* task_memory, User_Memory* user_memory, Task* task, char* line_start, int line_working_length, TextBuffer* text_buffer, Text_Cursor* text_cursor){
  char* line_end = line_start + line_working_length;
  // split into property and value parts. split on ':'
  char* split = memchr(line_start, (int) ':', line_working_length);
  int property_str_length;
  char* property_str = string_strip(&property_str_length, line_start, split - line_start);
  if (property_str_length == 0){
    return;
  }

  int value_str_length;
  char* value_str = string_strip(&value_str_length, split, line_end - split);

  printf("(task %s) add property='%.*s'  value='%.*s'\n", task->task_name, property_str_length, property_str, value_str_length, value_str);
  if (value_str_length == 0){
    return;
  }

  if (memcmp(property_str, "user", 4) == 0){
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
          printf("user: '%.*s' EXISTING\n", value_length, value);
        }
        user->trash = FALSE;
        user->mode_edit = TRUE;
        user_memory->editor_visited[user - user_memory->users] = TRUE;

        // assign to the task, if it is not already there
        task_user_add(task, user);

        // mark in cursor TODO MULTICURSOR
        if ((text_cursor->pos[0] >= property_split_start - text_buffer->text) && (text_cursor->pos[0] <= property_split_end - text_buffer->text)){
          text_cursor->entity_type = TEXTCURSOR_ENTITY_USER;
          text_cursor->entity = (void*) user;
          printf("  [CURSOR DETECT] says cursor on user '%s'\n", user->name);
        }
      }

      property_split_start = property_split_end + 1;
    }
  }

  else if(memcmp(property_str, "prereq", 6) == 0){
    printf("parsing prerequisites\n");
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
        Task* prereq = task_get(task_memory, value, value_length);
        if (prereq != NULL){
          task->prereqs[task->prereq_qty] = prereq;
          task->prereq_qty += 1;

          // mark in cursor TODO MULTICURSOR
          if ((text_cursor->pos[0] >= property_split_start - text_buffer->text) && (text_cursor->pos[0] <= property_split_end - text_buffer->text)){
            text_cursor->entity_type = TEXTCURSOR_ENTITY_PREREQ;
            text_cursor->entity = (void*) prereq;
            printf("  [CURSOR DETECT] says cursor on prereq '%s'\n", prereq->task_name);
          }
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
void editor_parse_text(Task_Memory* task_memory, User_Memory* user_memory, TextBuffer* text_buffer, Text_Cursor* text_cursor){
  uint64_t cpu_timer_start = SDL_GetPerformanceCounter();

  char* text_start = text_buffer->text;
  size_t text_length = text_buffer->length;
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
      tasks[i].prereq_qty = 0;
      //tasks[i].user_qty = 0;
    }
  }
  text_cursor->entity_type = TEXTCURSOR_ENTITY_NONE;
  text_cursor->entity = NULL;

  // PASS 1 - just add/remove tasks, mark them as visited. new tasks are marked edit_mode = TRUE
  editor_parse_task_detect(task_memory, text_start, text_length);

  // reset some properties for all tasks in the editor
  for (size_t t=0; t<task_memory->allocation_total; ++t){
    if (task_memory->tasks[t].mode_edit == TRUE){
      task_memory->tasks[t].schedule_constraints = 0;
    }
  }

  // PASS 2 - all task properties, now you can scrub dependencies
  // read one line at a time
  printf("[STATUS] PASS 2 working through the properties\n");
  char* line_start = text_start;
  char* line_end;
  int line_working_length = 0;
  Task* task = NULL;
  while (line_start < text_end){
    line_end = memchr(line_start, (int) '\n', text_end - line_start);
    if (line_end == NULL){
      if (text_end - line_start > 0){
        line_end = text_end;
      }
      else{
        break;
      }
    }
    line_working_length = line_end - line_start;
    if (line_working_length == 0){
      ++line_start;
      continue;
    }

    // task open
    if (memchr(line_start, (int) '{', line_working_length) != NULL){
      // cleanup old tasks that haven't been closed properly yet
      if (task != NULL){
        task_user_remove_unvisited(task, user_memory);
        task = NULL;
      }

      int task_name_length;
      char* task_name = string_strip(&task_name_length, line_start, line_working_length);
      if (task_name_length > 0){
        task = task_get(task_memory, task_name, task_name_length);
        assert( task != NULL);
      }

      // TODO MULTICURSOR
      if ((text_cursor->pos[0] > line_start - text_start) && (text_cursor->pos[0] < line_end - text_start)){
        printf("cursor on line creating task '%s'\n", task->task_name);
        text_cursor->entity_type = TEXTCURSOR_ENTITY_TASK;
        text_cursor->entity = (void*) task;
      }
    }

    // task close
    else if (memchr(line_start, (int) '}', line_working_length) != NULL){
      if (task != NULL){
        printf("detected '}' ... cleaning up / closing task '%s'\n", task->task_name);
        task_user_remove_unvisited(task, user_memory);
        task = NULL;
      }
    }

    // property line
    else if(memchr(line_start, (int) ':', line_working_length) != NULL){
      editor_parse_propertyline(task_memory, user_memory, task, line_start, line_working_length, text_buffer, text_cursor);
    }

    // advance to the next line
    line_start = line_end + 1;
  } // done going through lines

  // text at the top level creates activities with that name
  // open bracket increases to the next level
  // text at the next level causes a lookup for a struct member. colon separator

  // cleanup tasks that are in the progress of being written and don't have a close brace yet
  if (task != NULL){
    task_user_remove_unvisited(task, user_memory);
    task = NULL;
  }

  editor_tasks_cleanup(task_memory);
  editor_users_cleanup(user_memory);

  task_dependents_find_all(task_memory);

  uint64_t cpu_timer_end = SDL_GetPerformanceCounter();
  double cpu_timer_elapsed = ((double) cpu_timer_end - cpu_timer_start) / ((double) SDL_GetPerformanceFrequency());
  printf("[STATUS] Finished parsing text this round, time: %.3lf ms\n", cpu_timer_elapsed*1000);

  // TODO add better error handling warning stuff
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void sdl_startup(SDL_Window** win, SDL_Renderer** render){

  if( SDL_Init(SDL_INIT_VIDEO) < 0){
    printf("SDL init failed! %s\n", SDL_GetError());
    assert(0);
  }
 
  if (SDL_GetNumVideoDisplays() > 1){ // TODO HACK for stream
    SDL_Rect monitor;
    SDL_GetDisplayBounds(1, &monitor);
    *win = SDL_CreateWindow("[unnamed project planning software]", 
      monitor.x, monitor.y,
      WINDOW_WIDTH_INIT, WINDOW_HEIGHT_INIT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
  }
  else{
    *win = SDL_CreateWindow("[unnamed project planning software]", 
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
      WINDOW_WIDTH_INIT, WINDOW_HEIGHT_INIT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
  }
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


void task_draw_box(SDL_Renderer* render, Task_Display* task_display){
  Task* task = task_display->task;

  int border = 3;

  // draw outline if SELECTED
  if (task_display->task->mode_edit == TRUE){
    SDL_Rect outline;
    outline.x = task_display->local.x-border;
    outline.y = task_display->local.y-border;
    outline.w = task_display->local.w + 2*border;
    outline.h = task_display->local.h + 2*border;

    SDL_SetRenderDrawColor(render, 200, 100, 0, 255); // orange
    SDL_RenderFillRect(render, &outline);
  }

  // TODO draw a different border if the task is ACTIVE 

  
  // draw the base box
  int sc = task->status_color;
  SDL_SetRenderDrawColor(render, status_colors[sc].r, status_colors[sc].g, status_colors[sc].b, status_colors[sc].a);
  //SDL_SetRenderDrawColor(render, 220, 220, 220, 255); // light grey? 
  SDL_RenderFillRect(render, &task_display->local);
  
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
  textbox.x = task_display->local.x + border;
  textbox.y = task_display->local.y + border;
  TTF_SizeText(global_font, task->task_name, &textbox.w, &textbox.h);

  SDL_Rect src = {0, 0, textbox.w, textbox.h};
  assert(SDL_RenderCopy(render, texture, &src, &textbox) == 0);
  
  // TODO find a different way to do this that doesn't involve so much memory alloc and dealloc!!!
  SDL_DestroyTexture(texture);
  SDL_FreeSurface(surface);
}


// TODO make more efficient with some texture caching? 
// TODO lower transparency if drawing behind something?
void draw_dependency_curve(SDL_Renderer* render, int start_x, int start_y, int end_x, int end_y){ 
  float increment = 1.0 / 48.0; 
  float t = 0;
  
  int control_offset = 80;
  int draw_start_x = start_x;
  int draw_start_y = start_y;
  int draw_end_x;
  int draw_end_y;
  while (t < 1 + increment){
    // calulcate coordinates
    draw_end_x = (1-t)*(1-t)*(1-t)*start_x + 3.0*(1-t)*(1-t)*t*start_x + 3.0*(1-t)*t*t*end_x + t*t*t*end_x;
    draw_end_y = (1-t)*(1-t)*(1-t)*start_y + 3.0*(1-t)*(1-t)*t*(start_y+control_offset) + 3.0*(1-t)*t*t*(end_y - control_offset) + t*t*t*end_y;

    // draw
    SDL_RenderDrawLine(render, draw_start_x, draw_start_y, draw_end_x, draw_end_y); 
    
    // increment
    t += increment;
    draw_start_x = draw_end_x;
    draw_start_y = draw_end_y;
  }
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

void editor_buffer_destroy(TextBuffer* tb){
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

void editor_cursor_reset(Text_Cursor* text_cursor){
  text_cursor->pos[0] = 0;
  text_cursor->x[0] = 0;
  text_cursor->y[0] = 0;
  text_cursor->qty = 1;
  text_cursor->entity_type = TEXTCURSOR_ENTITY_NONE;
}


Text_Cursor* editor_cursor_create(){
  Text_Cursor* text_cursor = (Text_Cursor*) malloc(sizeof(Text_Cursor));
  text_cursor->pos = malloc(CURSOR_QTY_MAX * sizeof(*text_cursor->pos));
  text_cursor->x   = malloc(CURSOR_QTY_MAX * sizeof(*text_cursor->x));
  text_cursor->y   = malloc(CURSOR_QTY_MAX * sizeof(*text_cursor->y));
  editor_cursor_reset(text_cursor);

  return text_cursor;
}


void editor_cursor_destroy(Text_Cursor* text_cursor){
  free(text_cursor->pos);
  free(text_cursor->x);
  free(text_cursor->y);
  free(text_cursor);
}


// given pos, find xy at all indexes. shows a problem with 0/1 length lines?
void editor_cursor_find_xy(TextBuffer* text_buffer, Text_Cursor* text_cursor){

  // assume that incoming points are sorted - verify with assertions

  // advance until we find the xy for the first one
  // then keep advancing but look for the next one

  int line = 0;
  int sum = text_buffer->line_length[line];
  for (size_t index=0; index<text_cursor->qty; ++index){
    while (sum < text_cursor->pos[index]){ // add to the end of the line (after the cursor)
      line += 1;
      sum += text_buffer->line_length[line];
    }
    text_cursor->y[index] = line;
    text_cursor->x[index] = text_buffer->line_length[line] - (sum - text_cursor->pos[index]);
    printf("pos: %d --> (x,y) = (%d, %d)\n", text_cursor->pos[index], text_cursor->x[index], text_cursor->y[index]);
  }

}


size_t editor_cursor_quicksort_partition(int* list, size_t start, size_t end){
   // pivot value, from the middle of the array
   int pivot_index = (int) floor((start + end)/2 );
   int pivot = list[pivot_index];

   // left index
   size_t i = start - 1;

   // right index
   size_t j = end + 1;

   for(;;){
      // move the left index to the right (at least once) 
      // and while element at the left is less than the pivot
      do {
         ++i;
      } while(list[i] < pivot);

      // move the right index to the left at least once 
      // and while element at the right index is greater than the pivot
      do {
         --j;
      } while (list[j] > pivot);

      // if indices crossed, give up!
      if (i >= j){
         return j;
      }

      // swap
      int tmp = list[i];
      list[i] = list[j];
      list[j] = tmp;
   }

}

// quicksort thanks to wikipedia ! Hoare partition
void editor_cursor_quicksort(int* list, size_t start, size_t end){
   // if ((start >= 0) & (end >= 0) & (start < end)){
   if (start < end){

      // partition and get pivot index
      size_t pivot_loc = editor_cursor_quicksort_partition(list, start, end);

      // recursive break into subproblems
      editor_cursor_quicksort(list, start, pivot_loc);
      editor_cursor_quicksort(list, pivot_loc+1, end);
   }
  
   return;
}


// sort the cursors in descending order, then recompute xy coordinates
void editor_cursor_sort(TextBuffer* text_buffer, Text_Cursor* text_cursor){
  // sort pos using quicksort
  editor_cursor_quicksort(text_cursor->pos, 0, text_cursor->qty-1);

  // then process to recompute xy
  printf("result after sorting..\n");
  editor_cursor_find_xy(text_buffer, text_cursor);
}


// move one cursor by the given amount
void editor_cursor_move(TextBuffer* tb, Text_Cursor* tc, size_t index, int movedir){
  if (movedir == TEXTCURSOR_MOVE_DIR_RIGHT){
    if (tc->pos[index] < tb->length){
      // TODO if allowed by length
      tc->pos[index] += 1;
      tc->x[index] += 1;
      
      // wrap to next line
      if (tc->x[index] == tb->line_length[tc->y[index]]){
        tc->x[index] = 0;
        tc->y[index] += 1;
      }
    }
  }
  else if (movedir == TEXTCURSOR_MOVE_DIR_LEFT){
    if (tc->pos[index] > 0){
      tc->pos[index] -= 1;
      tc->x[index] -= 1;

      // wrap to previous line
      if(tc->x[index]  < 0){
        tc->y[index] -= 1;
        tc->x[index] = tb->line_length[tc->y[index]] - 1;
      }
    }
  }
  else if (movedir == TEXTCURSOR_MOVE_DIR_UP){
    if (tc->y[index] > 0){
      tc->y[index] -= 1;

      int x_delta = tc->x[index];
      // moving on to a shorter line
      if (tc->x[index] > tb->line_length[tc->y[index]]){
        tc->x[index] = tb->line_length[tc->y[index]]-1;
        tc->pos[index] -= x_delta + 1;
      }
      // moving onto a longer line
      else{
        x_delta += tb->line_length[tc->y[index]] - tc->x[index];
        tc->pos[index] -= x_delta;
      }
    }
  }
  else if (movedir == TEXTCURSOR_MOVE_DIR_DOWN){
    if (tc->y[index] < tb->lines){
      int x_delta = tb->line_length[tc->y[index]] - tc->x[index];
      tc->y[index] += 1;

      // moving onto a shorter line
      if (tc->x[index] > tb->line_length[tc->y[index]]){
        tc->x[index] = tb->line_length[tc->y[index]]-1;
      }
        
      tc->pos[index] += x_delta + tc->x[index];
    }
  }
  else if (movedir == TEXTCURSOR_MOVE_LINE_START){
    tc->pos[index] -= tc->x[index];
    tc->x[index] = 0;
  }
  else if (movedir == TEXTCURSOR_MOVE_LINE_END){
    int x_delta = tb->line_length[tc->y[index]] - tc->x[index] - 1;
    if (x_delta > 0){
      tc->pos[index] += x_delta;
      tc->x[index] += x_delta;
    }
  }

printf("move index %lu in direction %d\n", index, movedir);

  // TODO is this the best?
//editor_cursor_find_xy(tb, tc, index);
}


// throw out everything, load from a file and parse it
void editor_load_text(Task_Memory* task_memory, User_Memory* user_memory, TextBuffer* text_buffer, const char* filename, Text_Cursor* text_cursor){

  // open the file, create if not exist, use persmissions of current user
  FILE* fd = fopen(filename, "r"); 
  if (fd != NULL){
    char* text_cursor_loading = text_buffer->text;
    text_buffer->length = 0;
    do {
      *text_cursor_loading = fgetc(fd);
      ++text_cursor_loading;
      text_buffer->length += 1;
    } while(*(text_cursor_loading - 1) != EOF);
    fclose(fd);
    text_buffer->length -= 1;
    printf("loaded text of length %d\n", text_buffer->length);
    printf("text is '%.*s'\n", text_buffer->length, text_buffer->text);
  }
  else{
    printf("[WARNING] FILE '%s' DOES NOT EXIST, CREATING IT\n", filename);
    fd = fopen(filename, "w");
    fprintf(fd, " ");
    fclose(fd);

    // start an empty text buffer
    text_buffer->text[0] = ' ';
    text_buffer->length = 1;
  }

  // do an initial parse of the text information 
  for (size_t t=0; t<task_memory->allocation_total; ++t){
    task_memory->tasks[t].mode_edit = TRUE;
  }
  editor_parse_text(task_memory, user_memory, text_buffer, text_cursor);
  editor_find_line_lengths(text_buffer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* text_append_char(char* text, char new){
  *text = new;
  ++text;
  return text;
}


char* text_append_string(char* text, char* addition){
  int length = snprintf(NULL, 0, "%s", addition);
  memcpy(text, addition, length);
  text += length;
  return text;
}


// TODO rename to something better - "generate?"
void editor_text_from_data(Task_Memory* task_memory, TextBuffer* text_buffer, uint8_t all_tasks){
  char* text = text_buffer->text;

  // Fill the new one
  for (size_t t=0; t<task_memory->allocation_total; ++t){
    Task* task = task_memory->tasks + t;
    if (task->trash == FALSE){
      if ((task->mode_edit == TRUE) || (all_tasks == TRUE)){
        // task name
        memcpy(text, task->task_name, task->task_name_length);
        text += task->task_name_length;
        text = text_append_string(text, " {\n");

        // duration
        if ((task->schedule_constraints & SCHEDULE_CONSTRAINT_DURATION) > 0){
          text = text_append_string(text, "  duration: ");
          int length = snprintf(NULL, 0, "%ld", task->day_duration);
          int result = snprintf(text, length+1, "%ld", task->day_duration); // +1 due to how snprintf accounts for \0
          assert(result > 0);
          text += length;
          text = text_append_char(text, '\n');
        }
          
        // prereqs (dependency)
        if (task->prereq_qty > 0){
          text = text_append_string(text, "  prereq: ");

          for (size_t i=0; i<task->prereq_qty; ++i){
            memcpy(text, task->prereqs[i]->task_name, task->prereqs[i]->task_name_length);
            text += task->prereqs[i]->task_name_length;
            text = text_append_string(text, ", ");
          }
          text -= 2;
          text = text_append_char(text, '\n');
        }
        
        // users
        if (task->user_qty > 0){
          text = text_append_string(text, "  user: ");

          for (size_t u=0; u<task->user_qty; ++u){
            memcpy(text, task->users[u]->name, task->users[u]->name_length);
            text += task->users[u]->name_length;
            text = text_append_string(text, ", ");
          }
          text -= 2;
          text = text_append_char(text, '\n');
        }

        // fixed dates
        if ((task->schedule_constraints & SCHEDULE_CONSTRAINT_START) > 0){
          text = text_append_string(text, "  fixed_start: ");
          text = text_append_date(text, task->day_start);
          text = text_append_char(text, '\n');
        }
        if ((task->schedule_constraints & SCHEDULE_CONSTRAINT_END) > 0){
          text = text_append_string(text, "  fixed_end: ");
          text = text_append_date(text, task->day_end);
          text = text_append_char(text, '\n');
        }

        // color
        {
          text = text_append_string(text, "  color: ");
          int length = snprintf(NULL, 0, "%u", task->status_color);
          int result = snprintf(text, length+1, "%u", task->status_color);
          assert(result > 0);
          text += length;
          text = text_append_char(text, '\n');
        }

        // end this task
        text = text_append_string(text, "}\n");
      }
    }
  }
  text_buffer->length = text - text_buffer->text;

  if (text_buffer->length == 0){
    text_buffer->text[0] = ' ';
    text_buffer->length = 1;
  }
}


void text_buffer_save(TextBuffer* text_buffer, char* filename){

  FILE* fd = fopen(filename, "w");
  if (fd != NULL){
    fprintf(fd, "%.*s", text_buffer->length, text_buffer->text);
    fclose(fd);
    printf("[INFO] save successful.\n");
  }
  else{
    printf("[ERROR] could not open file '%s'.\n", filename);
  }
}


// find one string (the needle) in another string (they haystack)
char* strstr_n(char* haystack_start, size_t haystack_n, char* needle, size_t needle_n){
  printf("looking for %s...\n", needle);

  char* haystack = haystack_start; // current
  char* haystack_end = haystack_start + haystack_n;
  int done = 0;
  while (done == 0){
    // move the haystack to point at the first occurance of the first letter of the needle
    haystack = memchr(haystack, (int) needle[0], haystack_end - haystack);
    if (haystack == NULL){
      break;
    }

    // see if the rest of the word follows
    if (memcmp(haystack, needle, needle_n) == 0){
      done = 1;
      break;
    }
    else{
      ++haystack;
    }

    if (haystack == haystack_end){
      haystack = NULL;
      break;
    }
  }

  return haystack;
}

void editor_symbol_rename(Task_Memory* task_memory, User_Memory* user_memory, TextBuffer* text_buffer, Text_Cursor* text_cursor){
  printf("[SYMBOL RENAME] FUNCTION ACTIVATED**********************************\n");
  if (text_cursor->qty > 1){
    printf("[WARNING] CURRENT ENTITY BASED ONLY ON FIRST CURSOR [0]\n");
  }
 
  // force parsing of the text to update the cursor stuff
  editor_parse_text(task_memory, user_memory, text_buffer, text_cursor);

  char* keyword = NULL;
  int keyword_length = 0;

  // if renaming task...
  if (text_cursor->entity_type == TEXTCURSOR_ENTITY_TASK){
    printf("renaming task!\n");
    Task* task = (Task*) text_cursor->entity;

    // get the task name
    keyword = task->task_name;
    keyword_length = task->task_name_length;
    
    // mark all related tasks in edit mode
    for (size_t t=0; t<task->dependent_qty; ++t){
      task->dependents[t]->mode_edit = TRUE;
    }
  }
  else if (text_cursor->entity_type == TEXTCURSOR_ENTITY_USER){
    printf("renaming user\n");
    User* user = (User*) text_cursor->entity;

    // set the keyword as name
    keyword = user->name;
    keyword_length = user->name_length;

    // mark all related tasks in edit mode
    for (size_t t=0; t<user->task_qty; ++t){
      user->tasks[t]->mode_edit = TRUE;
    }
  }
  else if (text_cursor->entity_type == TEXTCURSOR_ENTITY_PREREQ){
    printf("renaming task by prereq reference!\n");
    Task* task = (Task*) text_cursor->entity;

    // get the task name
    keyword = task->task_name;
    keyword_length = task->task_name_length;
    
    // mark all related tasks in edit mode
    task->mode_edit = TRUE;
    for (size_t t=0; t<task->dependent_qty; ++t){
      task->dependents[t]->mode_edit = TRUE;
    }
  }

  // regenerate text..
  editor_text_from_data(task_memory, text_buffer, FALSE); 

  // now deploy the multi-cursors! search text for keyword, add a cursor at the end of each. move the original cursor
  // TODO how to handle keywords inside of other keywords?
  text_cursor->qty = 0;
  char* keyword_location = strstr_n(text_buffer->text, text_buffer->length, keyword, keyword_length);
  while (keyword_location != NULL){
    text_cursor->pos[text_cursor->qty] = (keyword_location - text_buffer->text) + keyword_length;
    keyword_location = strstr_n(keyword_location + keyword_length, text_buffer->length - text_cursor->pos[text_cursor->qty], 
                                keyword, keyword_length);
    text_cursor->qty += 1;
    assert(text_cursor->qty < CURSOR_QTY_MAX);
  }
  if (text_cursor->qty == 0){
    editor_cursor_reset(text_cursor);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int main(int argc, char* argv[]){
  // parse the input filename
  if (argc != 2){
    printf("[ERROR] must specify a schedule file\n");
    printf("USAGE   ./main.bin myschedule.json\n");
    return -1;
  }
  printf("using file %s\n", argv[1]);

  // Platform Init Window
  SDL_Window* win;
  SDL_Renderer* render;
  sdl_startup(&win, &render);

  int window_width, window_height;
  int running = 1;
  int viewport_active = VIEWPORT_EDITOR;
  // viewports, and dynamic sizing stuff
  SDL_Rect viewport_editor;
  SDL_Rect viewport_display;
  SDL_Rect viewport_display_body;
  SDL_Rect viewport_display_header;

  Task_Memory  task_memory_object;
  Task_Memory* task_memory = &task_memory_object;
  User_Memory  user_memory_object;
  User_Memory* user_memory = &user_memory_object;
  tasks_init(task_memory, user_memory);

  Schedule_Event_List* schedule_best = schedule_create();
  Schedule_Event_List* schedule_working = schedule_create();
  int schedule_solve_status = FAILURE;

  // TODO smooth scroll system
  // TODO error flagging / colors system; live syntax parsing
  TextBox editor_textbox;
  editor_textbox.color.r = 0; editor_textbox.color.g = 0; editor_textbox.color.b = 0; editor_textbox.color.a = 0xFF;
  editor_textbox.texture = NULL;
  Text_Cursor* text_cursor = editor_cursor_create();

  TextBox name_textbox;
  name_textbox.color.r = 0; name_textbox.color.g = 0; name_textbox.color.b = 0; name_textbox.color.a = 0xFF;
  name_textbox.texture = NULL;

  TextBuffer* text_buffer = editor_buffer_init();
  editor_load_text(task_memory, user_memory, text_buffer, argv[1], text_cursor); 
  schedule_solve_status = schedule_solve(task_memory, schedule_best, schedule_working);
  uint64_t day_project_start = schedule_best->day_start;

  // causes some overhead. can control with SDL_StopTextInput()
  SDL_StartTextInput();

  // DISPLAY VIEWPORT variables; list of tasks to plot, camera stuff
  // navigation among nodes, cursor system, selection system
  Task_Display* task_displays = (Task_Display*) malloc(TASK_DISPLAY_LIMIT * sizeof(Task_Display));
  size_t task_display_qty = 0;
  int display_pixels_per_day = 10; 
  int display_camera_y = 0;
  Task* display_task_active = NULL;

  uint8_t render_text = TRUE;
  uint8_t parse_text = TRUE;
  uint8_t display_selection_changed = FALSE; // TODO which is better to init?

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

    // DYNAMIC WINDOW RESIZING
    SDL_GetWindowSize(win, &window_width, &window_height);
    viewport_editor.x = 0;
    viewport_editor.y = 0;
    viewport_editor.w = window_width / 4;
    viewport_editor.h = window_height;
    viewport_display.x = viewport_editor.w;
    viewport_display.y = 0;
    viewport_display.w = window_width - viewport_editor.w;
    viewport_display.h = window_height;
    viewport_display_header.x = viewport_display.x;
    viewport_display_header.y = viewport_display.y;
    viewport_display_header.w = viewport_display.w;
    viewport_display_header.h = 40;
    viewport_display_body.x = viewport_display.x;
    viewport_display_body.y = viewport_display.y + viewport_display_header.h;
    viewport_display_body.w = viewport_display.w;
    viewport_display_body.h = viewport_display.h - viewport_display_header.h;

    editor_textbox.width_max = viewport_editor.w;
    name_textbox.width_max = window_width * 0.75;

    // INPUT
    SDL_Event evt;
    while (SDL_PollEvent(&evt) != 0){
      if (evt.type == SDL_QUIT){
        goto cleanup;
      }
      if (keybind_global_quit(evt) == TRUE){
        goto cleanup;
      }
      // SAVE
      if (keybind_global_file_save(evt) == TRUE){
        printf("[file op] save requested\n");
        // put into some temporary allocated buffer so not disrupting the current selection or editor buffer
        TextBuffer* save_buffer = editor_buffer_init();
        
        // regenerate text for all tasks
        editor_text_from_data(task_memory, save_buffer, TRUE);

        // save that text to the save file given by the filename on command line TODO
        text_buffer_save(save_buffer, argv[1]);
        
        // free the temporary text buffer
        editor_buffer_destroy(save_buffer);
      }
      // RELOAD
      if (keybind_global_file_reload(evt) == TRUE){
        printf("[file op] reload requested\n");
        // mark all tasks as being in editor
        for (size_t t=0; t<task_memory->allocation_total; ++t){
          if (task_memory->tasks[t].trash == FALSE){
            task_memory->tasks[t].mode_edit = TRUE;
          }
        }
        // load file contents into the editor text buffer, this will also parse the file
        editor_load_text(task_memory, user_memory, text_buffer, argv[1], text_cursor);
        parse_text = TRUE; // because load doesn't try a schedule solve
        render_text = 1;

        viewport_active = VIEWPORT_EDITOR;
        editor_cursor_reset(text_cursor);
     }

      if (keybind_viewport_mode_toggle(evt) == TRUE){
        if (viewport_active == VIEWPORT_DISPLAY){
          viewport_active = VIEWPORT_EDITOR;
          editor_cursor_reset(text_cursor);
          printf("switch to viewport editor\n");
          SDL_StartTextInput();
        }
        else if(viewport_active == VIEWPORT_EDITOR){
          viewport_active = VIEWPORT_DISPLAY;
          printf("switch to display viewport\n");
          SDL_StopTextInput();
        }
      }

      if (viewport_active == VIEWPORT_EDITOR){
        
        // special key input
        // TODO cursor management, underline corner style
        // TODO multi-cursor
        if (evt.type == SDL_KEYDOWN){
          if (evt.key.keysym.sym == SDLK_BACKSPACE && text_buffer->length > 0){
            for (size_t i=0; i<text_cursor->qty; ++i){
              // compensate for previous movement
              for (size_t j=0; j<i; ++j){
                text_cursor->pos[i] -= 1;
              }

              // actually remove the character
              char* text_dst = text_buffer->text + text_cursor->pos[i] - 1;
              char* text_src = text_dst + 1;
              char* text_end = text_buffer->text + text_buffer->length;
              memmove(text_dst, text_src, text_end-text_src); 

              --text_buffer->length;
              text_buffer->text[text_buffer->length] = '\0';
              text_buffer->line_length[text_cursor->y[i]] -= 1; // TODO what if line length is already zero??

              // now move this cursor left for the backspace action
              text_cursor->pos[i] -= 1;
            }
            render_text = 1;
            parse_text = TRUE;
          }
          else if( evt.key.keysym.sym == SDLK_DELETE && text_buffer->length > 0){
            for (size_t i=0; i<text_cursor->qty; ++i){
              // compensate for previous movement
              for (size_t j=0; j<i; ++j){
                text_cursor->pos[i] -= 1;
              }

              char* text_dst = text_buffer->text + text_cursor->pos[i];
              char* text_src = text_dst + 1;
              char* text_end = text_buffer->text + text_buffer->length;
              memmove(text_dst, text_src, text_end-text_src); 

              --text_buffer->length;
              text_buffer->text[text_buffer->length] = '\0';
              text_buffer->line_length[text_cursor->y[i]] -= 1; // TODO what if line length is already zero??
            }
            render_text = 1;
            parse_text = TRUE;
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
          else if (evt.key.keysym.sym == SDLK_RETURN){
            for (size_t i=0; i<text_cursor->qty; ++i){
              // compensate for previous movement
              for (size_t j=0; j<i; ++j){
                text_cursor->pos[i] += 1;
              }

              // move text to make space for inserting characters
              // TODO verify adding text at the end
              char* text_src = text_buffer->text + text_cursor->pos[i];
              char* text_dst = text_src + 1;
              char* text_end = text_buffer->text + text_buffer->length;
              memmove(text_dst, text_src, text_end - text_src);

              // actually add the character
              text_buffer->text[text_cursor->pos[i]] = '\n';
              ++text_buffer->length;
              text_cursor->pos[i] += 1;
            }
            render_text = 1;
            parse_text = TRUE;

            printf("[INSERT] RETURN\n"); 
          }
          else if(evt.key.keysym.sym == SDLK_LEFT){
            for (size_t i=text_cursor->qty; i>0; i--){
              editor_cursor_move(text_buffer, text_cursor, i-1, TEXTCURSOR_MOVE_DIR_LEFT);
            }
          }
          else if(evt.key.keysym.sym == SDLK_RIGHT){
            for (size_t i=text_cursor->qty; i>0; i--){
              editor_cursor_move(text_buffer, text_cursor, i-1, TEXTCURSOR_MOVE_DIR_RIGHT);
            }
          }
          else if (evt.key.keysym.sym == SDLK_UP){
            for (size_t i=text_cursor->qty; i>0; i--){
              editor_cursor_move(text_buffer, text_cursor, i-1, TEXTCURSOR_MOVE_DIR_UP);
            }
          }
          else if (evt.key.keysym.sym == SDLK_DOWN){
            for (size_t i=text_cursor->qty; i>0; i--){
              editor_cursor_move(text_buffer, text_cursor, i-1, TEXTCURSOR_MOVE_DIR_DOWN);
            }
          }
          else if (evt.key.keysym.sym == SDLK_HOME){
            for (size_t i=text_cursor->qty; i>0; i--){
              editor_cursor_move(text_buffer, text_cursor, i-1, TEXTCURSOR_MOVE_LINE_START);
            }
          }
          else if (evt.key.keysym.sym == SDLK_END){
            for (size_t i=text_cursor->qty; i>0; i--){
              editor_cursor_move(text_buffer, text_cursor, i-1, TEXTCURSOR_MOVE_LINE_END);
            }
          }
          else if (keybind_editor_multicursor_deselect(evt) == TRUE){
            text_cursor->qty = 1; 
            // TODO behavior choice
            size_t index = 0;
            printf("pos: %d --> (x,y) = (%d, %d)\n", text_cursor->pos[index], text_cursor->x[index], text_cursor->y[index]);
            editor_cursor_find_xy(text_buffer, text_cursor);
          }
          else if (keybind_editor_symbol_rename(evt) == TRUE){
            editor_symbol_rename(task_memory, user_memory, text_buffer, text_cursor);
            parse_text = TRUE;
            display_selection_changed = TRUE;
          }
          else if (evt.key.keysym.sym == SDLK_F4){ // TODO HACK start things
            text_cursor->pos[text_cursor->qty] = text_cursor->pos[text_cursor->qty-1] + 20;
            text_cursor->qty += 1;
            editor_cursor_sort(text_buffer, text_cursor);
          }
          else if (evt.key.keysym.sym == SDLK_F5){
            printf("updating xy coordinates of all cursors\n");
            editor_cursor_find_xy(text_buffer, text_cursor);
          }
          // F2 - rename symbol in edit mode
          // SHIFT+F2 - rename symbol even if not in edit mode
          // F3 - search stuff! can be smart scoping?

        } // keypress
        else if ((evt.type == SDL_TEXTINPUT) && !(SDL_GetModState() & KMOD_CTRL)){
          // assume cursors are sorted from soonest to latest in thee text
          for (size_t i=0; i<text_cursor->qty; ++i){
            assert(text_buffer->length < EDITOR_BUFFER_LENGTH);

            // update current cursor based on prior text growth. xy needs to be corrected later
            for (size_t j=0; j<i; ++j){
              printf("pre move\n");
              editor_cursor_move(text_buffer, text_cursor, i, TEXTCURSOR_MOVE_DIR_RIGHT);
            }
            int pos = text_cursor->pos[i]; // account for previous additions
            printf("adding character '%c' at %d\n", evt.text.text[0], pos); 
            // TODO fix first new character.. it needs a 

            // move text to make space for inserting characters
            // TODO verify adding text at the end
            char* text_src = text_buffer->text + pos; 
            char* text_dst = text_src + 1;
            char* text_end = text_buffer->text + text_buffer->length;
            memmove(text_dst, text_src, text_end - text_src);

            // actually add the character
            text_buffer->text[pos] = evt.text.text[0];
            ++text_buffer->length;
            text_buffer->line_length[text_cursor->y[i]] += 1;
            printf("final move right\n");
            editor_cursor_move(text_buffer, text_cursor, i, TEXTCURSOR_MOVE_DIR_RIGHT);
          }

          render_text = 1;
          parse_text = TRUE;

        }
        else if(evt.type == SDL_TEXTEDITING){
          // TODO handle SDL_TEXTEDITING for a temporary buffer to allow for more complex languages/character sets 
          assert(0);
        }

      } // viewport editor

      else if (viewport_active == VIEWPORT_DISPLAY){
        // zoom and camera motions
        if (keybind_display_camera_time_zoom_in(evt) == TRUE){
          display_pixels_per_day += 1;
          printf("zoom in\n"); 
        }
        else if (keybind_display_camera_time_zoom_out(evt) == TRUE){
          if (display_pixels_per_day > 1){
            display_pixels_per_day -= 1;
            printf("zoom out\n"); 
          }
        }
        else if (keybind_display_camera_time_scroll_up(evt) == TRUE){
          display_camera_y -= 3;
        }
        else if (keybind_display_camera_time_scroll_down(evt) == TRUE){
          display_camera_y += 3;
        }
        else if (keybind_display_camera_time_zoom_all(evt) == TRUE){
          display_camera_y = 0;
          display_pixels_per_day = (viewport_display_body.h ) / (schedule_best->day_duration);
        }

        else if (evt.type == SDL_MOUSEMOTION){
          // mouse state is within the entire window
          int mouse_x, mouse_y;
          SDL_GetMouseState(&mouse_x, &mouse_y);
          //printf("mouse: %d, %d\n", mouse_x, mouse_y);
        }
        else if (evt.type == SDL_MOUSEBUTTONDOWN){
          // event fires once even the mouse button continues to be held
          // (there is also SDL_MOUSEBUTTONUP event that could be used for box select stuff)
          int mouse_x, mouse_y;
          SDL_GetMouseState(&mouse_x, &mouse_y);

          // mouse relative to the viewport_display_body area
          mouse_x -= viewport_display_body.x;
          mouse_y -= viewport_display_body.y;
          
          // check if select anything
          int touched_anything = FALSE;
          for (size_t i=0; i<task_display_qty; ++i){
            if ((mouse_x > task_displays[i].local.x) && (mouse_x < task_displays[i].local.x + task_displays[i].local.w)){
              if ((mouse_y > task_displays[i].local.y) && (mouse_y < task_displays[i].local.y + task_displays[i].local.h)){
                display_task_active = task_displays[i].task;
                task_displays[i].task->mode_edit = TRUE;
                touched_anything = TRUE;
              }
            }
          }
          if (touched_anything == FALSE){
            for (size_t t=0; t<task_memory->allocation_total; ++t){
              display_task_active = NULL;
              task_memory->tasks[t].mode_edit = FALSE;
            }
          }
          display_selection_changed = TRUE;
        } // end processing mouse click

        else if (keybind_display_select_prereq_one(evt) == TRUE){
          for (size_t t=0; t<task_memory->allocation_total; ++t){
            if ((task_memory->tasks[t].trash == FALSE) && (task_memory->tasks[t].mode_edit == TRUE)){
                task_memory->temp_status[t] = TRUE;
            }
            else{
              task_memory->temp_status[t] = FALSE;
            }
          }

          for (size_t t=0; t<task_memory->allocation_total; ++t){
            if (task_memory->temp_status[t] == TRUE){
              for (size_t i=0; i<task_memory->tasks[t].prereq_qty; ++i){
                task_memory->tasks[t].prereqs[i]->mode_edit = TRUE;
              }
            }
          }
          display_selection_changed = TRUE;
        }
        else if (keybind_display_select_dependent_one(evt) == TRUE){
          for (size_t t=0; t<task_memory->allocation_total; ++t){
            if ((task_memory->tasks[t].trash == FALSE) && (task_memory->tasks[t].mode_edit == TRUE)){
                task_memory->temp_status[t] = TRUE;
            }
            else{
              task_memory->temp_status[t] = FALSE;
            }
          }

          for (size_t t=0; t<task_memory->allocation_total; ++t){
            if (task_memory->temp_status[t] == TRUE){
              for (size_t i=0; i<task_memory->tasks[t].dependent_qty; ++i){
                task_memory->tasks[t].dependents[i]->mode_edit = TRUE;
              }
            }
          }
          display_selection_changed = TRUE;
        }
        else if (keybind_display_select_prereq_all(evt) == TRUE){
          assert(0); // TODO implement
        }
        else if (keybind_display_select_dependent_all(evt) == TRUE){
          assert(0); // TODO implement
        }

        // deselect all
        else if (keybind_display_select_none(evt) == TRUE){
          for (size_t t=0; t<task_memory->allocation_total; ++t){
            if (task_memory->tasks[t].trash == FALSE){
              display_task_active = NULL;
              task_memory->tasks[t].mode_edit = FALSE;
            }
          }
        }

        else if (keybind_display_task_create_split(evt) == TRUE){
          // record what is edit mode now
          for (size_t t=0; t<task_memory->allocation_total; ++t){
            if ((task_memory->tasks[t].trash == FALSE) && (task_memory->tasks[t].mode_edit == TRUE)){
              task_memory->temp_status[t] = TRUE;
            }
            else{
              task_memory->temp_status[t] = FALSE;
            }
          }

          // now split each in edit mode!
          for (size_t t=0; t<task_memory->allocation_total; ++t){
            if (task_memory->temp_status[t] == TRUE){
              // duplicate this task to split it
              Task* base = task_memory->tasks+t;

              // find a new name, create the new task
              char* name_new = (char*) malloc(base->task_name_length + 4);
              int name_new_length;
              task_name_generate(task_memory, base, name_new, &name_new_length);
              Task* new = task_create(task_memory, name_new, name_new_length);
              free(name_new);

              // copy properties! 
              new->trash = FALSE;
              new->mode_edit = TRUE;
              new->schedule_done = FALSE;
              new->status_color = base->status_color;
              new->user_qty = base->user_qty;
              for (size_t u=0; u<new->user_qty; ++u){
                new->users[u] = base->users[u];
              }

              // figure out schedule constraints, start with duration
              new->schedule_constraints = SCHEDULE_CONSTRAINT_DURATION;
              new->day_duration = base->day_duration / 2;
              if (new->day_duration <= 0){
                new->day_duration = 1;
              }
              base->day_duration = new->day_duration;

              // accomodate other schedule constraints
              if ((base->schedule_constraints & SCHEDULE_CONSTRAINT_END) > 0){
                printf("schedule constraints were: %lu ... %lu\n", base->schedule_constraints, new->schedule_constraints);
                new->schedule_constraints |= SCHEDULE_CONSTRAINT_END;
                new->day_end = base->day_end;

                base->schedule_constraints ^= SCHEDULE_CONSTRAINT_END;
                printf("schedule constraints now: %lu ... %lu\n", base->schedule_constraints, new->schedule_constraints);
              }

              // make the dependencies work as intended
              new->prereq_qty = 1;
              new->prereqs[0] = base;

              // set dependents of the base to now depend on the new task instead
              // just update prereqs now, later scrub will make the dependents update properly
              for (size_t i=0; i<base->dependent_qty; ++i){
                Task* child = base->dependents[i];
                for (size_t j=0; j<child->prereq_qty; ++j){
                  if (child->prereqs[j] == base){
                    child->prereqs[j] = new;
                    break; // ASSUME that prereqs are not duplicated.. only need one
                  }
                }
              }
              parse_text = TRUE;
              display_selection_changed = TRUE;
            }
          } // end duplication of all tasks in edit mode
        } // end task split 

        else if (keybind_display_task_create_successor(evt)){
          // record what is edit mode now
          for (size_t t=0; t<task_memory->allocation_total; ++t){
            if ((task_memory->tasks[t].trash == FALSE) && (task_memory->tasks[t].mode_edit == TRUE)){
              task_memory->temp_status[t] = TRUE;
            }
            else{
              task_memory->temp_status[t] = FALSE;
            }
          }

          // now make next
          for (size_t t=0; t<task_memory->allocation_total; ++t){
            if (task_memory->temp_status[t] == TRUE){
              // look at this base class
              Task* base = task_memory->tasks + t;

              // find a new name, create the new task
              char* name_new = (char*) malloc(base->task_name_length + 4);
              int name_new_length;
              task_name_generate(task_memory, base, name_new, &name_new_length);
              Task* new = task_create(task_memory, name_new, name_new_length);
              free(name_new);

              // copy properties! 
              new->trash = FALSE;
              new->mode_edit = TRUE;
              new->schedule_done = FALSE;
              new->status_color = base->status_color;
              new->user_qty = base->user_qty;
              for (size_t u=0; u<new->user_qty; ++u){
                new->users[u] = base->users[u];
              }

              // schedule constraints
              new->schedule_constraints = SCHEDULE_CONSTRAINT_DURATION;
              new->day_duration = base->day_duration;

              // prereqs
              new->prereq_qty = 1;
              new->prereqs[0] = base;

              // mark
              parse_text = TRUE;
              display_selection_changed = TRUE;
            }
          }
        } // end task create successor
         
      } // viewport display

    } // end processing events
    // TODO navigate around the displayed nodes

    /////////////////////////////// PROCESSING //////////////////////////////////////////
    if (display_selection_changed == TRUE){
      printf("[STATUS] DISPLAY SELECTION CHANGED=============\n");
      for (size_t u=0; u<user_memory->allocation_total; ++u){
        user_memory->users[u].mode_edit = FALSE;
      }
      for (size_t t=0; t<task_memory->allocation_total; ++t){
        if (task_memory->tasks[t].trash == FALSE){
          if (task_memory->tasks[t].mode_edit == TRUE){
            for (size_t u=0; u<task_memory->tasks[t].user_qty; u++){
              task_memory->tasks[t].users[u]->mode_edit = TRUE;
            }
          }
        }
      }
      editor_text_from_data(task_memory, text_buffer, FALSE); 
    }

    if ((display_selection_changed == TRUE) || (parse_text == TRUE)){
      // figure out how many lines there are to render
      editor_find_line_lengths(text_buffer);
      
      editor_cursor_find_xy(text_buffer, text_cursor);
    }

    if (parse_text == TRUE){
      printf("[STATUS] TEXT PARSING REQUESTED--------------------------------------\n");

      // extract property changes from the text
      editor_parse_text(task_memory, user_memory, text_buffer, text_cursor);

      // PERFORM SCHEDULING!
      schedule_solve_status = schedule_solve(task_memory, schedule_best, schedule_working);
      day_project_start = schedule_best->day_start;
      
      // TODO insert some post scheduling work? to help with laying out things on screen
      // figure out and assign columns to each user
      if (task_memory->allocation_used > 0){
        User* users = user_memory->users;
        //// DRAW USER NAMES
        // TODO what is the right way to later connect user name to a column location (eventually, in pixels)

        // detect if there are any no-user tasks scheduled
        uint8_t orphaned_tasks = FALSE;
        for (size_t t=0; t<task_memory->allocation_total; ++t){
          if (task_memory->tasks[t].trash == FALSE){
            if (task_memory->tasks[t].user_qty == 0){
              orphaned_tasks = TRUE;
            }
          }
        }
        
        // figure out the user columns 
        int user_column_increment = viewport_display.w / (user_memory->allocation_used + 1);
        int nouser_column_center_px = user_column_increment/2;
        int user_column_loc = user_column_increment + user_column_increment / 2;
        size_t user_column_count = 1;
        for (size_t i=0; i<user_memory->allocation_total; ++i){ 
          if (users[i].trash == FALSE){
            users[i].column_index = user_column_count;
            users[i].column_center_px = user_column_loc;
            user_column_loc += user_column_increment;
            user_column_count += 1;
          }
        }

        // TODO pull out the username text box drawing
        // TODO column sorting?
        // have a list of pointers... so the shared 'column center pixel' is a pointer to which user, essentially.. and then those get shuffled to optimize?
        // basically.. want a dynamically updating thing so to change column is just changing one integer, not searching through N display_tasks to change every one

        // build the list of task blocks that have to be displayed
        // expect more display blocks than tasks since one task may be worked by several users
        for (size_t t=0; t<task_memory->allocation_total; ++t){
          task_memory->tasks[t].dependents_display_qty = 0;
        }
        
        task_display_qty = 0; // reset every loop
        for (size_t t=0; t<task_memory->allocation_total; ++t){
          Task* task = task_memory->tasks + t;
          if (task->trash == FALSE){
            if (task->user_qty > 0){  
              for (size_t u=0; u<task->user_qty; ++u){
                task_displays[task_display_qty].task = task;
                task_displays[task_display_qty].column = task->users[u]->column_center_px;
                for(size_t p=0; p<task->prereq_qty; ++p){ 
                  Task* prereq = task->prereqs[p];
                  prereq->dependents_display[prereq->dependents_display_qty] = &task_displays[task_display_qty];
                  prereq->dependents_display_qty +=1;
                }
                ++task_display_qty;
              }
            }
            else{
              task_displays[task_display_qty].task = task;
              task_displays[task_display_qty].column = nouser_column_center_px;
              for(size_t p=0; p<task->prereq_qty; ++p){ 
                Task* prereq = task->prereqs[p];
                prereq->dependents_display[prereq->dependents_display_qty] = &task_displays[task_display_qty];
                prereq->dependents_display_qty +=1;
              }
              ++task_display_qty;
            }
            assert(task_display_qty < TASK_DISPLAY_LIMIT);
          }
        }
      }
    } // end parse text & schedule

    /////////////////////////////// SECTION RENDERING //////////////////////////////////////////
    // clear screen
    SDL_SetRenderDrawColor(render, 0xFF, 0xFF, 0xFF, 0xFF); // chooose every frame now
    SDL_RenderClear(render);

    // editor viewport background
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
            if (text_cursor->qty == 1){
              if ((text_cursor->pos[0] >= line_start - text_buffer->text) && (text_cursor->pos[0] < line_end - text_buffer->text)){
                // draw a shaded background
                SDL_Rect cursor_line_background = {
                  .x = 0,
                  .y = line_height_offset,
                  .w = viewport_editor.w, 
                  .h = 20
                }; // TODO need to have a font line height as independent variable!
                SDL_SetRenderDrawColor(render, 230, 230, 230, 255);
                SDL_RenderFillRect(render, &cursor_line_background);
              }
            }

            for (size_t i=0; i<text_cursor->qty; ++i){
              if ((text_cursor->pos[i] >= line_start - text_buffer->text) && (text_cursor->pos[i] < line_end - text_buffer->text)){

              // find the location within the line?
              // TODO need to reference this to actual glyph widths!!
              // editor_cursor_pos_x = text_cursor->pos - (line_start - text_buffer->text);
              // editor_cursor_pos_y = text_lines - 1; // -1 for zero indexing

              SDL_Rect cursor_draw = {
                .x = text_cursor->x[i] * 10,
                .y = line_height_offset,
                .w = 3,
                .h = 20
              };
              SDL_SetRenderDrawColor(render, 50, 50, 80, 255);
              SDL_RenderFillRect(render, &cursor_draw);
              }
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


    SDL_RenderSetViewport(render, &viewport_display_header);

    // DRAW USERNAME HEADERS
    if (user_memory->allocation_used > 0){
      for(size_t i=0; i<user_memory->allocation_total; ++i){
        if (user_memory->users[i].trash == FALSE){
          sdlj_textbox_render(render, &name_textbox, user_memory->users[i].name);
          SDL_Rect src = {0, 0, name_textbox.width, name_textbox.height};
          SDL_Rect dst = {
            user_memory->users[i].column_center_px - name_textbox.width/2,
            5, 
            name_textbox.width, name_textbox.height};
          assert(SDL_RenderCopy(render, name_textbox.texture, &src, &dst) == 0);
        }
      }
    }

    // DRAW the schedule lines
    {
      SDL_RenderSetViewport(render, &viewport_display_body);
      SDL_SetRenderDrawColor(render, 0xA0, 0xA0, 0xA0, 0xFF);
      // three weeks, one day resolution
      int start_x = 0;
      int end_x = viewport_display_body.w;

      // show these lines relative to today
      time_t now_tmp;
      time(&now_tmp);
      int today_offset = (int) now_tmp / 86400; 
      today_offset -= (int) day_project_start;

      // days for 3 weeks
      int i=0;
      int limit1 = 7*3;
      int limit2 = 7*3 + limit1;
      while (i<200){
        int global_y = display_pixels_per_day*(i + today_offset);
        int local_y = global_y + display_camera_y;
        SDL_RenderDrawLine(render, start_x, local_y, end_x, local_y);

        if (i < limit1){ // increment 1 day
          i += 1;
        }
        else if(i < limit2){ // increment 1 week chunks
          i += 7;
        }
        else{ // increment 4 week chunks
          i += 7*4;
        }
      }
    }

    // TODO DRAW the keyboard cursor for display mode

    // DRAW THE TASKS AND RELATION CURVES
    if (task_memory->allocation_used > 0){      
      // parse the display list to assign pixel values and display
      SDL_RenderSetViewport(render, &viewport_display_body);

      for (size_t i=0; i<task_display_qty; ++i){
        Task_Display* td = task_displays + i;
        td->global.w = 180;
        td->global.x = td->column - td->global.w / 2;
        td->global.y = display_pixels_per_day*(td->task->day_start - day_project_start);
        td->global.h = display_pixels_per_day*(td->task->day_duration); // TODO account for weekends

        // now compute the local stuff given the camera location
        td->local.x = td->global.x;
        td->local.w = td->global.w;
        td->local.y = td->global.y + display_camera_y;
        td->local.h = td->global.h;

        // now display on screen!
        task_draw_box(render, td);
      }

      // now draw bezier curves!
      for (size_t i=0; i<task_display_qty; ++i){
        Task_Display* td = task_displays + i;
        Task* task = td->task;
         
        int sc = task->status_color;
        SDL_SetRenderDrawColor(render, status_colors[sc].r, status_colors[sc].g, status_colors[sc].b, status_colors[sc].a);

        // draw a line from this task to each of its plotted dependencies
        for (size_t j=0; j<task->dependents_display_qty; ++j){
          Task_Display* td_dep = task->dependents_display[j];

          int start_x = td->local.x + td->local.w/2;
          int start_y = td->local.y + td->local.h;
          int end_x = td_dep->local.x + td_dep->local.w/2;
          int end_y = td_dep->local.y;
          draw_dependency_curve(render, start_x, start_y, end_x, end_y);
          draw_dependency_curve(render, start_x+1, start_y, end_x+1, end_y);
          draw_dependency_curve(render, start_x-1, start_y, end_x-1, end_y);

        }
      }

    } // end if there are any tasks to draw

    // TODO graveyard for orphaned tasks (improper dependencies to be plotted, etc.)

    // TODO write better warning for schedule fail
    SDL_RenderSetViewport(render, &viewport_display);
    if (schedule_solve_status == FAILURE){
      SDL_Rect rect_errors;
      rect_errors.w = window_width;
      rect_errors.h = 15; 
      rect_errors.x = 0; 
      rect_errors.y = window_height - rect_errors.h;

      SDL_SetRenderDrawColor(render, 220, 0, 0, 255);
      SDL_RenderFillRect(render, &rect_errors);
    }
    
    //// UPDATE SCREEN
    SDL_RenderPresent(render);

    // reset for the next loop
    render_text = FALSE;
    parse_text = FALSE;
    display_selection_changed = FALSE;
  } // while forever


  cleanup:
  sdl_cleanup(win, render);
  tasks_free(task_memory, user_memory);
  editor_buffer_destroy(text_buffer);
  schedule_free(schedule_best); 
  schedule_free(schedule_working);
  free(task_displays); 
  editor_cursor_destroy(text_cursor);
 return 0;
}
