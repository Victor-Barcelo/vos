#ifndef BASIC_PROGRAMS_H
#define BASIC_PROGRAMS_H

// Number of example programs
#define BASIC_NUM_PROGRAMS 10

// Get program source by index (1-10)
const char* basic_get_program(int index);

// Get program name by index (1-10)
const char* basic_get_program_name(int index);

// Get program description by index (1-10)
const char* basic_get_program_description(int index);

#endif

