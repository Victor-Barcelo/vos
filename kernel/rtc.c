#include "rtc.h"
#include "io.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDRESS, (uint8_t)(reg | 0x80));  // Disable NMI
    io_wait();
    return inb(CMOS_DATA);
}

static void cmos_write(uint8_t reg, uint8_t value) {
    outb(CMOS_ADDRESS, (uint8_t)(reg | 0x80));  // Disable NMI
    io_wait();
    outb(CMOS_DATA, value);
    io_wait();
}

static bool rtc_update_in_progress(void) {
    return (cmos_read(0x0A) & 0x80) != 0;
}

static void rtc_wait_for_update(void) {
    while (rtc_update_in_progress()) {
        __asm__ volatile ("pause");
    }
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)((bcd & 0x0F) + ((bcd >> 4) * 10));
}

static uint8_t bin_to_bcd(uint8_t bin) {
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

static bool is_leap_year(uint16_t year) {
    if (year % 400 == 0) return true;
    if (year % 100 == 0) return false;
    return (year % 4) == 0;
}

static uint8_t days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && is_leap_year(year)) return 29;
    return days[month - 1];
}

typedef struct rtc_raw {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;
    uint8_t regb;
} rtc_raw_t;

static void rtc_read_raw(rtc_raw_t* raw) {
    raw->second  = cmos_read(0x00);
    raw->minute  = cmos_read(0x02);
    raw->hour    = cmos_read(0x04);
    raw->day     = cmos_read(0x07);
    raw->month   = cmos_read(0x08);
    raw->year    = cmos_read(0x09);
    raw->century = cmos_read(0x32);
    raw->regb    = cmos_read(0x0B);
}

static bool rtc_raw_equal(const rtc_raw_t* a, const rtc_raw_t* b) {
    return a->second == b->second &&
           a->minute == b->minute &&
           a->hour == b->hour &&
           a->day == b->day &&
           a->month == b->month &&
           a->year == b->year &&
           a->century == b->century &&
           a->regb == b->regb;
}

bool rtc_read_datetime(rtc_datetime_t* out) {
    if (!out) {
        return false;
    }

    rtc_raw_t a;
    rtc_raw_t b;

    rtc_wait_for_update();
    rtc_read_raw(&a);

    for (int i = 0; i < 10; i++) {
        rtc_wait_for_update();
        rtc_read_raw(&b);
        if (rtc_raw_equal(&a, &b)) {
            break;
        }
        a = b;
    }

    bool binary = (b.regb & 0x04) != 0;
    bool mode_24h = (b.regb & 0x02) != 0;

    uint8_t second = b.second;
    uint8_t minute = b.minute;
    uint8_t hour = b.hour;
    uint8_t day = b.day;
    uint8_t month = b.month;
    uint8_t year = b.year;
    uint8_t century = b.century;

    bool pm = false;
    if (!mode_24h) {
        pm = (hour & 0x80) != 0;
        hour &= 0x7F;
    }

    if (!binary) {
        second = bcd_to_bin(second);
        minute = bcd_to_bin(minute);
        hour = bcd_to_bin(hour);
        day = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year = bcd_to_bin(year);
        if (century != 0) {
            century = bcd_to_bin(century);
        }
    }

    if (!mode_24h) {
        if (pm) {
            if (hour < 12) hour = (uint8_t)(hour + 12);
        } else {
            if (hour == 12) hour = 0;
        }
    }

    uint16_t full_year = (century != 0) ? (uint16_t)(century * 100u + year) : (uint16_t)(2000u + year);

    if (month < 1 || month > 12) return false;
    uint8_t dim = days_in_month(full_year, month);
    if (dim == 0 || day < 1 || day > dim) return false;
    if (hour > 23 || minute > 59 || second > 59) return false;

    out->year = full_year;
    out->month = month;
    out->day = day;
    out->hour = hour;
    out->minute = minute;
    out->second = second;
    return true;
}

bool rtc_set_datetime(const rtc_datetime_t* dt) {
    if (!dt) {
        return false;
    }
    if (dt->year < 1970 || dt->year > 2099) return false;
    if (dt->month < 1 || dt->month > 12) return false;
    uint8_t dim = days_in_month(dt->year, dt->month);
    if (dim == 0 || dt->day < 1 || dt->day > dim) return false;
    if (dt->hour > 23 || dt->minute > 59 || dt->second > 59) return false;

    rtc_wait_for_update();

    uint8_t regb = cmos_read(0x0B);
    bool binary = (regb & 0x04) != 0;
    bool mode_24h = (regb & 0x02) != 0;

    uint8_t second = dt->second;
    uint8_t minute = dt->minute;
    uint8_t hour = dt->hour;
    uint8_t day = dt->day;
    uint8_t month = dt->month;
    uint8_t year = (uint8_t)(dt->year % 100u);
    uint8_t century = (uint8_t)(dt->year / 100u);

    uint8_t hour_pm_bit = 0;
    if (!mode_24h) {
        bool pm = hour >= 12;
        uint8_t hour12 = (uint8_t)(hour % 12);
        if (hour12 == 0) hour12 = 12;
        hour = hour12;
        if (pm) hour_pm_bit = 0x80;
    }

    if (!binary) {
        second = bin_to_bcd(second);
        minute = bin_to_bcd(minute);
        day = bin_to_bcd(day);
        month = bin_to_bcd(month);
        year = bin_to_bcd(year);
        century = bin_to_bcd(century);
        hour = (uint8_t)(bin_to_bcd(hour) | hour_pm_bit);
    } else {
        hour = (uint8_t)(hour | hour_pm_bit);
    }

    cmos_write(0x0B, (uint8_t)(regb | 0x80));

    cmos_write(0x00, second);
    cmos_write(0x02, minute);
    cmos_write(0x04, hour);
    cmos_write(0x07, day);
    cmos_write(0x08, month);
    cmos_write(0x09, year);
    cmos_write(0x32, century);

    cmos_write(0x0B, regb);
    return true;
}
