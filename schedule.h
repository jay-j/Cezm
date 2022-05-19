#include <stdint.h>
#include "utilities-c/hash_lib/hashtable.h"

#define FALSE 0
#define TRUE 1
#define SUCCESS 2
#define FAILURE 3

// maximum possible number of tasks and users hardcoded since I don't have a hashtable-resizing function
#define HT_TASKS_MAX 8192
#define HT_USERS_MAX 1024

// a single task may be worked by up to 8 users
#define TASK_USERS_MAX 8
#define TASK_DEPENDENCIES_MAX 64

#define TASK_MODE_EDIT (1<<1)
#define TASK_MODE_EDIT_CURSOR (1<<2)
#define TASK_MODE_DISPLAY_SELECTED (1<<3)
#define TASK_MODE_DISPLAY_CURSOR (1<<4)

#define SCHEDULE_CONSTRAINT_DURATION (1)
#define SCHEDULE_CONSTRAINT_START (1<<1)
#define SCHEDULE_CONSTRAINT_END (1<<2)
#define SCHEDULE_CONSTRAINT_NOSOONER (1<<3)

// a user may be assigned a maximum of 1024 tasks
#define USER_TASKS_MAX 1024

typedef struct Task Task;

typedef struct User{
  char* name;
  uint8_t trash; // if TRUE, OK to recycle this object
  uint8_t mode_edit;
  uint8_t visited; // if TRUE, have seen this user this round

  // network properties
  Task* tasks[USER_TASKS_MAX];
  size_t task_qty;

  // display properties
  uint32_t column_center_px;
  size_t column_index; // display column
} User;

typedef struct User_Memory{
  User* users;
  HashTable* hashtable;
  size_t allocation_total;
  size_t allocation_used;
  size_t last_created;
  uint8_t* editor_visited;
} User_Memory;

// how to smartly handle renaming? TODO rename symbol button

struct Task{
  char* task_name;
  uint8_t trash;
  uint8_t mode_edit;
  uint8_t mode_display_selected;

  User* users[TASK_USERS_MAX];
  size_t user_qty;

  Task* prereqs[TASK_DEPENDENCIES_MAX];
  size_t prereq_qty;

  uint64_t schedule_constraints;
  uint64_t day_start;
  uint64_t day_duration;
  uint64_t day_end;

  uint8_t status_color;
  uint16_t subsystem_id;

  // DERIVED VARIABLES BELOW THIS LINE
  Task* dependents[TASK_DEPENDENCIES_MAX];
  size_t dependent_qty;
  
  // WORKING PROPERTIES
  // selected by cursor?
  uint8_t schedule_done;
};

typedef struct Task_Memory{
  Task* tasks;
  HashTable* hashtable;
  size_t allocation_total;
  size_t allocation_used;
  size_t last_created;
  uint8_t* editor_visited;
} Task_Memory;


SDL_Color status_colors[10];
void status_color_init(){
  // grey unknown
  status_colors[0].r = 150;
  status_colors[0].g = 150;
  status_colors[0].b = 150;

  // deep red
  status_colors[1].r = 192;
  status_colors[1].g = 0;
  status_colors[1].b = 0;

  // bright red
  status_colors[2].r = 255;
  status_colors[2].g = 0;
  status_colors[2].b = 0;

  // orange
  status_colors[3].r = 255;
  status_colors[3].g = 192;
  status_colors[3].b = 00;

  // yelllow
  status_colors[4].r = 255;
  status_colors[4].g = 255;
  status_colors[4].b = 0;

  // light green
  status_colors[5].r = 146;
  status_colors[5].g = 208;
  status_colors[5].b = 80;

  // dark green
  status_colors[6].r = 0;
  status_colors[6].g = 176;
  status_colors[6].b = 80;

  // cyan blue
  status_colors[7].r = 0;
  status_colors[7].g = 176;
  status_colors[7].b = 240;

  // dark blue
  status_colors[8].r = 0;
  status_colors[8].g = 112;
  status_colors[8].b = 192;

  // purple
  status_colors[9].r = 112;
  status_colors[9].g = 48;
  status_colors[9].b = 160;

  for (size_t i=0; i<10; ++i){
    status_colors[i].a = 255;
  }
}

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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO how to make this kind of model deal with uncertainty? 
// depth first.. 
// search is an array. elements are [[date, task_init], [date, task init], [date, task init] ... ]
// can start an arbitrary number of tasks on the same day
// keep track of two paths. one is the current (WIP) path. the other is the best solution path found to date. best being shortest total duration

