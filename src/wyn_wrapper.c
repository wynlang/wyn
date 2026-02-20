// Wrapper program to initialize arguments for Wyn-compiled programs
#include <stdio.h>
#include <stdlib.h>
#include "wyn_interface.h"

// External main function from the Wyn-compiled program
extern int wyn_main(void);

// Global argc/argv for System::args()
extern int __wyn_argc;
extern char** __wyn_argv;

__attribute__((flatten))
int main(int argc, char** argv) {
    // Initialize arguments for Wyn interface
    wyn_init_args(argc, argv);
    
    // Set global argc/argv for System::args()
    __wyn_argc = argc;
    __wyn_argv = argv;
    
    // Call the Wyn-compiled main function
    return wyn_main();
}
