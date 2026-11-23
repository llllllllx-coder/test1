#include "bsp_dht11.h"
#include "main.h"
#include <stddef.h>

#define BSP_DHT11_START_LOW_MS 20U
#define BSP_DHT11_RESPONSE_TIMEOUT_US 120U
#define BSP_DHT11_BIT_TIMEOUT_US 100U
#define BSP_DHT11_BIT_SAMPLE_US 40U
#define BSP_DHT11_DATA_BYTES 5U
#define BSP_DHT11_DATA_BITS 40U

static uint32_t s_cycles_per_us;
static uint8_t s_dwt_ready;

static void BSP_DHT11_SetOutputMode(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = DHT11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

static void BSP_DHT11_SetInputMode(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = DHT11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

static uint8_t BSP_DHT11_LineIsHigh(void)
{
  return ((DHT11_GPIO_Port->IDR & DHT11_Pin) != 0U) ? 1U : 0U;
}

static void BSP_DHT11_DelayCycles(uint32_t cycles)
{
  uint32_t start = DWT->CYCCNT;

  while ((uint32_t)(DWT->CYCCNT - start) < cycles)
  {
  }
}

static void BSP_DHT11_DelayUs(uint32_t microseconds)
{
  BSP_DHT11_DelayCycles(microseconds * s_cycles_per_us);
}

static uint8_t BSP_DHT11_WaitForLevel(uint8_t high_level,
                                      uint32_t timeout_us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t timeout_cycles = timeout_us * s_cycles_per_us;

  while (BSP_DHT11_LineIsHigh() != high_level)
  {
    if ((uint32_t)(DWT->CYCCNT - start) > timeout_cycles)
    {
      return 0U;
    }
  }

  return 1U;
}

static uint8_t BSP_DHT11_ReadRaw(uint8_t raw[BSP_DHT11_DATA_BYTES])
{
  uint32_t primask;
  uint32_t index;
  uint8_t byte_index;
  uint8_t bit_mask;

  BSP_DHT11_SetOutputMode();
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
  HAL_Delay(BSP_DHT11_START_LOW_MS);

  BSP_DHT11_SetInputMode();
  primask = __get_PRIMASK();
  __disable_irq();

  if ((BSP_DHT11_WaitForLevel(0U,
                              BSP_DHT11_RESPONSE_TIMEOUT_US) == 0U) ||
      (BSP_DHT11_WaitForLevel(1U,
                              BSP_DHT11_RESPONSE_TIMEOUT_US) == 0U))
  {
    __set_PRIMASK(primask);
    return 0U;
  }

  for (index = 0U; index < BSP_DHT11_DATA_BITS; index++)
  {
    if ((BSP_DHT11_WaitForLevel(0U, BSP_DHT11_BIT_TIMEOUT_US) == 0U) ||
        (BSP_DHT11_WaitForLevel(1U, BSP_DHT11_BIT_TIMEOUT_US) == 0U))
    {
      __set_PRIMASK(primask);
      return 0U;
    }

    BSP_DHT11_DelayUs(BSP_DHT11_BIT_SAMPLE_US);
    byte_index = (uint8_t)(index / 8U);
    bit_mask = (uint8_t)(1U << (7U - (index % 8U)));

    if (BSP_DHT11_LineIsHigh() != 0U)
    {
      raw[byte_index] |= bit_mask;
    }
  }

  __set_PRIMASK(primask);
  return 1U;
}

uint8_t BSP_DHT11_Init(void)
{
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);
  BSP_DHT11_SetOutputMode();

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  s_cycles_per_us = SystemCoreClock / 1000000U;
  if (s_cycles_per_us == 0U)
  {
    s_dwt_ready = 0U;
    return 0U;
  }

  s_dwt_ready =
      ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U) ? 1U : 0U;

  return s_dwt_ready;
}

uint8_t BSP_DHT11_Read(BSP_DHT11_Data *data)
{
  uint8_t raw[BSP_DHT11_DATA_BYTES] = {0};
  uint8_t checksum;

  if ((data == NULL) || (s_dwt_ready == 0U))
  {
    return 0U;
  }

  if (BSP_DHT11_ReadRaw(raw) == 0U)
  {
    return 0U;
  }

  checksum = (uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]);
  if (checksum != raw[4])
  {
    return 0U;
  }

  data->humidity_percent = raw[0];
  data->temperature_celsius = raw[2];
  return 1U;
}
