#include "math.h"

float clamp(float x, float min, float max) {
    return fmaxf(min, fminf(x, max));
}

// Return a random integer between min and max (inclusive).
int rand_range(int min, int max) {
    return min + (rand() / ((RAND_MAX / (max - min + 1)) + 1));
}

// Return a random floating-point number between min and max (inclusive).
float frand_range(float min, float max) {
    return min + (rand() / ((double)RAND_MAX / (max - min)));
}
