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
volatile uint32_t pt_thread_start_first;
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
        pt_thread_sleep(250U);
    }
}

static void task2(void)
{
    while (1)
    {
        HAL_GPIO_TogglePin(TEST_1_GPIO_Port, TEST_1_Pin);
        pt_thread_sleep(500U);
    }
}

static void busy_task(void)
{
    while (1)
    {
        /*
         * Deliberately do not call pt_thread_sleep() or
         * pt_thread_relinquish().
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

static uint32_t *pt_thread_stack_build(uint32_t *stack, void (*entry)(void))
{
    uint32_t *sp = (uint32_t *)((uintptr_t)(stack + TASK_STACK_WORDS) &
                                ~(uintptr_t)7U);
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

static uint32_t pt_thread_index(const TCB_t *task)
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

static TCB_t *pt_thread_schedule(void)
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
        rr_cursor = (pt_thread_index(selected) + 1U) % NORMAL_TASK_COUNT;
    }
    return selected;
}

static void pt_thread_context_switch(TCB_t *selected)
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

static void pt_semaphore_put_locked(pt_semaphore_t *semaphore)
{
    /* 保存重新调度后选中的任务。 */
    TCB_t *selected;
    /* 保存从等待队列头部取出的任务。 */
    TCB_t *task;

    /* 没有等待任务时，增加一个可用信号，但不超过最大值。 */
    if (semaphore->wait_head == NULL)
    {
        /* 计数信号量不能超过配置的最大计数。 */
        if (semaphore->count < semaphore->max_count)
        {
            semaphore->count++;
        }
        /* 没有任务需要唤醒，释放操作到此结束。 */
        return;
    }

    /* 取出等待队列中最早等待的任务。 */
    task = semaphore->wait_head;
    /* 将等待队列头移动到下一个任务。 */
    semaphore->wait_head = task->semaphore_next;
    /* 如果没有下一个任务，说明等待队列已经为空。 */
    if (semaphore->wait_head == NULL)
    {
        /* 队列为空时，尾指针也必须清空。 */
        semaphore->wait_tail = NULL;
    }

    /* 将被唤醒的任务从信号量等待链表中断开。 */
    task->semaphore_next = NULL;
    /* 清除任务正在等待的信号量记录。 */
    task->waiting_semaphore = NULL;
    /* 信号量直接交给被唤醒的任务，因此计数保持为 0。 */
    semaphore->count = 0U;
    /* 允许该任务重新参与调度。 */
    task->state = TASK_READY;

    /* 当前任务不是空闲任务且正在运行时，重新加入就绪队列。 */
    if (current_tcb != &idle_tcb &&
        current_tcb->state == TASK_RUNNING)
    {
        /* 当前任务让出本次运行机会。 */
        current_tcb->state = TASK_READY;
    }

    /* 根据优先级和轮询规则选择下一个任务。 */
    selected = pt_thread_schedule();
    /* 请求 PendSV 执行真正的上下文切换。 */
    pt_thread_context_switch(selected);
}

void pt_kernel_initialize(void)
{
    task1_tcb.sp = pt_thread_stack_build(task1_stack, task1);
    task2_tcb.sp = pt_thread_stack_build(task2_stack, task2);
    busy_tcb.sp = pt_thread_stack_build(busy_stack, busy_task);
    idle_tcb.sp = pt_thread_stack_build(idle_stack, idle_task);

    task1_tcb.state = TASK_READY;
    task2_tcb.state = TASK_READY;
    busy_tcb.state = TASK_READY;
    idle_tcb.state = TASK_READY;
    task1_tcb.semaphore_next = NULL;
    task2_tcb.semaphore_next = NULL;
    busy_tcb.semaphore_next = NULL;
    idle_tcb.semaphore_next = NULL;
    task1_tcb.waiting_semaphore = NULL;
    task2_tcb.waiting_semaphore = NULL;
    busy_tcb.waiting_semaphore = NULL;
    idle_tcb.waiting_semaphore = NULL;
    task1_tcb.mutex_next = NULL;
    task2_tcb.mutex_next = NULL;
    busy_tcb.mutex_next = NULL;
    idle_tcb.mutex_next = NULL;
    task1_tcb.waiting_mutex = NULL;
    task2_tcb.waiting_mutex = NULL;
    busy_tcb.waiting_mutex = NULL;
    idle_tcb.waiting_mutex = NULL;
    task1_tcb.owned_mutexes = NULL;
    task2_tcb.owned_mutexes = NULL;
    busy_tcb.owned_mutexes = NULL;
    idle_tcb.owned_mutexes = NULL;
    task1_tcb.wait_deadline = PT_WAIT_FOREVER;
    task2_tcb.wait_deadline = PT_WAIT_FOREVER;
    busy_tcb.wait_deadline = PT_WAIT_FOREVER;
    idle_tcb.wait_deadline = PT_WAIT_FOREVER;
    task1_tcb.wait_result = 0U;
    task2_tcb.wait_result = 0U;
    busy_tcb.wait_result = 0U;
    idle_tcb.wait_result = 0U;
    task1_tcb.wake_tick = 0U;
    task2_tcb.wake_tick = 0U;
    busy_tcb.wake_tick = 0U;
    idle_tcb.wake_tick = 0U;
    task1_tcb.priority = 1U;
    task2_tcb.priority = 1U;
    busy_tcb.priority = 1U;
    idle_tcb.priority = UINT8_MAX;
    task1_tcb.base_priority = task1_tcb.priority;
    task2_tcb.base_priority = task2_tcb.priority;
    busy_tcb.base_priority = busy_tcb.priority;
    idle_tcb.base_priority = idle_tcb.priority;

    os_tick = 0U;
    busy_counter = 0U;
    busy_led_divider = 0U;
    rr_cursor = 0U;
    current_tcb = &task1_tcb;
    next_tcb = &task1_tcb;
    pt_thread_start_first = 0U;
}

void pt_semaphore_init(pt_semaphore_t *semaphore,
                       uint32_t max_count,
                       uint32_t initial_count)
{
    semaphore->max_count = max_count;
    semaphore->count = (initial_count < max_count) ?
                       initial_count : max_count;
    semaphore->wait_head = NULL;
    semaphore->wait_tail = NULL;
}

void pt_semaphore_get(pt_semaphore_t *semaphore)
{
    TCB_t *selected;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (semaphore->count != 0U)
    {
        semaphore->count--;
        __set_PRIMASK(primask);
        return;
    }

    current_tcb->semaphore_next = NULL;
    current_tcb->waiting_semaphore = semaphore;
    if (semaphore->wait_tail == NULL)
    {
        semaphore->wait_head = current_tcb;
    }
    else
    {
        semaphore->wait_tail->semaphore_next = current_tcb;
    }
    semaphore->wait_tail = current_tcb;
    current_tcb->state = TASK_BLOCKED;

    selected = pt_thread_schedule();
    pt_thread_context_switch(selected);
    __set_PRIMASK(primask);
}

void pt_semaphore_put(pt_semaphore_t *semaphore)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    pt_semaphore_put_locked(semaphore);
    __set_PRIMASK(primask);
}

