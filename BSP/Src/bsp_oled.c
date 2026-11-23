#include "bsp_oled.h"
#include "i2c.h"
#include <stddef.h>
#include <string.h>

#define BSP_OLED_ADDRESS_3C 0x3CU
#define BSP_OLED_ADDRESS_3D 0x3DU
#define BSP_OLED_I2C_TIMEOUT 100U

typedef struct
{
  char character;
  uint8_t columns[5];
} BSP_OLED_Glyph;

static const BSP_OLED_Glyph s_oled_glyphs[] =
{
  { ' ', { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U } },
  { ':', { 0x00U, 0x36U, 0x36U, 0x00U, 0x00U } },
  { '0', { 0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU } },
  { '1', { 0x00U, 0x42U, 0x7FU, 0x40U, 0x00U } },
  { '2', { 0x42U, 0x61U, 0x51U, 0x49U, 0x46U } },
  { '3', { 0x21U, 0x41U, 0x45U, 0x4BU, 0x31U } },
  { '4', { 0x18U, 0x14U, 0x12U, 0x7FU, 0x10U } },
  { '5', { 0x27U, 0x45U, 0x45U, 0x45U, 0x39U } },
  { '6', { 0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U } },
  { '7', { 0x01U, 0x71U, 0x09U, 0x05U, 0x03U } },
  { '8', { 0x36U, 0x49U, 0x49U, 0x49U, 0x36U } },
  { '9', { 0x06U, 0x49U, 0x49U, 0x29U, 0x1EU } },
  { '-', { 0x08U, 0x08U, 0x08U, 0x08U, 0x08U } },
  { 'A', { 0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU } },
  { 'B', { 0x7FU, 0x49U, 0x49U, 0x49U, 0x36U } },
  { 'C', { 0x3EU, 0x41U, 0x41U, 0x41U, 0x22U } },
  { 'D', { 0x7FU, 0x41U, 0x41U, 0x41U, 0x3EU } },
  { 'E', { 0x7FU, 0x49U, 0x49U, 0x49U, 0x41U } },
  { 'F', { 0x7FU, 0x09U, 0x09U, 0x09U, 0x01U } },
  { 'H', { 0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU } },
  { 'I', { 0x00U, 0x41U, 0x7FU, 0x41U, 0x00U } },
  { 'L', { 0x7FU, 0x40U, 0x40U, 0x40U, 0x40U } },
  { 'M', { 0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU } },
  { 'N', { 0x7FU, 0x02U, 0x04U, 0x08U, 0x7FU } },
  { 'O', { 0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU } },
  { 'P', { 0x7FU, 0x09U, 0x09U, 0x09U, 0x06U } },
  { 'R', { 0x7FU, 0x09U, 0x19U, 0x29U, 0x46U } },
  { 'S', { 0x46U, 0x49U, 0x49U, 0x49U, 0x31U } },
  { 'T', { 0x01U, 0x01U, 0x7FU, 0x01U, 0x01U } },
  { 'U', { 0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU } },
  { 'W', { 0x7FU, 0x20U, 0x18U, 0x20U, 0x7FU } },
  { 'X', { 0x63U, 0x14U, 0x08U, 0x14U, 0x63U } },
  { '%', { 0x23U, 0x13U, 0x08U, 0x64U, 0x62U } }
};

static uint16_t s_oled_address;
static uint8_t s_oled_ready;

static uint8_t BSP_OLED_WriteCommands(const uint8_t *commands, uint16_t length)
{
  uint8_t buffer[32];

  if ((length == 0U) || (length > (sizeof(buffer) - 1U)))
  {
    return 0U;
  }

  buffer[0] = 0x00U;
  memcpy(&buffer[1], commands, length);

  return (HAL_I2C_Master_Transmit(&hi2c1,
                                  s_oled_address,
                                  buffer,
                                  (uint16_t)(length + 1U),
                                  BSP_OLED_I2C_TIMEOUT) == HAL_OK) ? 1U : 0U;
}

