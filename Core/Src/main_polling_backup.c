/* USER CODE BEGIN Header */
/* Legacy polling implementation retained only for migration reference. */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_bh1750.h"
#include "bsp_dht11.h"
#include "bsp_oled.h"
#include "bsp_soil_moisture.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
  uint8_t soil_valid;
  uint8_t soil_moisture_percent;
  uint8_t dht11_valid;
  uint8_t temperature_celsius;
  uint8_t humidity_percent;
  uint8_t lux_valid;
  uint16_t lux;
  uint8_t led_supplement_on;
  uint8_t fan_alert_on;
  uint8_t watering_alert_on;
} AppPlantState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DHT11_READ_PERIOD_MS 2000U

/*
 * Defaults for a general indoor foliage plant:
 * - Add light below 1000 lux and stop above 1500 lux.
 * - Request fan above 30 C and stop below 28 C.
 * - Request watering at or below 30% soil moisture and clear at 40%.
 */
#define PLANT_LIGHT_START_LUX 1000U
#define PLANT_LIGHT_STOP_LUX 1500U
#define PLANT_HIGH_TEMP_START_C 30U
#define PLANT_HIGH_TEMP_STOP_C 28U
#define PLANT_LOW_SOIL_START_PERCENT 30U
#define PLANT_LOW_SOIL_STOP_PERCENT 40U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static uint32_t s_last_second_tick;
static uint32_t s_last_dht11_tick;
static AppPlantState s_plant_state;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void App_ReadSoil(AppPlantState *state);
static void App_ReadDht11(AppPlantState *state);
static void App_ReadLight(AppPlantState *state);
static void App_UpdateDisplay(const AppPlantState *state);
static void App_PrintStatus(const AppPlantState *state);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void App_SetSupplementLed(uint8_t enabled)
{
  /* The onboard PC13 LED is active low. */
  HAL_GPIO_WritePin(LED_GPIO_Port,
                    LED_Pin,
                    (enabled != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void App_InitPlantState(AppPlantState *state)
{
  state->soil_valid = 0U;
  state->soil_moisture_percent = 0U;
  state->dht11_valid = 0U;
  state->temperature_celsius = 0U;
  state->humidity_percent = 0U;
  state->lux_valid = 0U;
  state->lux = 0U;
  state->led_supplement_on = 0U;
  state->fan_alert_on = 0U;
  state->watering_alert_on = 0U;
  App_SetSupplementLed(0U);
}

static void App_UartTransmit(const char *text, int length)
{
  if ((text != NULL) && (length > 0))
  {
    (void)HAL_UART_Transmit(&huart1,
                            (uint8_t *)text,
                            (uint16_t)length,
                            100U);
  }
}

static void App_ReadSoil(AppPlantState *state)
{
  BSP_SoilMoisture_Data data;

  if (BSP_SoilMoisture_Read(&data) == 0U)
  {
    state->soil_valid = 0U;
    return;
  }

  state->soil_valid = 1U;
  state->soil_moisture_percent = data.moisture_percent;

  if ((state->watering_alert_on == 0U) &&
      (data.moisture_percent <= PLANT_LOW_SOIL_START_PERCENT))
  {
    state->watering_alert_on = 1U;
  }
  else if ((state->watering_alert_on != 0U) &&
           (data.moisture_percent >= PLANT_LOW_SOIL_STOP_PERCENT))
  {
    state->watering_alert_on = 0U;
  }
}

static void App_ReadDht11(AppPlantState *state)
{
  BSP_DHT11_Data data;

  if (BSP_DHT11_Read(&data) == 0U)
  {
    state->dht11_valid = 0U;
    return;
  }

  state->dht11_valid = 1U;
  state->temperature_celsius = data.temperature_celsius;
  state->humidity_percent = data.humidity_percent;

  if ((state->fan_alert_on == 0U) &&
      (data.temperature_celsius >= PLANT_HIGH_TEMP_START_C))
  {
    state->fan_alert_on = 1U;
  }
  else if ((state->fan_alert_on != 0U) &&
           (data.temperature_celsius <= PLANT_HIGH_TEMP_STOP_C))
  {
    state->fan_alert_on = 0U;
  }
}

static void App_ReadLight(AppPlantState *state)
{
  uint16_t lux;

  if (BSP_BH1750_ReadLux(&lux) == 0U)
  {
    state->lux_valid = 0U;
    state->led_supplement_on = 0U;
    App_SetSupplementLed(0U);
    return;
  }

  state->lux_valid = 1U;
  state->lux = lux;

  if ((state->led_supplement_on == 0U) &&
      (lux < PLANT_LIGHT_START_LUX))
  {
    state->led_supplement_on = 1U;
  }
  else if ((state->led_supplement_on != 0U) &&
           (lux >= PLANT_LIGHT_STOP_LUX))
  {
    state->led_supplement_on = 0U;
  }

  App_SetSupplementLed(state->led_supplement_on);
}

static void App_UpdateDisplay(const AppPlantState *state)
{
  char display_text[24];
  int display_length;

  if (state->dht11_valid != 0U)
  {
    display_length = snprintf(
        display_text,
        sizeof(display_text),
        "TEMP %2uC HUM %2u%%",
        (unsigned int)state->temperature_celsius,
        (unsigned int)state->humidity_percent);
  }
  else
  {
    display_length = snprintf(display_text,
                              sizeof(display_text),
                              "TEMP --C HUM --%%");
  }
  if (display_length > 0)
  {
    BSP_OLED_WriteString(0U, 0U, display_text);
  }

  if (state->lux_valid != 0U)
  {
    display_length = snprintf(display_text,
                              sizeof(display_text),
                              "LUX %5u",
                              (unsigned int)state->lux);
  }
  else
  {
    display_length = snprintf(display_text,
                              sizeof(display_text),
                              "LUX ERROR");
  }
  if (display_length > 0)
  {
    BSP_OLED_WriteString(1U, 0U, display_text);
  }

  if (state->soil_valid != 0U)
  {
    display_length = snprintf(
        display_text,
        sizeof(display_text),
        "SOIL %3u%%",
        (unsigned int)state->soil_moisture_percent);
  }
  else
  {
    display_length = snprintf(display_text,
                              sizeof(display_text),
                              "SOIL ERROR");
  }
  if (display_length > 0)
  {
    BSP_OLED_WriteString(2U, 0U, display_text);
  }

  BSP_OLED_WriteString(
      3U,
      0U,
      (state->led_supplement_on != 0U) ? "LED ON         "
                                       : "LED OFF        ");
  BSP_OLED_WriteString(
      4U,
      0U,
      (state->fan_alert_on != 0U) ? "FAN ON         "
                                  : "FAN OFF        ");
  BSP_OLED_WriteString(
      5U,
      0U,
      (state->watering_alert_on != 0U) ? "WATER ON       "
                                       : "WATER OFF      ");
}

static void App_PrintStatus(const AppPlantState *state)
{
  char status_text[128];
  char temperature_text[8] = "ERR";
  char humidity_text[8] = "ERR";
  char lux_text[8] = "ERR";
  char soil_text[8] = "ERR";
  int status_length;

  if (state->dht11_valid != 0U)
  {
    (void)snprintf(temperature_text,
                   sizeof(temperature_text),
                   "%uC",
                   (unsigned int)state->temperature_celsius);
    (void)snprintf(humidity_text,
                   sizeof(humidity_text),
                   "%u%%",
                   (unsigned int)state->humidity_percent);
  }

  if (state->lux_valid != 0U)
  {
    (void)snprintf(lux_text,
                   sizeof(lux_text),
                   "%u",
                   (unsigned int)state->lux);
  }

  if (state->soil_valid != 0U)
  {
    (void)snprintf(soil_text,
                   sizeof(soil_text),
                   "%u%%",
                   (unsigned int)state->soil_moisture_percent);
  }

  status_length = snprintf(
      status_text,
      sizeof(status_text),
      "DATA,TEMP=%s,HUM=%s,LUX=%s,SOIL=%s,LED=%s,FAN=%s,WATER=%s\r\n",
      temperature_text,
      humidity_text,
      lux_text,
      soil_text,
      (state->led_supplement_on != 0U) ? "ON" : "OFF",
      (state->fan_alert_on != 0U) ? "ON" : "OFF",
      (state->watering_alert_on != 0U) ? "ON" : "OFF");
  if (status_length >= (int)sizeof(status_text))
  {
    status_length = (int)sizeof(status_text) - 1;
  }
  App_UartTransmit(status_text, status_length);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  if (BSP_SoilMoisture_Init() == 0U)
  {
    Error_Handler();
  }

  App_InitPlantState(&s_plant_state);
  (void)BSP_OLED_Init();
  (void)BSP_DHT11_Init();
  (void)BSP_BH1750_Init();
  App_ReadSoil(&s_plant_state);
  App_UpdateDisplay(&s_plant_state);

  /* DHT11 requires about one second after power-up before the first read. */
  HAL_Delay(1000U);
  App_ReadDht11(&s_plant_state);
  App_ReadLight(&s_plant_state);
  App_UpdateDisplay(&s_plant_state);
  App_PrintStatus(&s_plant_state);
  s_last_dht11_tick = HAL_GetTick();
  s_last_second_tick = s_last_dht11_tick;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now_tick;
    uint32_t elapsed_seconds;

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    now_tick = HAL_GetTick();
    elapsed_seconds = (uint32_t)(now_tick - s_last_second_tick) / 1000U;

    if (elapsed_seconds != 0U)
    {
      s_last_second_tick += elapsed_seconds * 1000U;
      App_ReadSoil(&s_plant_state);
      App_ReadLight(&s_plant_state);
      App_UpdateDisplay(&s_plant_state);
    }

    if ((uint32_t)(HAL_GetTick() - s_last_dht11_tick) >=
        DHT11_READ_PERIOD_MS)
    {
      App_ReadDht11(&s_plant_state);
      App_UpdateDisplay(&s_plant_state);
      App_PrintStatus(&s_plant_state);
      s_last_dht11_tick = HAL_GetTick();
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
