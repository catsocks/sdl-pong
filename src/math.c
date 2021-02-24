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

static int sign(int x) {
	return (x < 0) ? -1 : (x > 0);
}

float move_towards(float current, float target, float max_delta) {
     if (fabsf(target - current) <= max_delta) {
         return target;
     }
     return current + (sign(target - current) * max_delta);
}
