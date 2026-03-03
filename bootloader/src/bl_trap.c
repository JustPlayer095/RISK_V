#include "bl_trap.h"
#include <stdint.h>

void bl_trap_init(void) {
    extern void trap_entry(void);
    uintptr_t trap_addr = (uintptr_t)&trap_entry;
    __asm__ volatile ("csrw mtvec, %0" :: "r"(trap_addr));
}

void bl_panic_forever(void) {
    __asm__ volatile ("csrci mstatus, 0x8");
    __asm__ volatile ("csrw mie, zero");
    for (;;) {
        __asm__ volatile ("wfi");
    }
}
