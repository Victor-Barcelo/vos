#ifndef RTC_H
#define RTC_H

#include "types.h"

typedef struct rtc_datetime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_datetime_t;

bool rtc_read_datetime(rtc_datetime_t* out);
bool rtc_set_datetime(const rtc_datetime_t* dt);

#endif
