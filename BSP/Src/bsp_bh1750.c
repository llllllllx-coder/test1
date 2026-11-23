#include "bsp_bh1750.h"
#include "i2c.h"
#include <stddef.h>

#define BSP_BH1750_ADDRESS_LOW 0x23U
#define BSP_BH1750_ADDRESS_HIGH 0x5CU
#define BSP_BH1750_COMMAND_POWER_ON 0x01U
#define BSP_BH1750_COMMAND_CONTINUOUS_H_RES 0x10U
#define BSP_BH1750_I2C_TIMEOUT 100U

static uint16_t s_bh1750_address;
static uint8_t s_bh1750_ready;

static uint8_t BSP_BH1750_WriteCommand(uint8_t command)
{
  return (HAL_I2C_Master_Transmit(&hi2c2,
                                  s_bh1750_address,
                                  &command,
                                  1U,
                                  BSP_BH1750_I2C_TIMEOUT) == HAL_OK) ? 1U : 0U;
}

uint8_t BSP_BH1750_Init(void)
{
  const uint8_t addresses[] =
  {
    (uint8_t)(BSP_BH1750_ADDRESS_LOW << 1U),
    (uint8_t)(BSP_BH1750_ADDRESS_HIGH << 1U)
  };
  uint32_t index;

  s_bh1750_ready = 0U;

  for (index = 0U; index < (sizeof(addresses) / sizeof(addresses[0])); index++)
  {
    s_bh1750_address = addresses[index];
    if (HAL_I2C_IsDeviceReady(&hi2c2,
                              s_bh1750_address,
                              3U,
                              BSP_BH1750_I2C_TIMEOUT) != HAL_OK)
    {
      continue;
    }

    if ((BSP_BH1750_WriteCommand(BSP_BH1750_COMMAND_POWER_ON) == 0U) ||
        (BSP_BH1750_WriteCommand(
             BSP_BH1750_COMMAND_CONTINUOUS_H_RES) == 0U))
    {
      continue;
    }

    s_bh1750_ready = 1U;
    HAL_Delay(10U);
    return 1U;
  }

  return 0U;
}

uint8_t BSP_BH1750_ReadLux(uint16_t *lux)
{
  uint8_t buffer[2];
  uint32_t raw_value;
  uint32_t calculated_lux;

  if ((lux == NULL) || (s_bh1750_ready == 0U))
  {
    return 0U;
  }

  if (HAL_I2C_Master_Receive(&hi2c2,
                             s_bh1750_address,
                             buffer,
                             2U,
                             BSP_BH1750_I2C_TIMEOUT) != HAL_OK)
  {
    return 0U;
  }

  raw_value = ((uint32_t)buffer[0] << 8U) | buffer[1];
  calculated_lux = ((raw_value * 5U) + 3U) / 6U;
  *lux = (uint16_t)calculated_lux;
  return 1U;
}
