#include "bsp_soil_moisture.h"
#include "adc.h"
#include <stddef.h>

#define BSP_SOIL_ADC_SAMPLE_COUNT 16U
#define BSP_SOIL_ADC_FULL_SCALE 4095U
#define BSP_SOIL_ADC_CONVERSION_TIMEOUT_MS 10U

uint8_t BSP_SoilMoisture_Init(void)
{
  return (HAL_ADCEx_Calibration_Start(&hadc1) == HAL_OK) ? 1U : 0U;
}

static uint8_t BSP_SoilMoisture_ReadAverage(uint32_t *adc_value)
{
  uint32_t sum = 0U;
  uint32_t index;

  if (adc_value == NULL)
  {
    return 0U;
  }

  for (index = 0U; index < BSP_SOIL_ADC_SAMPLE_COUNT; index++)
  {
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
      return 0U;
    }

    if (HAL_ADC_PollForConversion(
            &hadc1, BSP_SOIL_ADC_CONVERSION_TIMEOUT_MS) != HAL_OK)
    {
      (void)HAL_ADC_Stop(&hadc1);
      return 0U;
    }

    sum += HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);
  }

  *adc_value = sum / BSP_SOIL_ADC_SAMPLE_COUNT;
  return 1U;
}

static uint8_t BSP_SoilMoisture_ConvertPercent(uint32_t adc_value)
{
  if (adc_value >= BSP_SOIL_DRY_ADC_VALUE)
  {
    return 0U;
  }

  if (adc_value <= BSP_SOIL_WET_ADC_VALUE)
  {
    return 100U;
  }

  return (uint8_t)(((BSP_SOIL_DRY_ADC_VALUE - adc_value) * 100U) /
                   (BSP_SOIL_DRY_ADC_VALUE - BSP_SOIL_WET_ADC_VALUE));
}

uint8_t BSP_SoilMoisture_Read(BSP_SoilMoisture_Data *data)
{
  uint32_t adc_value;

  if (data == NULL)
  {
    return 0U;
  }

  if (BSP_SoilMoisture_ReadAverage(&adc_value) == 0U)
  {
    return 0U;
  }

  data->adc_value = adc_value;
  data->millivolt =
      ((adc_value * BSP_SOIL_ADC_REFERENCE_MV) +
       (BSP_SOIL_ADC_FULL_SCALE / 2U)) /
      BSP_SOIL_ADC_FULL_SCALE;
  data->moisture_percent = BSP_SoilMoisture_ConvertPercent(adc_value);

  return 1U;
}
