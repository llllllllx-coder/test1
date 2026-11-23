#ifndef BSP_OLED_H
#define BSP_OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t BSP_OLED_Init(void);
void BSP_OLED_Clear(void);
void BSP_OLED_SetCursor(uint8_t page, uint8_t column);
void BSP_OLED_WriteString(uint8_t page, uint8_t column, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* BSP_OLED_H */
