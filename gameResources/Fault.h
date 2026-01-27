// Fault.h - Stub for StateMachine.h compatibility with VOS
#ifndef FAULT_H
#define FAULT_H

#include <stdio.h>

// Simple fault handler - just prints error and continues
// In a real system this might halt or log to persistent storage
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            printf("ASSERT FAILED: %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

#define FaultHandler(msg) \
    do { \
        printf("FAULT: %s at %s:%d\n", (msg), __FILE__, __LINE__); \
    } while(0)

#endif // FAULT_H
