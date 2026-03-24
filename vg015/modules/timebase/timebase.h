#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

// Global millisecond counter incremented from timer interrupt.
extern volatile uint32_t ms_ticks;

#endif // TIMEBASE_H