typedef struct Schedule_Event {
  uint64_t date;
  Task* task;
} Schedule_Event;

typedef struct Schedule_Event_List {
  size_t qty;
  size_t qty_max;
  uint64_t day_start;
  uint64_t day_end;
  uint64_t day_duration;
  uint8_t solved;
  Schedule_Event* events;
} Schedule_Event_List;


Schedule_Event_List* schedule_create(){
  Schedule_Event_List* schedule = (Schedule_Event_List*) malloc(sizeof(Schedule_Event_List));
  schedule->qty = 0;
  schedule->qty_max = 64;
  schedule->events = (Schedule_Event*) malloc(schedule->qty_max * sizeof(Schedule_Event));
  schedule->solved = FALSE;
  return schedule;
}

void schedule_memory_management(Schedule_Event_List* schedule){
  if (schedule->qty >= schedule->qty_max){
    printf("[CAUTION] SCHEDULE MEMORY MANAGEMENT ACTIVATED, INCREASING MEMORY ALLOCATIONS\n");
    schedule->qty_max *= 1.5;
    schedule->events = (Schedule_Event*) realloc(schedule->events, schedule->qty_max * sizeof(Schedule_Event));
  }
}

void schedule_free(Schedule_Event_List* schedule){
  free(schedule->events);
  free(schedule);
}


// copy the schedule from src to dst. namely for use in saving the current best schedule
void schedule_copy(Schedule_Event_List* dst, Schedule_Event_List* src){
  Schedule_Event* events_tmp = dst->events;
  memcpy(dst, src, sizeof (Schedule_Event_List));
  dst->events = events_tmp;
  memcpy(dst->events, src->events, sizeof( Schedule_Event) * dst->qty_max);
}


void schedule_calculate_duration(Schedule_Event_List* schedule, Task_Memory* task_memory){
  uint64_t day_earliest = SIZE_MAX;
  uint64_t day_latest = 0;

  for (size_t t=0; t<task_memory->allocation_total; ++t){
    Task* task = task_memory->tasks + t;
    if (task->trash == FALSE){
      if (task->day_start < day_earliest){
        day_earliest = task->day_start;
      }
      if (task->day_end > day_latest){
        day_latest = task->day_end;
      }
    }
  }
  schedule->day_start = day_earliest;
  schedule->day_end = day_latest;
  schedule->day_duration = day_latest - day_earliest;
}


// TODO return the number of conflicting days? e.g. the number of days you need to move the task for it to be OK to schedule
int schedule_conflict_detect(Task* proposed_task){
  int conflict_detected = FALSE;

  // look at the users of the task being scheduled
  for (size_t u=0; u<proposed_task->user_qty; ++u){
    User* user = proposed_task->users[u];

    // search through every (other) task assigned to those users
    for (size_t t=0; t<user->task_qty; ++t){
      Task* scheduled_task = user->tasks[t];

      if (scheduled_task->schedule_done == TRUE){
        // detect if there is any conflict between the proposed task and previously scheduled tasks
        if (proposed_task->day_start > scheduled_task->day_start){
          if (proposed_task->day_start <= scheduled_task->day_end){
            conflict_detected = TRUE;
          }
        }
        else if (proposed_task->day_start < scheduled_task->day_start){
          if (proposed_task->day_end >= scheduled_task->day_start){
            conflict_detected = TRUE;
          }
        }
        else{
          conflict_detected = TRUE;
        }
      }
    }
  }
   
  return conflict_detected;
}