void pt_semaphore_put_from_isr(pt_semaphore_t *semaphore)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    pt_semaphore_put_locked(semaphore);
    __set_PRIMASK(primask);
}

void pt_binary_semaphore_init(pt_binary_semaphore_t *semaphore,
                              uint32_t initial_state)
{
    pt_semaphore_init(semaphore, 1U, initial_state != 0U ? 1U : 0U);
}

void pt_binary_semaphore_get(pt_binary_semaphore_t *semaphore)
{
    pt_semaphore_get(semaphore);
}

void pt_binary_semaphore_put(pt_binary_semaphore_t *semaphore)
{
    pt_semaphore_put(semaphore);
}

void pt_binary_semaphore_put_from_isr(
    pt_binary_semaphore_t *semaphore)
{
    pt_semaphore_put_from_isr(semaphore);
}

void pt_mutex_init(pt_mutex_t *mutex)
{
    mutex->owner = NULL;
    mutex->lock_count = 0U;
    mutex->wait_head = NULL;
    mutex->wait_tail = NULL;
    mutex->owner_next = NULL;
}

static void pt_mutex_add_owned_locked(pt_mutex_t *mutex, TCB_t *owner)
{
    mutex->owner_next = owner->owned_mutexes;
    owner->owned_mutexes = mutex;
}

