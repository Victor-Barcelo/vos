# Chapter 10: Timekeeping

Modern kernels rely on **time** for everything from scheduling to UI refresh. VOS uses two classic PC time sources:

- **PIT (Programmable Interval Timer)** for a fast, monotonic tick
- **CMOS RTC (Real-Time Clock)** for calendar date/time

## Two Notions of Time

### Monotonic Time

A counter that only moves forward while the system is running:

- Uptime tracking
- Sleep durations ("sleep for 100 ms")
- Scheduling time slices
- Rate-limiting UI refreshes

Monotonic time doesn't care about the calendar - it just counts.

### Wall Time

Human calendar date/time (year-month-day hour:minute:second):

- `date` command output
- File timestamps
- Log entries

Wall time corresponds to real-world time.

## PIT Timer (IRQ0)

The Programmable Interval Timer (8253/8254 compatible) is the historical timer chip on PCs.

### Hardware Details

- **Base frequency**: ~1,193,182 Hz
- **Command port**: 0x43
- **Channel 0 data**: 0x40 (connected to IRQ0)
- **Mode**: Square wave generator (mode 3)

### Configuring the PIT

```c
#define PIT_FREQ        1193182
#define PIT_COMMAND     0x43
#define PIT_CHANNEL0    0x40

void timer_init(uint32_t hz) {
    timer_hz = hz;

    // Calculate divisor
    uint16_t divisor = PIT_FREQ / hz;

    // Channel 0, lobyte/hibyte, mode 3, binary
    outb(PIT_COMMAND, 0x36);

    // Send divisor
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    // Register IRQ handler
    irq_register_handler(0, timer_irq_handler);
}
```

VOS uses **1000 Hz** (1 ms per tick) - a good balance between resolution and overhead.

### Timer Tick Handler

```c
static volatile uint64_t timer_ticks = 0;

void timer_irq_handler(interrupt_frame_t *frame) {
    timer_ticks++;
}

uint64_t timer_get_ticks(void) {
    uint32_t flags = irq_save();
    uint64_t ticks = timer_ticks;
    irq_restore(flags);
    return ticks;
}
```

The `irq_save`/`irq_restore` ensures atomic reads even if interrupted mid-read.

### Uptime and Sleep

```c
uint64_t timer_uptime_ms(void) {
    return (timer_get_ticks() * 1000) / timer_hz;
}

void timer_sleep_ms(uint32_t ms) {
    uint64_t target = timer_get_ticks() + (ms * timer_hz) / 1000;

    // Enable interrupts if disabled
    uint32_t flags = irq_save();
    sti();

    while (timer_get_ticks() < target) {
        hlt();  // Sleep until next interrupt
    }

    irq_restore(flags);
}
```

### Why Use hlt?

The `hlt` instruction puts the CPU in a low-power state until an interrupt occurs:

- Saves power
- Reduces heat
- Still wakes on timer tick

Without `hlt`, sleep would be a busy loop consuming 100% CPU.

## Scheduler Tick

VOS uses the timer for preemptive scheduling:

```c
static uint32_t sched_tick_counter = 0;
#define SCHED_QUANTUM 10  // 10 ticks = 10ms

interrupt_frame_t* tasking_on_timer_tick(interrupt_frame_t *frame) {
    sched_tick_counter++;

    if (sched_tick_counter >= SCHED_QUANTUM) {
        sched_tick_counter = 0;
        return schedule(frame);  // May return different task's frame
    }

    return frame;
}
```

Every 10ms, the scheduler considers switching to another task.

## CMOS RTC (Real-Time Clock)

The RTC provides calendar time, battery-backed to survive reboots.

### Hardware Details

- **Index port**: 0x70
- **Data port**: 0x71
- **Battery-backed**: Keeps time when powered off

### Reading RTC Registers

```c
#define CMOS_INDEX  0x70
#define CMOS_DATA   0x71

uint8_t rtc_read_register(uint8_t reg) {
    outb(CMOS_INDEX, reg);
    return inb(CMOS_DATA);
}
```

### RTC Registers

| Register | Content |
|----------|---------|
| 0x00 | Seconds |
| 0x02 | Minutes |
| 0x04 | Hours |
| 0x06 | Day of week |
| 0x07 | Day of month |
| 0x08 | Month |
| 0x09 | Year (last 2 digits) |
| 0x0A | Status Register A |
| 0x0B | Status Register B |
| 0x32 | Century (if available) |

### Reading Date/Time Safely

The RTC updates once per second. Reading during an update can give inconsistent values.

```c
typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
} rtc_datetime_t;

bool rtc_read_datetime(rtc_datetime_t *dt) {
    rtc_datetime_t dt1, dt2;

    // Wait for update-in-progress to clear
    while (rtc_read_register(0x0A) & 0x80);

    // Read twice and compare
    for (int retries = 0; retries < 10; retries++) {
        read_rtc_fields(&dt1);
        read_rtc_fields(&dt2);

        if (memcmp(&dt1, &dt2, sizeof(dt1)) == 0) {
            *dt = dt1;
            return true;
        }
    }

    return false;
}
```