int schedule_task_push(Schedule_Event_List* schedule_working, Task* task, int schedule_shift_dir){
  // figure out when this task is getting scheduled
  // depends on whether it is added by dependency or prerequisite. EXCEPT for user busyness!!
  // if added by prerequisite.. search forwards. from prerequisite day end to the earliest point when all users are available
  // if added by dependency... search backwards. 
  // need to make sure the entire block of user time is available
  // push the task farther in the scheduling direction until conflict is resolved? one day at a time
  //
  // only add a task to ready once all of its prereqs or dependents are scheduled. add to ready with a suggested date
  // use task->schedule_seek_direction to know which way to try and adjust a task to make it work. 
  // still need to detect if the new task placement becomes impossible  TODO

  // try and find a time to put this task
  // guess a time based on earliest/latest possible from the prereq/dependent list
  uint64_t start;
  if (schedule_shift_dir == 1){ // prerequisites have been met (schedule after)
    start = 0;
    // first start date is [latest prereq end date] + 1
    for (size_t t=0; t<task->prereq_qty; ++t){
      if (start < task->prereqs[t]->day_end + 1){
        start = task->prereqs[t]->day_end + 1;
      }
    }
  }
  else if (schedule_shift_dir == -1){ // schedule before
    start = SIZE_MAX - task->day_duration - 2;
    // latest possible end date is [earliest dependent] - 1
    for (size_t t=0; t<task->dependent_qty; ++t){
      if (start + task->day_duration - 1 >= task->dependents[t]->day_start){ // TODO was task->day_duration + 1
        start = task->dependents[t]->day_start - task->day_duration;
      }
    }
  }
  else{
    printf("schedule error, invalid shift direction\n");
    assert(0);
  }
  //printf("[SCHEDULER] initial guess puts task %s at day %lu - %lu\n", task->task_name, start, start+task->day_duration);

  // while scheduling conflict exists shift task in direction indicated by schedule_shift_dir
  task->day_start = start;
  task->day_end = task->day_start + task->day_duration - 1;

  size_t loop_counter = 0;
  while (schedule_conflict_detect(task) == TRUE){
    //printf("  conflict adjustment...\n");
    task->day_start += schedule_shift_dir;
    task->day_end = task->day_start + task->day_duration - 1;

    // TODO handle detect infinite loop problems??
    if (loop_counter > 1e4){
      printf("[WARNING] scheduling task shift loop counter exceeded\n");
      return FAILURE;
    }

    // verify that prereqs and dependencies are still being met
    if (schedule_shift_dir > 0){
      // check against scheduled dependents
      for (size_t t=0; t<task->dependent_qty; ++t){
        if (task->dependents[t]->schedule_done == TRUE){
          if (task->day_end >= task->dependents[t]->day_start){
            return FAILURE;
          }
        }
      }
    }
    else{
      // check against scheduled prerequisites
      for (size_t t=0; t<task->prereq_qty; ++t){
        if (task->prereqs[t]->schedule_done == TRUE){
          if (task->day_start <= task->prereqs[t]->day_end){
            return FAILURE;
          }
        }
      }
    }
  }

  // store the task solution so it can be recreated later out of the best task
  schedule_working->qty += 1;
  schedule_memory_management(schedule_working);
  schedule_working->events[schedule_working->qty-1].task = task;
  schedule_working->events[schedule_working->qty-1].date = task->day_start;
  task->schedule_done = TRUE;

  return SUCCESS;
}


// remove the last value from the list of scheduled tasks
int schedule_task_pop(Schedule_Event_List* schedule_working){
  assert(schedule_working->qty > 0);
  schedule_working->qty -= 1;
  Task* task = schedule_working->events[schedule_working->qty].task;
  task->schedule_done = FALSE;
  return SUCCESS;
}