static void pt_mutex_remove_owned_locked(pt_mutex_t *mutex, TCB_t *owner)
{
    pt_mutex_t *previous = NULL;
    pt_mutex_t *owned = owner->owned_mutexes;

    while (owned != NULL)
    {
        if (owned == mutex)
        {
            if (previous == NULL)
            {
                owner->owned_mutexes = owned->owner_next;
            }
            else
            {
                previous->owner_next = owned->owner_next;
            }
            owned->owner_next = NULL;
            return;
        }

        previous = owned;
        owned = owned->owner_next;
    }
}

static void pt_mutex_recalculate_priority_locked(TCB_t *task)
{
    uint8_t priority = task->base_priority;
    pt_mutex_t *mutex = task->owned_mutexes;

    while (mutex != NULL)
    {
        TCB_t *waiter = mutex->wait_head;
        while (waiter != NULL)
        {
            if (waiter->priority < priority)
            {
                priority = waiter->priority;
            }
            waiter = waiter->mutex_next;
        }
        mutex = mutex->owner_next;
    }

    task->priority = priority;
}

static void pt_mutex_inherit_priority_locked(TCB_t *waiter,
                                             pt_mutex_t *mutex)
{
    TCB_t *owner = mutex->owner;

    while (owner != NULL && waiter->priority < owner->priority)
    {
        owner->priority = waiter->priority;
        if (owner->waiting_mutex == NULL)
        {
            break;
        }
        owner = owner->waiting_mutex->owner;
    }
}

static void pt_mutex_insert_waiter_locked(pt_mutex_t *mutex, TCB_t *task)
{
    TCB_t *previous = NULL;
    TCB_t *current = mutex->wait_head;

    while (current != NULL && current->priority <= task->priority)
    {
        previous = current;
        current = current->mutex_next;
    }

    task->mutex_next = current;
    if (previous == NULL)
    {
        mutex->wait_head = task;
    }
    else
    {
        previous->mutex_next = task;
    }

    if (current == NULL)
    {
        mutex->wait_tail = task;
    }
}

uint32_t pt_mutex_get(pt_mutex_t *mutex, uint32_t wait_ticks)
{
    TCB_t *selected;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (mutex->owner == NULL)
    {
        mutex->owner = current_tcb;
        mutex->lock_count = 1U;
        pt_mutex_add_owned_locked(mutex, current_tcb);
        __set_PRIMASK(primask);
        return 1U;
    }

    if (mutex->owner == current_tcb)
    {
        mutex->lock_count++;
        __set_PRIMASK(primask);
        return 1U;
    }

    if (wait_ticks == 0U)
    {
        __set_PRIMASK(primask);
        return 0U;
    }

    current_tcb->mutex_next = NULL;
    current_tcb->waiting_mutex = mutex;
    current_tcb->wait_deadline = (wait_ticks == PT_WAIT_FOREVER) ?
                                  PT_WAIT_FOREVER : os_tick + wait_ticks;
    current_tcb->wait_result = 0U;
    pt_mutex_insert_waiter_locked(mutex, current_tcb);
    pt_mutex_inherit_priority_locked(current_tcb, mutex);
    current_tcb->state = TASK_BLOCKED;

    selected = pt_thread_schedule();
    pt_thread_context_switch(selected);
    __set_PRIMASK(primask);
    return current_tcb->wait_result;
}

static void pt_mutex_remove_waiter_locked(pt_mutex_t *mutex, TCB_t *target)
{
    TCB_t *previous = NULL;
    TCB_t *task = mutex->wait_head;

    while (task != NULL)
    {
        if (task == target)
        {
            if (previous == NULL)
            {
                mutex->wait_head = task->mutex_next;
            }
            else
            {
                previous->mutex_next = task->mutex_next;
            }

            if (mutex->wait_tail == task)
            {
                mutex->wait_tail = previous;
            }
            task->mutex_next = NULL;
            return;
        }

        previous = task;
        task = task->mutex_next;
    }
}

