#include "profile_trap.h"

TrapInfo profile_trap_setup(double profile_speed, double profile_acceleration){
    TrapInfo trap;
    trap.profile_speed = profile_speed;
    trap.profile_acceleration = profile_acceleration;
    trap.current_velocity = 0;
    trap.current_position = 0;
    trap.current_acceleration = 0;
    return trap;
}

inline static double profile_trap_sign(double value){
    return (1.0 - 2.0*(value < 0));
}

inline static double profile_trap_abs(double value){
    return value*profile_trap_sign(value);
}

// TODO write branchless variant
inline static double profile_trap_limit(double value, double low, double high){
    value = value > high ? high : value;
    value = value < low ? low : value;
    return value;
}

double profile_trap_smooth(TrapInfo* trap, double goal, double dt){
    // close to goal - stay!
    if ((profile_trap_abs(goal - trap->current_position) < 0.5*trap->profile_acceleration*dt*dt)
        & (profile_trap_abs(trap->current_velocity) <= trap->profile_acceleration*dt)) {
        trap->current_position = goal;
        trap->current_velocity = 0;
        trap->current_acceleration = 0;
    }

    // far from goal - move! 
    else{
        // distance and direction to get to end goal
        double direction_to_goal = profile_trap_sign(goal - trap->current_position);

        // where do you end up if you try to stop right now?  
        double stop_time = profile_trap_abs(trap->current_velocity) / trap->profile_acceleration;
        double stop_position = 0.5*(-1.0*direction_to_goal*trap->profile_acceleration)*stop_time*stop_time
             + trap->current_velocity*stop_time + trap->current_position;

        // perform correction for quantized time. more important at coarse quantization
        double stop_steps = stop_time / dt;
        double stop_distance_correction = stop_steps*0.5*trap->profile_acceleration*dt*dt;
        stop_position += profile_trap_sign(trap->current_velocity)*stop_distance_correction;

        double stop_position_error = stop_position - goal;

        // accelerate = -1 on final approach, +1 if falling short of the goal
        trap->current_acceleration = -1 + 2*(stop_position_error*direction_to_goal < 0);

        // adjust velocity, limited
        trap->current_velocity = profile_trap_limit(trap->current_velocity + trap->profile_acceleration*direction_to_goal*trap->current_acceleration*dt, 
                                                    -trap->profile_speed, trap->profile_speed);
     
        // increment position target
        trap->current_position = trap->current_position + trap->current_velocity*dt;
    }

    return trap->current_position;
}
