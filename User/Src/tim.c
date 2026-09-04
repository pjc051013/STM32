/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.c
  * @brief   Timer helpers and callback for blinking LEDs
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;

/* Period elapse callback toggles LEDs */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
    /* TIM1 update: toggle TEST_0 */
    HAL_GPIO_TogglePin(TEST_0_GPIO_Port, TEST_0_Pin);
  }
  else if (htim->Instance == TIM2)
  {
    /* TIM2 update: toggle TEST_1 */
    HAL_GPIO_TogglePin(TEST_1_GPIO_Port, TEST_1_Pin);
  }
}
