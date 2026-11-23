#ifndef BSP_DHT11_H
#define BSP_DHT11_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  uint8_t humidity_percent;
  uint8_t temperature_celsius;
} BSP_DHT11_Data;

uint8_t BSP_DHT11_Init(void);
uint8_t BSP_DHT11_Read(BSP_DHT11_Data *data);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DHT11_H */