uint32_t pt_mutex_put(pt_mutex_t *mutex)
{
    TCB_t *task;
    TCB_t *selected;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (mutex->owner != current_tcb)
    {
        __set_PRIMASK(primask);
        return 0U;
    }

    mutex->lock_count--;
    if (mutex->lock_count != 0U)
    {
        __set_PRIMASK(primask);
        return 1U;
    }

    task = mutex->wait_head;
    if (task == NULL)
    {
        pt_mutex_remove_owned_locked(mutex, current_tcb);
        mutex->owner = NULL;
        pt_mutex_recalculate_priority_locked(current_tcb);
        __set_PRIMASK(primask);
        return 1U;
    }

    mutex->wait_head = task->mutex_next;
    if (mutex->wait_head == NULL)
    {
        mutex->wait_tail = NULL;
    }

    pt_mutex_remove_owned_locked(mutex, current_tcb);
    task->mutex_next = NULL;
    task->waiting_mutex = NULL;
    task->wait_deadline = PT_WAIT_FOREVER;
    task->wait_result = 1U;
    task->state = TASK_READY;
    mutex->owner = task;
    mutex->lock_count = 1U;
    pt_mutex_add_owned_locked(mutex, task);
    pt_mutex_recalculate_priority_locked(current_tcb);
    pt_mutex_recalculate_priority_locked(task);

    if (current_tcb != &idle_tcb &&
        current_tcb->state == TASK_RUNNING)
    {
        current_tcb->state = TASK_READY;
    }

    selected = pt_thread_schedule();
    pt_thread_context_switch(selected);
    __set_PRIMASK(primask);
    return 1U;
}

void pt_kernel_start(void)
{
    __disable_irq();
    next_tcb = pt_thread_schedule();
    next_tcb->state = TASK_RUNNING;
    pt_thread_start_first = 1U;
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
    __enable_irq();

    while (1)
    {
    }
}

void pt_thread_sleep(uint32_t ticks)
{
    TCB_t *selected;

    if (ticks == 0U)
    {
        pt_thread_relinquish();
        return;
    }

    __disable_irq();
    current_tcb->wake_tick = os_tick + ticks;
    current_tcb->waiting_semaphore = NULL;
    current_tcb->waiting_mutex = NULL;
    current_tcb->wait_deadline = PT_WAIT_FOREVER;
    current_tcb->state = TASK_BLOCKED;
    selected = pt_thread_schedule();
    pt_thread_context_switch(selected);
    __enable_irq();
}

void pt_thread_relinquish(void)
{
    TCB_t *selected;

    __disable_irq();
    if (current_tcb != &idle_tcb)
    {
        current_tcb->state = TASK_READY;
    }
    selected = pt_thread_schedule();
    pt_thread_context_switch(selected);
    __enable_irq();
}

void pt_timer_interrupt(void)
{
    uint32_t i;
    TCB_t *selected;

    for (i = 0U; i < NORMAL_TASK_COUNT; ++i)
    {
        TCB_t *task = task_list[i];
        if (task->state == TASK_BLOCKED &&
            task->waiting_semaphore == NULL &&
            task->waiting_mutex == NULL &&
            (int32_t)(os_tick - task->wake_tick) >= 0)
        {
            task->state = TASK_READY;
        }
        else if (task->state == TASK_BLOCKED &&
                 task->waiting_mutex != NULL &&
                 task->wait_deadline != PT_WAIT_FOREVER &&
                 (int32_t)(os_tick - task->wait_deadline) >= 0)
        {
            pt_mutex_t *mutex = task->waiting_mutex;
            pt_mutex_remove_waiter_locked(mutex, task);
            if (mutex->owner != NULL)
            {
                pt_mutex_recalculate_priority_locked(mutex->owner);
            }
            task->waiting_mutex = NULL;
            task->wait_deadline = PT_WAIT_FOREVER;
            task->wait_result = 0U;
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

    selected = pt_thread_schedule();
    pt_thread_context_switch(selected);
}
