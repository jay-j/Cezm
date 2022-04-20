#include <stdint.h>

#define TASK_USERS_MAX 8
#define TASK_DEPENDENCIES_MAX 64

#define SCHEDULE_CONSTRAINT_START (1)
#define SCHEDULE_CONSTRAINT_DURATION (1<1)
#define SCHEDULE_CONSTRAINT_END (1<2)

// build a crazy hash table out of nodes? name -> id?
// how to smartly handle renaming? 
// TODO rename symbol button

typedef struct Task_Node{
  uint64_t task_id;
  char* task_name;

  uint16_t user_id[TASK_USERS_MAX];
  uint16_t user_qty;

  uint64_t dependents[TASK_DEPENDENCIES_MAX];
  uint16_t dependent_qty;

  uint64_t schedule_constraints;
  uint64_t day_start;
  uint64_t day_duration;
  uint64_t day_end;

  uint8_t status_color;
  uint16_t subsystem_id;

  // DERIVED VARIABLES BELOW THIS LINE
  uint64_t prerequisites[TASK_DEPENDENCIES_MAX];
  uint16_t prerequisite_qty;

} Task_Node;


// track the quantity of activities created, to just increment forever. don't worry about re-use and abandoning old numbers
// how to keep memory use efficient? don't care about (un)mallocing new items since this is infrequent? 
// why use ids to store things vs. using pointers? 
//   import/export process will have to convert


// hash table lookup map to connect name with pointer?

Task_Node task_node_create();

void task_node_destroy();
