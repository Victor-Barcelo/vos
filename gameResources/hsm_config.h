// hsm_config.h - Configuration for UML Hierarchical State Machine
#ifndef HSM_CONFIG_H
#define HSM_CONFIG_H

// Maximum hierarchical levels (nesting depth of states)
#define HSM_MAX_HIERARCHICAL_LEVEL 5

// Enable/disable debug logging
// #define HSM_DEBUG

#ifdef HSM_DEBUG
#include <stdio.h>
#define HSM_LOG(fmt, ...) printf("[HSM] " fmt "\n", ##__VA_ARGS__)
#else
#define HSM_LOG(fmt, ...) ((void)0)
#endif

#endif // HSM_CONFIG_H
