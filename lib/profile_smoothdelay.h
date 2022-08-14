#ifndef SMOOTH_DELAY_H
#define SMOOTH_DELAY_H

#include <stdio.h>
#include <stdlib.h>

typedef struct {
   int steps;
   int step_current;
   double* coefficients;
   double* history;
} SmoothDelayInfo;

SmoothDelayInfo profile_smoothdelay_setup(int steps, double initial_value);

double profile_smoothdelay_smooth(SmoothDelayInfo* profile, double curent_raw);

void profile_smoothdelay_free(SmoothDelayInfo* profile);

void profile_smoothdelay_print_coefficients(SmoothDelayInfo* profile);

#endif 
