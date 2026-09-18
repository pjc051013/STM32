#ifndef PTOS_H
#define PTOS_H

#include <stdint.h>

#define PT_WAIT_FOREVER UINT32_MAX

typedef enum {
    TASK_READY,
    TASK_BLOCKED,
    TASK_RUNNING
} TaskState;

typedef struct TCB TCB_t;
typedef struct pt_semaphore pt_semaphore_t;
typedef struct pt_mutex pt_mutex_t;
typedef pt_semaphore_t pt_binary_semaphore_t;

struct pt_semaphore {
    volatile uint32_t count;
    uint32_t max_count;
    TCB_t *wait_head;
    TCB_t *wait_tail;
};

struct pt_mutex {
    TCB_t *owner;
    uint32_t lock_count;
    TCB_t *wait_head;
    TCB_t *wait_tail;
    pt_mutex_t *owner_next;
};

struct TCB {
    uint32_t *sp;
    TaskState state;
    uint32_t wake_tick;
    uint8_t priority;
    uint8_t base_priority;
    TCB_t *semaphore_next;
    pt_semaphore_t *waiting_semaphore;
    TCB_t *mutex_next;
    pt_mutex_t *waiting_mutex;
    pt_mutex_t *owned_mutexes;
    uint32_t wait_deadline;
    uint32_t wait_result;
};

extern volatile uint32_t pt_thread_start_first;

extern TCB_t *current_tcb;
extern TCB_t *next_tcb;

void pt_kernel_initialize(void);
void pt_kernel_start(void);
void pt_thread_relinquish(void);

extern volatile uint32_t os_tick;

void pt_thread_sleep(uint32_t ticks);

void pt_timer_interrupt(void);

void pt_semaphore_init(pt_semaphore_t *semaphore,
                       uint32_t max_count,
                       uint32_t initial_count);
void pt_semaphore_get(pt_semaphore_t *semaphore);
void pt_semaphore_put(pt_semaphore_t *semaphore);
void pt_semaphore_put_from_isr(pt_semaphore_t *semaphore);

void pt_binary_semaphore_init(pt_binary_semaphore_t *semaphore,
                              uint32_t initial_state);
void pt_binary_semaphore_get(pt_binary_semaphore_t *semaphore);
void pt_binary_semaphore_put(pt_binary_semaphore_t *semaphore);
void pt_binary_semaphore_put_from_isr(pt_binary_semaphore_t *semaphore);

void pt_mutex_init(pt_mutex_t *mutex);
uint32_t pt_mutex_get(pt_mutex_t *mutex, uint32_t wait_ticks);
uint32_t pt_mutex_put(pt_mutex_t *mutex);

#endif