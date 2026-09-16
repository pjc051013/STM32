#ifndef PTOS_H
#define PTOS_H

#include <stdint.h>

typedef enum {
    TASK_READY,
    TASK_BLOCKED,
    TASK_RUNNING
} TaskState;

typedef struct TCB TCB_t;

struct TCB {
    uint32_t *sp;
    TaskState state;
    uint32_t wake_tick;
    uint8_t priority;
};

extern volatile uint32_t ptOS_start_first;

extern TCB_t *current_tcb;
extern TCB_t *next_tcb;

void ptOS_Init(void);
void ptOS_Start(void);
void ptOS_Yield(void);

extern volatile uint32_t os_tick;

void ptOS_Delay(uint32_t ticks);

void ptOS_Tick(void);

#endif