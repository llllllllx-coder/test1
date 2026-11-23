#ifndef BSP_BH1750_H
#define BSP_BH1750_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t BSP_BH1750_Init(void);
uint8_t BSP_BH1750_ReadLux(uint16_t *lux);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BH1750_H */
