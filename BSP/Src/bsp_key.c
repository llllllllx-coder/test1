#include "bsp_key.h"
#include "main.h"

typedef enum
{
  BSP_KEY_STATE_RELEASED = 0U,
  BSP_KEY_STATE_PRESSED
} BSP_KeyState;

static volatile uint32_t s_edge_tick;
static volatile uint8_t s_edge_pending;
static BSP_KeyState s_key_state = BSP_KEY_STATE_RELEASED;

void BSP_KEY_OnExti(uint16_t gpio_pin)
{
  if (gpio_pin == KEY1_Pin)
  {
    s_edge_tick = HAL_GetTick();
    s_edge_pending = 1U;
  }
}

uint8_t BSP_KEY_Process(void)
{
  if (s_edge_pending == 0U)
  {
    if ((s_key_state == BSP_KEY_STATE_PRESSED) &&
        (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET))
    {
      s_key_state = BSP_KEY_STATE_RELEASED;
    }

    return 0U;
  }

  if ((uint32_t)(HAL_GetTick() - s_edge_tick) < BSP_KEY_DEBOUNCE_MS)
  {
    return 0U;
  }

  s_edge_pending = 0U;

  if ((s_key_state == BSP_KEY_STATE_RELEASED) &&
      (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET))
  {
    s_key_state = BSP_KEY_STATE_PRESSED;
    return 1U;
  }

  return 0U;
}
