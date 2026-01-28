#include <math.h>
#include <stdlib.h>

// Math module - basic mathematical functions

// Absolute value
double wyn_math_abs(double x) {
    return fabs(x);
}

// Minimum of two values
double wyn_math_min(double a, double b) {
    return (a < b) ? a : b;
}

// Maximum of two values
double wyn_math_max(double a, double b) {
    return (a > b) ? a : b;
}

// Power function
double wyn_math_pow(double base, double exp) {
    return pow(base, exp);
}

// Square root
double wyn_math_sqrt(double x) {
    return sqrt(x);
}

// Floor function
double wyn_math_floor(double x) {
    return floor(x);
}

// Ceiling function
double wyn_math_ceil(double x) {
    return ceil(x);
}

// Round function
double wyn_math_round(double x) {
    return round(x);
}