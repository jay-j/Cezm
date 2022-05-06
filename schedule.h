#include <stdint.h>

#define FALSE 0
#define TRUE 1

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

#define SCHEDULE_CONSTRAINT_START (1)
#define SCHEDULE_CONSTRAINT_DURATION (1<<1)
#define SCHEDULE_CONSTRAINT_END (1<<2)
#define SCHEDULE_CONSTRAINT_NOSOONER (1<<3)

// a user may be assigned a maximum of 1024 tasks
#define USER_TASKS_MAX 1024

typedef struct Task_Node Task_Node;

typedef struct User{
  char* name;
  uint8_t trash; // if TRUE, OK to recycle this object
  uint8_t mode_edit;
  uint8_t visited; // if TRUE, have seen this user this round

  // network properties
  Task_Node* tasks[USER_TASKS_MAX];
  size_t task_qty;

  // display properties
  uint32_t column_center_px;
  size_t column_index; // display column
} User;

// how to smartly handle renaming? TODO rename symbol button

struct Task_Node{
  char* task_name;
  uint8_t trash;
  uint8_t mode_edit;

  User* users[TASK_USERS_MAX];
  size_t user_qty;

  Task_Node* dependents[TASK_DEPENDENCIES_MAX];
  size_t dependent_qty;

  uint64_t schedule_constraints;
  uint64_t day_start;
  uint64_t day_duration;
  uint64_t day_end;

  uint8_t status_color;
  uint16_t subsystem_id;

  // DERIVED VARIABLES BELOW THIS LINE
  Task_Node* prerequisites[TASK_DEPENDENCIES_MAX];
  size_t prerequisite_qty;
  
  // WORKING PROPERTIES
  // selected by cursor?

};

// track the quantity of activities created, to just increment forever. don't worry about re-use and abandoning old numbers
// how to keep memory use efficient? don't care about (un)mallocing new items since this is infrequent? 
//   import/export process will have to convert



SDL_Color status_colors[10];
void status_color_init(){
  status_colors[0].r = 150;
  status_colors[0].g = 150;
  status_colors[0].b = 150;

  status_colors[1].r = 150;
  status_colors[1].g = 0;
  status_colors[1].b = 0;

  status_colors[2].r = 255;
  status_colors[2].g = 0;
  status_colors[2].b = 0;

  // orange
  status_colors[3].r = 245;
  status_colors[3].g = 180;
  status_colors[3].b = 50;

  // yelllow
  status_colors[4].r = 245;
  status_colors[4].g = 230;
  status_colors[4].b = 70;

  // light green
  status_colors[5].r = 75;
  status_colors[5].g = 245;
  status_colors[5].b = 70;

  // dark green
  status_colors[6].r = 0;
  status_colors[6].g = 100;
  status_colors[6].b = 5;

  // cyan blue
  status_colors[7].r = 0;
  status_colors[7].g = 230;
  status_colors[7].b = 255;

  // dark blue
  status_colors[8].r = 0;
  status_colors[8].g = 50;
  status_colors[8].b = 200;

  // purple
  status_colors[9].r = 50;
  status_colors[9].g = 0;
  status_colors[9].b = 140;

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

// how does scheduling work? 
// have a graph structure problem
// with infinite possible islands...
// 'topological sort' algorithm
// align all nodes in a line such that all dependencies point to the left
// danger - circular dependencies
// can I write as a matrix problem? 
// UNKNOWNS: when does each task start? (for tasks without fixed end or start)
//
// with differing costs.. seems like a path planning type algorithm is needed
// construct a date network? at each date look at available tasks to start working on and check which of all is best?
// envision fixed start and fixed end tasks are obstacles in this path
// how to make this kind of model deal with uncertainty? 
// depth first.. 
// search is an array. elements are [[date, task_init], [date, task init], [date, task init] ... ]
// can start an arbitrary number of tasks on the same day
// keep track of two paths. one is the current (WIP) path. the other is the best solution path found to date. best being shortest total duration
//

typedef struct Schedule_Event {
  uint64_t date;
  Task_Node* task;
} Schedule_Event;

typedef struct Schedule_Event_List {
  size_t qty;
  size_t qty_max;
  uint8_t solved;
  struct Schedule_Event* events;
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


int schedule_solve(Task_Node* tasks, Schedule_Event_List* schedule_best, Schedule_Event_List* schedule_working){
  // reset previous search efforts
  schedule_best->qty = 0;
  schedule_working->qty = 0;

  for (size_t t=0; t<task_allocation_total; ++t){
    if (tasks[t].dependent_qty == 0){
      // TODO put it on the open list!
    }
  }
  // find tasks with no dependencies and fixed_start constraints to initiate the search
  // for each of these....

  // list active tasks?
  // store user free/busy state? 
  // calculate actual start and end dates for the task. don't overwrite given constraints
  
  // advance day by one, try to see if there are any tasks which can now be started
  // maintain an 'active' list of tasks that can be attempted. 


  return 0;
}
