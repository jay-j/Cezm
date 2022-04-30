#include <stdint.h>

#define TASK_USERS_MAX 8
#define TASK_DEPENDENCIES_MAX 64

#define TASK_MODE_EDIT (1<<1)
#define TASK_MODE_EDIT_CURSOR (1<<2)
#define TASK_MODE_DISPLAY_SELECTED (1<<3)
#define TASK_MODE_DISPLAY_CURSOR (1<<4)

#define SCHEDULE_CONSTRAINT_START (1)
#define SCHEDULE_CONSTRAINT_DURATION (1<<1)
#define SCHEDULE_CONSTRAINT_END (1<<2)

// build a crazy hash table out of nodes? name -> id?
// how to smartly handle renaming? 
// TODO rename symbol button

// does the Task_Node need to be built of some property struct? 
// for each property to store information about itself (value vs list, list length, if it is unset, auto-calculated, or user set...)

typedef struct Task_Node{
  uint64_t task_id;
  char* task_name;
  uint16_t mode; // see TASK_MODE_
  uint8_t trash;

  uint16_t user_id[TASK_USERS_MAX];
  uint16_t user_qty;

  // TODO should this instead be pointers to those tasks?
  uint64_t dependents[TASK_DEPENDENCIES_MAX];
  uint16_t dependent_qty;

  uint64_t schedule_constraints;
  uint64_t day_start;
  uint64_t day_duration;
  uint64_t day_end;

  uint8_t status_color;
  uint16_t subsystem_id;

  // DERIVED VARIABLES BELOW THIS LINE
  // TODO should this instead be pointers to those other tasks? 
  uint64_t prerequisites[TASK_DEPENDENCIES_MAX];
  uint16_t prerequisite_qty;
  
  // WORKING PROPERTIES
  // selected by cursor?

} Task_Node;

typedef struct User{
  char* name;
  uint8_t trash; 
  uint8_t mode_edit;
} User;

// track the quantity of activities created, to just increment forever. don't worry about re-use and abandoning old numbers
// how to keep memory use efficient? don't care about (un)mallocing new items since this is infrequent? 
// why use ids to store things vs. using pointers? 
//   import/export process will have to convert


// hash table lookup map to connect name with pointer?

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
