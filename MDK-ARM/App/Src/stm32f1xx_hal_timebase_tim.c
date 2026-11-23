#include "stm32f1xx_hal.h"

static TIM_HandleTypeDef s_hal_timebase_tim;

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef clock_config;
  uint32_t timer_clock;
  uint32_t apb1_prescaler;
  uint32_t prescaler;
  uint32_t flash_latency;

  __HAL_RCC_TIM4_CLK_ENABLE();
  HAL_NVIC_SetPriority(TIM4_IRQn, TickPriority, 0U);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);

  HAL_RCC_GetClockConfig(&clock_config, &flash_latency);
  apb1_prescaler = clock_config.APB1CLKDivider;
  timer_clock = (apb1_prescaler == RCC_HCLK_DIV1)
                    ? HAL_RCC_GetPCLK1Freq()
                    : (2U * HAL_RCC_GetPCLK1Freq());
  prescaler = (timer_clock / 1000000U) - 1U;

  s_hal_timebase_tim.Instance = TIM4;
  s_hal_timebase_tim.Init.Prescaler = prescaler;
  s_hal_timebase_tim.Init.CounterMode = TIM_COUNTERMODE_UP;
  s_hal_timebase_tim.Init.Period = 999U;
  s_hal_timebase_tim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  s_hal_timebase_tim.Init.AutoReloadPreload =
      TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&s_hal_timebase_tim) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_TIM_Base_Start_IT(&s_hal_timebase_tim);
}

void HAL_SuspendTick(void)
{
  __HAL_TIM_DISABLE_IT(&s_hal_timebase_tim, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
  __HAL_TIM_ENABLE_IT(&s_hal_timebase_tim, TIM_IT_UPDATE);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
}

void TIM4_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&s_hal_timebase_tim);
}
