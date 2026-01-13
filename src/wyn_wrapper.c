// Wrapper program to initialize arguments for Wyn-compiled programs
#include <stdio.h>
#include <stdlib.h>
#include "wyn_interface.h"

// External main function from the Wyn-compiled program
extern int wyn_main(void);

int main(int argc, char** argv) {
    // Initialize arguments for Wyn interface
    wyn_init_args(argc, argv);
    
    // Call the Wyn-compiled main function
    return wyn_main();
}
