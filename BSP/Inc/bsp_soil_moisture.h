#ifndef BSP_SOIL_MOISTURE_H
#define BSP_SOIL_MOISTURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Adjust these values according to measurements from the actual sensor. */
#ifndef BSP_SOIL_ADC_REFERENCE_MV
#define BSP_SOIL_ADC_REFERENCE_MV 3300U
#endif

#ifndef BSP_SOIL_DRY_ADC_VALUE
#define BSP_SOIL_DRY_ADC_VALUE 3000U
#endif

#ifndef BSP_SOIL_WET_ADC_VALUE
#define BSP_SOIL_WET_ADC_VALUE 1100U
#endif

typedef struct
{
  uint32_t adc_value;
  uint32_t millivolt;
  uint8_t moisture_percent;
} BSP_SoilMoisture_Data;

uint8_t BSP_SoilMoisture_Init(void);
uint8_t BSP_SoilMoisture_Read(BSP_SoilMoisture_Data *data);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SOIL_MOISTURE_H */
