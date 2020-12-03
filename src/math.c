#include "math.h"

float clamp(float x, float min, float max) {
    return x < min ? min : x > max ? max : x;
}

int rand_range(int min, int max) {
    return min + (rand() / ((RAND_MAX / (max - min + 1)) + 1));
}

double rand_double() {
    return rand() / (double)RAND_MAX;
}
