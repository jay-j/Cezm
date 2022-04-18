#include <stdint.h>

#define ACTIVITY_USERS_MAX 8
#define ACTIVITY_DEPENDENCIES_MAX 64

#define SCHEDULE_CONSTRAINT_START (1)
#define SCHEDULE_CONSTRAINT_DURATION (1<1)
#define SCHEDULE_CONSTRAINT_END (1<2)

// build a crazy hash table out of nodes? name -> id?
// how to smartly handle renaming? 
// TODO rename symbol button

typedef struct Activity_Node{
  uint16_t activity_id;
  char* activity_name;

  uint16_t user_id[ACTIVITY_USERS_MAX];
  uint16_t user_qty;

  uint16_t dependents[ACTIVITY_DEPENDENCIES_MAX];
  uint16_t dependent_qty;

  uint64_t schedule_constraints;
  uint64_t day_start;
  uint64_t day_duration;
  uint64_t day_end;

  uint8_t status_color;
  uint16_t subsystem_id;

  // DERIVED VARIABLES BELOW THIS LINE
  uint16_t prerequisites[ACTIVITY_DEPENDENCIES_MAX];
  uint16_t prerequisite_qty;

} Activity_Node;


