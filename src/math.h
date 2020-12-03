#pragma once

#include <SDL.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float clamp(float x, float min, float max);
int rand_range(int min, int max);
double rand_double();
