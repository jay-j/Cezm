#ifndef LIB_TRAP_H
#define LIB_TRAP_H

typedef struct {
    double profile_acceleration;
    double profile_speed;
    double current_velocity;
    double current_position;
    double current_acceleration;
} TrapInfo;


TrapInfo profile_trap_setup(double profile_speed, double profile_acceleration);

double profile_trap_smooth(TrapInfo* trap, double goal, double dt);


#endif