// no island tasks allowed.. every task must either have fixed_i or a prereq
// how do you know when you are done? when all non trash tasks are scheduled
// how do you know when to give up? TODO
void schedule_solve_iter(Task_Memory* task_memory, Schedule_Event_List* schedule_best, Schedule_Event_List* schedule_working){
  // quit when all tasks have been scheduled
  if (task_memory->allocation_used - schedule_working->qty == 0){
    //printf("[SCHEDULER] ALL TASKS SCHEDULED - SUCCESS!\n");
    
    // check for and save best schedule 
    schedule_working->solved = TRUE; 
    schedule_calculate_duration(schedule_working, task_memory);

    if (schedule_best->solved == FALSE){
      //printf("[SCHEDULER] FIRST SOLVE\n");
      schedule_copy(schedule_best, schedule_working);
    }
    else if (schedule_working->day_duration < schedule_best->day_duration){
      //printf("[SCHEDULER] A BETTER SOLVE THAN BEFORE :)\n");
      schedule_copy(schedule_best, schedule_working);
    }
    else{
      // not a winner
      //printf("[SCHEDULER] A WORSE SOLVE THAN BEFORE :(\n");
    }

    return; 
  }

  Task* task;
  for (size_t t=0; t<task_memory->allocation_total; ++t){
    task = task_memory->tasks+t;

    if ((task->trash == FALSE) & (task->schedule_done == FALSE)){
      //printf("[SCHEDULER] considering task '%s'..\n", task->task_name);
      int schedule_shift_dir = 0;

      // detect if all dependents are scheduled
      if (task->dependent_qty > 0){
        size_t dependents_scheduled = 0;
        for (size_t i=0; i<task->dependent_qty; ++i){
          if (task->dependents[i]->schedule_done == TRUE){
            ++dependents_scheduled;
          }
        }
        if (dependents_scheduled == task->dependent_qty){
          //printf("       all dependents for %s are scheduled.\n", task->task_name);
          schedule_shift_dir = -1;
        }
      }
      // detect if all prereqs are scheduled
      if (task->prereq_qty > 0){
        size_t prereqs_scheduled = 0;
        for (size_t i=0; i<task->prereq_qty; ++i){
          if (task->prereqs[i]->schedule_done == TRUE){
            ++prereqs_scheduled;
          }
        }
        if (prereqs_scheduled == task->prereq_qty){
          //printf("       all prereqs for %s are scheduled.\n", task->task_name);
          schedule_shift_dir = 1;
        }
      }


      // if either.. then try scheduling this task
      if (schedule_shift_dir != 0){
        //printf("       adding to the schedule\n");
        int pushed = schedule_task_push(schedule_working, task_memory->tasks+t, schedule_shift_dir);
        if (pushed == FAILURE){ // is this the right option? will there be an infinite loop?
          continue;
        }

        // recursion
        schedule_solve_iter(task_memory, schedule_best, schedule_working);

        //printf("   back up a level\n");

        // if you come out of that.. then that path was no good or looking for an alternate solution
        schedule_task_pop(schedule_working);
      }
    }
  }


  // TODO island detection....

}


// scheduling algorithm built around having at least one fixed start/end task per task island
int schedule_solve(Task_Memory* task_memory, Schedule_Event_List* schedule_best, Schedule_Event_List* schedule_working){
  Task* tasks = task_memory->tasks;
  // reset previous search efforts
  schedule_best->qty = 0;
  schedule_working->qty = 0;
  schedule_best->solved = FALSE;
  schedule_working->solved = FALSE;

  // clear out previous scheduling results
  for (size_t t=0; t<task_memory->allocation_total; ++t){
    tasks[t].schedule_done = FALSE;
  }

  // pre-process some constraints
  for (size_t t=0; t<task_memory->allocation_total; ++t){
    if ((tasks[t].schedule_constraints & (SCHEDULE_CONSTRAINT_END | SCHEDULE_CONSTRAINT_START)) > 0){
      // schedule this fixed constraint task
      printf("[SCHEDULER] task %s is a locked schedule task of type %lu\n", tasks[t].task_name, tasks[t].schedule_constraints);
      tasks[t].schedule_done = TRUE;
      schedule_working->events[schedule_working->qty].task = tasks + t;

      if ((tasks[t].schedule_constraints & SCHEDULE_CONSTRAINT_END) > 0){
        tasks[t].day_start = tasks[t].day_end - tasks[t].day_duration + 1;
      }
      else if ((tasks[t].schedule_constraints & SCHEDULE_CONSTRAINT_START) > 0){
        tasks[t].day_end = tasks[t].day_start + tasks[t].day_duration - 1;
      }
      schedule_working->events[schedule_working->qty].date = tasks[t].day_start;
      //printf("  schedule for %lu to %lu\n", tasks[t].day_start, tasks[t].day_end);

      schedule_working->qty += 1;
    }
  }

  printf("[SCHEDULER] after constraints, have %lu tasks to schedule\n", task_memory->allocation_used - schedule_working->qty);

  // grow in all direction from fixed_start and fixed_end tasks? need to have dependent AND prereq data
  schedule_solve_iter(task_memory, schedule_best, schedule_working);

  // TODO fail if impossible to satisfy prerequisite chain; if start date is earlier than a scheduled end date for task X

  //printf("final best schedule: \n");

  if (schedule_best->solved == TRUE){
    for(size_t e=0; e<schedule_best->qty; ++e){
      schedule_best->events[e].task->day_start = schedule_best->events[e].date;
    }
    return SUCCESS;
  }
  else{
    return FAILURE;
  }

}
