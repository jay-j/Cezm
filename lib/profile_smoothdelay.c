#include "profile_smoothdelay.h"
// Filter smoothes as much as possible (constant 2nd-derivative) while at guaranteed latency
//        in response to step input, output equals input after [steps]
// Can be good for teleoperation applications, since behavior is very predictable
//        and does still permit aggressive changes


// Create the structure, allocate interior variables, and initialize the delay size
SmoothDelayInfo profile_smoothdelay_setup(size_t steps){
   SmoothDelayInfo profile;
   profile.step_current = 0;
   profile.steps = steps;

   // init the coefficients. 
   profile.coefficients = (double*) malloc(profile.steps * sizeof( *profile.coefficients));
   double sum = 0;
   
   // coefficients are a parabola in velocity space; constant acceleration
   for (int i=0; i<profile.steps; ++i){
      double t = (double) i;
      profile.coefficients[i] = -(t + 1)*(t - (double) profile.steps);
      sum += profile.coefficients[i];
   }
   // normalize. sum(coefficients) = 1 
   for (int i=0; i<profile.steps; ++i){
      profile.coefficients[i] /= sum;
   }

   // initialize the input history array
   profile.history = (double*) malloc(profile.steps * sizeof( *profile.history));
   for (size_t i=0; i<profile.steps; ++i){
      profile.history[i] = 0;
   }

   return profile;
}


// updates history with the new input, spits out a new filterred value
// CAUTION - relies on the calling loop to operate at a constant frequency
double profile_smoothdelay_smooth(SmoothDelayInfo* profile, double current_raw){
   profile->history[profile->step_current] = current_raw;

   // discrete convolution integral. 
   double result = 0;
   for(int history_index=0; history_index<profile->steps; ++history_index){
      int profile_index = history_index - profile->step_current - 1;

      // DIY mod operator that handles negative values as expected
      if (profile_index >= profile->steps){
         profile_index -= profile->steps;
      }
      if (profile_index < 0){
         profile_index += profile->steps;
      }

      // actually compute the integral
      result += profile->coefficients[profile_index] * profile->history[history_index];
   }   

   // advance the current step, wrap as needed
   profile->step_current = (profile->step_current + 1) % profile->steps;

   return result;
}


// cleanup
void profile_smoothdelay_free(SmoothDelayInfo* profile){
   free(profile->coefficients);
   free(profile->history);
}


// print computed coefficients
void profile_smoothdelay_print_coefficients(SmoothDelayInfo* profile){
   printf("Profile Coefficients: %d\n", profile->steps);
   for(size_t i=0; i<profile->steps; ++i){
      printf("%lf  ", profile->coefficients[i]);
   }
   printf("\n");
}
