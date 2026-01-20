#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void timer_init(uint32_t hz);
uint32_t timer_get_hz(void);
uint32_t timer_get_ticks(void);
uint32_t timer_uptime_ms(void);
void timer_sleep_ms(uint32_t ms);

#endif
