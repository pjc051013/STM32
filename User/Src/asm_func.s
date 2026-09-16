.syntax unified
.cpu cortex-m7
.thumb

.global asm_twoadd
.global asm_threeadd

.type asm_twoadd, %function
asm_twoadd:
    add r0, r0, r1
    bx  lr
.size asm_twoadd, .-asm_twoadd

.type asm_threeadd, %function
asm_threeadd:
    push {r2, lr}
    bl   asm_twoadd
    pop  {r2, lr}
    add  r0, r0, r2
    bx   lr
.size asm_threeadd, .-asm_threeadd