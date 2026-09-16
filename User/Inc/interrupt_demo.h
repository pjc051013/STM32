#ifndef __INTERRUPT_DEMO_H__
#define __INTERRUPT_DEMO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

/**
 * @brief 中断挂起与优先级抢占演示
 * 
 * 实验 1: 演示相同优先级时，同时挂起两个中断，按硬件中断号先后响应
 * 实验 2: 演示不同优先级时，同时挂起两个中断，高优先级先响应
 * 实验 3: 演示中断嵌套抢占（低优先级中断运行期间，高优先级中断打断它）
 */
void Interrupt_Demo_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* __INTERRUPT_DEMO_H__ */
