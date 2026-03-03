    .section ".text.crt.trap_entry","ax",@progbits
    .align 6
    .globl trap_entry

trap_entry:
    j bl_panic_forever
