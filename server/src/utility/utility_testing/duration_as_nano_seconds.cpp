//
// Created by jeremiah on 1/29/22.
//

#include "utility_testing_functions.h"

//returns the duration between the 2 passed timespec values
double durationAsNanoSeconds(timespec stop, timespec start) {
    return stop.tv_sec - start.tv_sec + (1e-9)*(stop.tv_nsec - start.tv_nsec);
}