### BCD vs Binary

RTC values may be in BCD (Binary-Coded Decimal) or binary:

```c
static void read_rtc_fields(rtc_datetime_t *dt) {
    dt->second = rtc_read_register(0x00);
    dt->minute = rtc_read_register(0x02);
    dt->hour   = rtc_read_register(0x04);
    dt->day    = rtc_read_register(0x07);
    dt->month  = rtc_read_register(0x08);
    dt->year   = rtc_read_register(0x09);

    // Check register B for format
    uint8_t regB = rtc_read_register(0x0B);
    bool binary = regB & 0x04;
    bool hour24 = regB & 0x02;

    if (!binary) {
        // Convert BCD to binary
        dt->second = bcd_to_binary(dt->second);
        dt->minute = bcd_to_binary(dt->minute);
        dt->hour   = bcd_to_binary(dt->hour & 0x7F);
        dt->day    = bcd_to_binary(dt->day);
        dt->month  = bcd_to_binary(dt->month);
        dt->year   = bcd_to_binary(dt->year);
    }

    // Handle 12-hour format
    if (!hour24 && (dt->hour & 0x80)) {
        dt->hour = ((dt->hour & 0x7F) % 12) + 12;
    }

    // Add century
    uint8_t century = rtc_read_register(0x32);
    if (century) {
        dt->year += bcd_to_binary(century) * 100;
    } else {
        dt->year += 2000;  // Assume 2000s
    }
}

static uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}
```

### Setting the RTC

```c
bool rtc_set_datetime(const rtc_datetime_t *dt) {
    // Validate ranges
    if (dt->month < 1 || dt->month > 12) return false;
    if (dt->day < 1 || dt->day > 31) return false;
    if (dt->hour > 23) return false;
    if (dt->minute > 59) return false;
    if (dt->second > 59) return false;

    // Disable RTC updates
    uint8_t regB = rtc_read_register(0x0B);
    rtc_write_register(0x0B, regB | 0x80);  // Set update inhibit

    // Write values (assuming BCD mode)
    rtc_write_register(0x00, binary_to_bcd(dt->second));
    rtc_write_register(0x02, binary_to_bcd(dt->minute));
    rtc_write_register(0x04, binary_to_bcd(dt->hour));
    rtc_write_register(0x07, binary_to_bcd(dt->day));
    rtc_write_register(0x08, binary_to_bcd(dt->month));
    rtc_write_register(0x09, binary_to_bcd(dt->year % 100));
    rtc_write_register(0x32, binary_to_bcd(dt->year / 100));

    // Re-enable updates
    rtc_write_register(0x0B, regB & ~0x80);

    return true;
}
```

## Shell Commands

```c
static void cmd_uptime(void) {
    uint64_t ms = timer_uptime_ms();
    uint32_t seconds = ms / 1000;
    uint32_t milliseconds = ms % 1000;

    printf("Uptime: %u.%03u seconds\n", seconds, milliseconds);
}

static void cmd_date(void) {
    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt)) {
        printf("%04u-%02u-%02u %02u:%02u:%02u\n",
               dt.year, dt.month, dt.day,
               dt.hour, dt.minute, dt.second);
    }
}

static void cmd_sleep(const char *args) {
    uint32_t ms = atoi(args);
    timer_sleep_ms(ms);
}
```

## Time Syscalls

VOS provides time-related syscalls:

```c
// Get Unix timestamp
int32_t sys_time(void) {
    rtc_datetime_t dt;
    rtc_read_datetime(&dt);
    return datetime_to_unix(&dt);
}

// Get time with microseconds
int32_t sys_gettimeofday(struct timeval *tv) {
    if (!tv) return -EFAULT;

    tv->tv_sec = sys_time();
    tv->tv_usec = (timer_get_ticks() % timer_hz) * 1000000 / timer_hz;

    return 0;
}

// Sleep with nanosecond precision
int32_t sys_nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!req) return -EFAULT;

    uint64_t ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;
    timer_sleep_ms(ms);

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return 0;
}
```

## Summary

VOS timekeeping provides:

1. **PIT timer** for monotonic ticks (uptime, sleep, scheduling)
2. **CMOS RTC** for wall clock time (date/time)
3. **Sleep functions** using hlt for power efficiency
4. **Scheduler integration** for preemptive multitasking
5. **Syscalls** for user programs to access time

The PIT handles "how long" questions while the RTC handles "what time is it" questions.

---

*Previous: [Chapter 9: Memory Management](09_memory.md)*
*Next: [Chapter 11: Console Output](11_console.md)*
