#include "bl_jump.h"
#include <stdint.h>

static void bl_disable_interrupts(void) {
    __asm__ volatile ("csrci mstatus, 0x8");
    __asm__ volatile ("csrw mie, zero");
}

void bl_jump_to_app(uint32_t app_entry) {
    void (*app_start)(void) = (void (*)(void))(uintptr_t)app_entry;

    bl_disable_interrupts();
    __asm__ volatile ("fence iorw, iorw");
    app_start();

    for (;;) {
    }
}
