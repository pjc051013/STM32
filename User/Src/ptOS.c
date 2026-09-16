#include <stdint.h>
#include "stm32h7xx_hal.h"
#include "ptOS.h"
#include "main.h"

#define TASK_STACK_WORDS 128U
#define NORMAL_TASK_COUNT 3U
#define TASK_COUNT        (NORMAL_TASK_COUNT + 1U)

static TCB_t task1_tcb;
static TCB_t task2_tcb;
static TCB_t busy_tcb;
static TCB_t idle_tcb;

static uint32_t task1_stack[TASK_STACK_WORDS] __attribute__((aligned(8)));
static uint32_t task2_stack[TASK_STACK_WORDS] __attribute__((aligned(8)));
static uint32_t busy_stack[TASK_STACK_WORDS] __attribute__((aligned(8)));
static uint32_t idle_stack[TASK_STACK_WORDS] __attribute__((aligned(8)));

static TCB_t *const task_list[TASK_COUNT] = {
    &task1_tcb, &task2_tcb, &busy_tcb, &idle_tcb
};

volatile uint32_t os_tick;
volatile uint32_t ptOS_start_first;
TCB_t *current_tcb;
TCB_t *next_tcb;
static uint32_t rr_cursor;
static volatile uint32_t busy_counter;
static volatile uint32_t busy_led_divider;

static void task1(void)
{
    while (1)
    {
        HAL_GPIO_TogglePin(TEST_0_GPIO_Port, TEST_0_Pin);
        ptOS_Delay(250U);
    }
}

static void task2(void)
{
    while (1)
    {
        HAL_GPIO_TogglePin(TEST_1_GPIO_Port, TEST_1_Pin);
        ptOS_Delay(500U);
    }
}

static void busy_task(void)
{
    while (1)
    {
        /*
         * Deliberately do not call ptOS_Delay() or ptOS_Yield().
         * SysTick must preempt this task for the other tasks to run.
         */
        busy_counter++;
        busy_led_divider++;
        if (busy_led_divider >= 100000U)
        {
            HAL_GPIO_TogglePin(TEST_2_GPIO_Port, TEST_2_Pin);
            busy_led_divider = 0U;
        }
    }
}

static void idle_task(void)
{
    while (1)
    {
        __WFI();
    }
}

static uint32_t *build_stack(uint32_t *stack, uint32_t words,
                             void (*entry)(void))
{
    uint32_t *sp = (uint32_t *)((uintptr_t)(stack + words) & ~(uintptr_t)7U);
    unsigned int i;

    sp -= 8U;
    sp[0] = 0U;
    sp[1] = 0U;
    sp[2] = 0U;
    sp[3] = 0U;
    sp[4] = 0U;
    sp[5] = 0U;
    sp[6] = ((uint32_t)(uintptr_t)entry) | 1U;
    sp[7] = 0x01000000U;

    sp -= 8U;
    for (i = 0U; i < 8U; ++i)
    {
        sp[i] = 0U;
    }
    return sp;
}

static uint32_t task_index(const TCB_t *task)
{
    uint32_t i;
    for (i = 0U; i < TASK_COUNT; ++i)
    {
        if (task_list[i] == task)
        {
            return i;
        }
    }
    return 0U;
}

static TCB_t *select_next(void)
{
    TCB_t *selected = &idle_tcb;
    uint8_t best_priority = UINT8_MAX;
    uint32_t offset;

    for (offset = 0U; offset < NORMAL_TASK_COUNT; ++offset)
    {
        uint32_t index = (rr_cursor + offset) % NORMAL_TASK_COUNT;
        TCB_t *candidate = task_list[index];
        if (candidate->state == TASK_READY &&
            candidate->priority < best_priority)
        {
            selected = candidate;
            best_priority = candidate->priority;
        }
    }

    if (selected != &idle_tcb)
    {
        rr_cursor = (task_index(selected) + 1U) % NORMAL_TASK_COUNT;
    }
    return selected;
}

static void request_switch(TCB_t *selected)
{
    if (selected == current_tcb)
    {
        selected->state = TASK_RUNNING;
        return;
    }

    next_tcb = selected;
    selected->state = TASK_RUNNING;
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

void ptOS_Init(void)
{
    task1_tcb.sp = build_stack(task1_stack, TASK_STACK_WORDS, task1);
    task2_tcb.sp = build_stack(task2_stack, TASK_STACK_WORDS, task2);
    busy_tcb.sp = build_stack(busy_stack, TASK_STACK_WORDS, busy_task);
    idle_tcb.sp = build_stack(idle_stack, TASK_STACK_WORDS, idle_task);

    task1_tcb.state = TASK_READY;
    task2_tcb.state = TASK_READY;
    busy_tcb.state = TASK_READY;
    idle_tcb.state = TASK_READY;
    task1_tcb.wake_tick = 0U;
    task2_tcb.wake_tick = 0U;
    busy_tcb.wake_tick = 0U;
    idle_tcb.wake_tick = 0U;
    task1_tcb.priority = 1U;
    task2_tcb.priority = 1U;
    busy_tcb.priority = 1U;
    idle_tcb.priority = UINT8_MAX;

    os_tick = 0U;
    busy_counter = 0U;
    busy_led_divider = 0U;
    rr_cursor = 0U;
    current_tcb = &task1_tcb;
    next_tcb = &task1_tcb;
    ptOS_start_first = 0U;
}

void ptOS_Start(void)
{
    __disable_irq();
    next_tcb = select_next();
    next_tcb->state = TASK_RUNNING;
    ptOS_start_first = 1U;
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
    __enable_irq();

    while (1)
    {
    }
}

void ptOS_Delay(uint32_t ticks)
{
    TCB_t *selected;

    if (ticks == 0U)
    {
        ptOS_Yield();
        return;
    }

    __disable_irq();
    current_tcb->wake_tick = os_tick + ticks;
    current_tcb->state = TASK_BLOCKED;
    selected = select_next();
    request_switch(selected);
    __enable_irq();
}

void ptOS_Yield(void)
{
    TCB_t *selected;

    __disable_irq();
    if (current_tcb != &idle_tcb)
    {
        current_tcb->state = TASK_READY;
    }
    selected = select_next();
    request_switch(selected);
    __enable_irq();
}

void ptOS_Tick(void)
{
    uint32_t i;
    TCB_t *selected;

    for (i = 0U; i < NORMAL_TASK_COUNT; ++i)
    {
        TCB_t *task = task_list[i];
        if (task->state == TASK_BLOCKED &&
            (int32_t)(os_tick - task->wake_tick) >= 0)
        {
            task->state = TASK_READY;
        }
    }

    /*
     * The SysTick interrupt is the time-slice boundary. Make the running
     * task eligible again before selecting the next ready task so tasks with
     * the same priority are preempted in round-robin order.
     */
    if (current_tcb != &idle_tcb &&
        current_tcb->state == TASK_RUNNING)
    {
        current_tcb->state = TASK_READY;
    }

    selected = select_next();
    request_switch(selected);
}
