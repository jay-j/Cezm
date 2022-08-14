#ifndef SMOOTH_DELAY_H
#define SMOOTH_DELAY_H

#include <stdio.h>
#include <stdlib.h>

typedef struct {
   int steps;
   int step_current;
   int* coefficients;
   int* history;
   int sum;
} SmoothDelayInfo;

SmoothDelayInfo profile_smoothdelay_setup(int steps, int initial_value);

int profile_smoothdelay_smooth(SmoothDelayInfo* profile, int curent_raw);

void profile_smoothdelay_free(SmoothDelayInfo* profile);

void profile_smoothdelay_print_coefficients(SmoothDelayInfo* profile);

#endif 
