# STM32H743 最小操作系统实现步骤

目标：在 STM32H743 上实现两个任务的时间片轮转切换。

## 1. 准备启动环境

确认工程已经能够正常启动，并了解以下内容：

- `startup_stm32h743xx.s`
- 链接脚本 `.ld`
- `Reset_Handler`
- 中断向量表
- `main()`

启动文件负责设置初始栈、调用系统初始化函数和进入 `main()`。

## 2. 定义任务控制块

为每个任务保存最基本的信息：

```c
typedef struct
{
    uint32_t *sp;
} TCB_t;
```

`sp` 保存任务当前的栈位置。

## 3. 创建任务栈

每个任务必须拥有独立的栈：

```c
#define TASK_STACK_WORDS 128

static uint32_t task1_stack[TASK_STACK_WORDS];
static uint32_t task2_stack[TASK_STACK_WORDS];
```

任务栈需要满足 8 字节对齐要求。

## 4. 编写任务函数

先使用两个最简单的任务：

```c
static void task1(void)
{
    while (1)
    {
        /* 翻转 GPIO 或执行测试代码 */
    }
}

static void task2(void)
{
    while (1)
    {
        /* 翻转另一个 GPIO 或执行测试代码 */
    }
}
```

任务函数通常不能直接返回。

## 5. 初始化任务栈帧

在任务栈中伪造 Cortex-M 异常栈帧：

```text
R0
R1
R2
R3
R12
LR
PC
xPSR
```

其中：

- `PC` 设置为任务函数地址；
- `xPSR` 设置为 `0x01000000`，表示 Thumb 状态；
- 额外预留 `R4-R11` 的软件保存空间。

这样恢复任务时，CPU 可以直接跳转到任务函数。

## 6. 初始化 TCB 和当前任务

创建两个 TCB，并保存各自的初始栈指针：

```c
task1_tcb.sp = build_stack(task1_stack, TASK_STACK_WORDS, task1);
task2_tcb.sp = build_stack(task2_stack, TASK_STACK_WORDS, task2);

current_tcb = &task1_tcb;
next_tcb = &task2_tcb;
```

## 7. 编写最简单的调度器

先使用轮转调度：

```text
task1 -> task2 -> task1 -> task2
```

调度器只需要完成：

1. 保存当前任务；
2. 选择下一个任务；
3. 恢复下一个任务。

## 8. 配置 SysTick

SysTick 用于产生周期性系统节拍，例如每 1 ms 触发一次。

每次 SysTick 到来时，请求 PendSV：

```c
void SysTick_Handler(void)
{
    HAL_IncTick();

    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}
```

PendSV 适合执行任务切换，因为它通常设置为最低异常优先级。

## 9. 编写 PendSV 上下文切换

`PendSV_Handler` 通常使用汇编实现：

1. 读取当前任务的 PSP；
2. 保存当前任务的 `R4-R11`；
3. 把新的栈指针写入当前 TCB；
4. 选择下一个 TCB；
5. 恢复下一个任务的 `R4-R11`；
6. 写入下一个任务的 PSP；
7. 使用异常返回指令恢复任务。

任务切换的核心是保存和恢复任务栈指针。

## 10. 启动第一个任务

系统初始化完成后，使用 SVC 启动第一个任务：

```asm
svc 0
```

`SVC_Handler` 负责加载第一个任务的栈指针，并返回到任务函数。

## 11. 设置异常优先级

建议使用以下优先级关系：

```text
PendSV  最低
SysTick 高于 PendSV
普通外设中断 根据需要设置
```

这样可以避免任务切换打断其他重要中断。

## 12. 测试任务切换

使用 GPIO、计数器或调试器验证：

- task1 是否能够运行；
- task2 是否能够运行；
- SysTick 是否周期性触发；
- PendSV 是否执行；
- 两个任务的局部变量是否互不影响；
- 任务切换后寄存器和栈是否正确恢复。

## 13. 增加阻塞和延时

基础切换稳定后，再增加：

- `delay`；
- 任务状态；
- 就绪队列；
- 阻塞队列；
- 超时唤醒。

## 14. 增加同步机制

之后可以实现：

- 临界区；
- 信号量；
- 互斥锁；
- 消息队列；
- 事件标志组。

## 15. 增加设备驱动和错误处理

最后再接入：

- GPIO；
- UART；
- 定时器；
- DMA；
- 缓存和 MPU；
- HardFault 错误分析。

## 最小系统必须具备的部分

```text
启动文件
    ↓
TCB
    ↓
任务独立栈
    ↓
任务初始栈帧
    ↓
SysTick
    ↓
PendSV
    ↓
汇编上下文切换
    ↓
两个任务轮转运行
```