static const uint8_t *BSP_OLED_FindGlyph(char character)
{
  static const uint8_t blank[5] = { 0U, 0U, 0U, 0U, 0U };
  uint32_t index;

  for (index = 0U; index < (sizeof(s_oled_glyphs) / sizeof(s_oled_glyphs[0]));
       index++)
  {
    if (s_oled_glyphs[index].character == character)
    {
      return s_oled_glyphs[index].columns;
    }
  }

  return blank;
}

uint8_t BSP_OLED_Init(void)
{
  static const uint8_t init_commands[] =
  {
    0xAEU,
    0xD5U, 0x80U,
    0xA8U, 0x3FU,
    0xD3U, 0x00U,
    0x40U,
    0x8DU, 0x14U,
    0x20U, 0x00U,
    0xA1U,
    0xC8U,
    0xDAU, 0x12U,
    0x81U, 0xCFU,
    0xD9U, 0xF1U,
    0xDBU, 0x40U,
    0xA4U,
    0xA6U,
    0x2EU,
    0xAFU
  };
  const uint16_t addresses[] =
  {
    (uint16_t)(BSP_OLED_ADDRESS_3C << 1U),
    (uint16_t)(BSP_OLED_ADDRESS_3D << 1U)
  };
  uint32_t index;

  s_oled_ready = 0U;

  for (index = 0U; index < (sizeof(addresses) / sizeof(addresses[0])); index++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1,
                              addresses[index],
                              3U,
                              BSP_OLED_I2C_TIMEOUT) == HAL_OK)
    {
      s_oled_address = addresses[index];
      break;
    }
  }

  if (index >= (sizeof(addresses) / sizeof(addresses[0])))
  {
    return 0U;
  }

  if (BSP_OLED_WriteCommands(init_commands,
                             (uint16_t)sizeof(init_commands)) == 0U)
  {
    return 0U;
  }

  s_oled_ready = 1U;
  BSP_OLED_Clear();

  return 1U;
}

void BSP_OLED_Clear(void)
{
  uint8_t page;
  uint8_t payload[129];
  uint32_t index;

  if (s_oled_ready == 0U)
  {
    return;
  }

  payload[0] = 0x40U;
  for (index = 1U; index < sizeof(payload); index++)
  {
    payload[index] = 0x00U;
  }

  for (page = 0U; page < 8U; page++)
  {
    BSP_OLED_SetCursor(page, 0U);
    (void)HAL_I2C_Master_Transmit(&hi2c1,
                                  s_oled_address,
                                  payload,
                                  (uint16_t)sizeof(payload),
                                  BSP_OLED_I2C_TIMEOUT);
  }
}

void BSP_OLED_SetCursor(uint8_t page, uint8_t column)
{
  uint8_t commands[3];

  if ((s_oled_ready == 0U) || (page > 7U) || (column > 127U))
  {
    return;
  }

  commands[0] = (uint8_t)(0xB0U | page);
  commands[1] = (uint8_t)(column & 0x0FU);
  commands[2] = (uint8_t)(0x10U | ((column >> 4U) & 0x0FU));
  (void)BSP_OLED_WriteCommands(commands, (uint16_t)sizeof(commands));
}

void BSP_OLED_WriteString(uint8_t page, uint8_t column, const char *text)
{
  uint8_t payload[6];
  const uint8_t *glyph;

  if ((s_oled_ready == 0U) || (text == NULL) || (column > 121U))
  {
    return;
  }

  BSP_OLED_SetCursor(page, column);
  payload[0] = 0x40U;
  payload[5] = 0x00U;

  while ((*text != '\0') && (column <= 121U))
  {
    glyph = BSP_OLED_FindGlyph(*text);
    memcpy(&payload[1], glyph, 5U);

    if (HAL_I2C_Master_Transmit(&hi2c1,
                                s_oled_address,
                                payload,
                                (uint16_t)sizeof(payload),
                                BSP_OLED_I2C_TIMEOUT) != HAL_OK)
    {
      return;
    }

    column = (uint8_t)(column + 6U);
    text++;
  }
}
