// Compile all inline functions from wyn_runtime.h into the runtime library
#define _POSIX_C_SOURCE 200809L
#include "wyn_runtime.h"
// This file exists solely to ensure all inline/static functions from
// wyn_runtime.h are compiled into libwyn_rt.a
