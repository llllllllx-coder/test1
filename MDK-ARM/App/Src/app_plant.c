#include "app_plant.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "main.h"
#include "usart.h"
#include "bsp_bh1750.h"
#include "bsp_dht11.h"
#include "bsp_oled.h"
#include "bsp_soil_moisture.h"
#include <stddef.h>
#include <stdio.h>

#define PLANT_SENSOR_PERIOD_MS 500U
#define PLANT_DHT_PERIOD_CYCLES 4U
#define PLANT_DISPLAY_PERIOD_MS 500U
#define PLANT_COMM_PERIOD_MS 500U

#define PLANT_LIGHT_START_LUX 1000U
#define PLANT_LIGHT_STOP_LUX 1500U
#define PLANT_HIGH_TEMP_START_C 30U
#define PLANT_HIGH_TEMP_STOP_C 28U
#define PLANT_LOW_SOIL_START_PERCENT 30U
#define PLANT_LOW_SOIL_STOP_PERCENT 40U

#define PLANT_SENSOR_TASK_STACK 256U
#define PLANT_DISPLAY_TASK_STACK 320U
#define PLANT_COMM_TASK_STACK 384U

#define PLANT_SENSOR_TASK_PRIORITY (tskIDLE_PRIORITY + 4U)
#define PLANT_COMM_TASK_PRIORITY (tskIDLE_PRIORITY + 2U)
#define PLANT_DISPLAY_TASK_PRIORITY (tskIDLE_PRIORITY + 1U)

typedef struct
{
  uint32_t sequence;
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
} PlantSnapshot;

static PlantSnapshot s_snapshot;
static PlantSnapshot s_sensor_snapshot;
static SemaphoreHandle_t s_snapshot_mutex;

static void PlantSensorTask(void *argument);
static void PlantDisplayTask(void *argument);
static void PlantCommTask(void *argument);

static void PlantClearSnapshot(PlantSnapshot *snapshot)
{
  snapshot->sequence = 0U;
  snapshot->soil_valid = 0U;
  snapshot->soil_moisture_percent = 0U;
  snapshot->dht11_valid = 0U;
  snapshot->temperature_celsius = 0U;
  snapshot->humidity_percent = 0U;
  snapshot->lux_valid = 0U;
  snapshot->lux = 0U;
  snapshot->led_supplement_on = 0U;
  snapshot->fan_alert_on = 0U;
  snapshot->watering_alert_on = 0U;
}

static void PlantSetSupplementLed(uint8_t enabled)
{
  HAL_GPIO_WritePin(LED_GPIO_Port,
                    LED_Pin,
                    (enabled != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void PlantSnapshotCopy(PlantSnapshot *destination)
{
  if ((s_snapshot_mutex != NULL) &&
      (xSemaphoreTake(s_snapshot_mutex, portMAX_DELAY) == pdPASS))
  {
    *destination = s_snapshot;
    (void)xSemaphoreGive(s_snapshot_mutex);
  }
}

static void PlantSnapshotPublish(const PlantSnapshot *source)
{
  if ((s_snapshot_mutex != NULL) &&
      (xSemaphoreTake(s_snapshot_mutex, portMAX_DELAY) == pdPASS))
  {
    s_snapshot = *source;
    s_snapshot.sequence++;
    (void)xSemaphoreGive(s_snapshot_mutex);
  }
}

static void PlantReadSoil(PlantSnapshot *snapshot)
{
  BSP_SoilMoisture_Data data;

  if (BSP_SoilMoisture_Read(&data) == 0U)
  {
    snapshot->soil_valid = 0U;
    return;
  }

  snapshot->soil_valid = 1U;
  snapshot->soil_moisture_percent = data.moisture_percent;

  if ((snapshot->watering_alert_on == 0U) &&
      (data.moisture_percent <= PLANT_LOW_SOIL_START_PERCENT))
  {
    snapshot->watering_alert_on = 1U;
  }
  else if ((snapshot->watering_alert_on != 0U) &&
           (data.moisture_percent >= PLANT_LOW_SOIL_STOP_PERCENT))
  {
    snapshot->watering_alert_on = 0U;
  }
}

static void PlantReadDht11(PlantSnapshot *snapshot)
{
  BSP_DHT11_Data data;

  if (BSP_DHT11_Read(&data) == 0U)
  {
    snapshot->dht11_valid = 0U;
    return;
  }

  snapshot->dht11_valid = 1U;
  snapshot->temperature_celsius = data.temperature_celsius;
  snapshot->humidity_percent = data.humidity_percent;

  if ((snapshot->fan_alert_on == 0U) &&
      (data.temperature_celsius >= PLANT_HIGH_TEMP_START_C))
  {
    snapshot->fan_alert_on = 1U;
  }
  else if ((snapshot->fan_alert_on != 0U) &&
           (data.temperature_celsius <= PLANT_HIGH_TEMP_STOP_C))
  {
    snapshot->fan_alert_on = 0U;
  }
}

static void PlantReadLight(PlantSnapshot *snapshot)
{
  uint16_t lux;

  if (BSP_BH1750_ReadLux(&lux) == 0U)
  {
    snapshot->lux_valid = 0U;
    snapshot->led_supplement_on = 0U;
    PlantSetSupplementLed(0U);
    return;
  }

  snapshot->lux_valid = 1U;
  snapshot->lux = lux;

  if ((snapshot->led_supplement_on == 0U) &&
      (lux < PLANT_LIGHT_START_LUX))
  {
    snapshot->led_supplement_on = 1U;
  }
  else if ((snapshot->led_supplement_on != 0U) &&
           (lux >= PLANT_LIGHT_STOP_LUX))
  {
    snapshot->led_supplement_on = 0U;
  }

  PlantSetSupplementLed(snapshot->led_supplement_on);
}

static void PlantDrawSnapshot(const PlantSnapshot *snapshot)
{
  char display_text[24];

  if (snapshot->dht11_valid != 0U)
  {
    (void)snprintf(display_text,
                   sizeof(display_text),
                   "TEMP %2uC HUM %2u%%",
                   (unsigned int)snapshot->temperature_celsius,
                   (unsigned int)snapshot->humidity_percent);
  }
  else
  {
    (void)snprintf(display_text,
                   sizeof(display_text),
                   "TEMP --C HUM --%%");
  }
  BSP_OLED_WriteString(0U, 0U, display_text);

  if (snapshot->lux_valid != 0U)
  {
    (void)snprintf(display_text,
                   sizeof(display_text),
                   "LUX %5u",
                   (unsigned int)snapshot->lux);
  }
  else
  {
    (void)snprintf(display_text,
                   sizeof(display_text),
                   "LUX ERROR");
  }
  BSP_OLED_WriteString(1U, 0U, display_text);

  if (snapshot->soil_valid != 0U)
  {
    (void)snprintf(display_text,
                   sizeof(display_text),
                   "SOIL %3u%% ",
                   (unsigned int)snapshot->soil_moisture_percent);
  }
  else
  {
    (void)snprintf(display_text,
                   sizeof(display_text),
                   "SOIL ERROR");
  }
  BSP_OLED_WriteString(2U, 0U, display_text);

  BSP_OLED_WriteString(
      3U,
      0U,
      (snapshot->led_supplement_on != 0U) ? "LED ON         "
                                          : "LED OFF        ");
  BSP_OLED_WriteString(
      4U,
      0U,
      (snapshot->fan_alert_on != 0U) ? "FAN ON         "
                                     : "FAN OFF        ");
  BSP_OLED_WriteString(
      5U,
      0U,
      (snapshot->watering_alert_on != 0U) ? "WATER ON       "
                                          : "WATER OFF      ");
}

static void PlantPrintSnapshot(const PlantSnapshot *snapshot)
{
  char status_text[128];
  char temperature_text[8] = "ERR";
  char humidity_text[8] = "ERR";
  char lux_text[8] = "ERR";
  char soil_text[8] = "ERR";
  int status_length;

  if (snapshot->dht11_valid != 0U)
  {
    (void)snprintf(temperature_text,
                   sizeof(temperature_text),
                   "%uC",
                   (unsigned int)snapshot->temperature_celsius);
    (void)snprintf(humidity_text,
                   sizeof(humidity_text),
                   "%u%%",
                   (unsigned int)snapshot->humidity_percent);
  }
  if (snapshot->lux_valid != 0U)
  {
    (void)snprintf(lux_text,
                   sizeof(lux_text),
                   "%u",
                   (unsigned int)snapshot->lux);
  }
  if (snapshot->soil_valid != 0U)
  {
    (void)snprintf(soil_text,
                   sizeof(soil_text),
                   "%u%%",
                   (unsigned int)snapshot->soil_moisture_percent);
  }

  status_length = snprintf(
      status_text,
      sizeof(status_text),
      "DATA,TEMP=%s,HUM=%s,LUX=%s,SOIL=%s,LED=%s,FAN=%s,WATER=%s\r\n",
      temperature_text,
      humidity_text,
      lux_text,
      soil_text,
      (snapshot->led_supplement_on != 0U) ? "ON" : "OFF",
      (snapshot->fan_alert_on != 0U) ? "ON" : "OFF",
      (snapshot->watering_alert_on != 0U) ? "ON" : "OFF");
  if (status_length >= (int)sizeof(status_text))
  {
    status_length = (int)sizeof(status_text) - 1;
  }
  if (status_length > 0)
  {
    (void)HAL_UART_Transmit(&huart1,
                            (uint8_t *)status_text,
                            (uint16_t)status_length,
                            100U);
  }
}

static void PlantSensorTask(void *argument)
{
  TickType_t last_wake_time = xTaskGetTickCount();
  uint32_t cycle = 0U;

  (void)argument;
  PlantClearSnapshot(&s_sensor_snapshot);

  for (;;)
  {
    PlantReadSoil(&s_sensor_snapshot);
    PlantReadLight(&s_sensor_snapshot);

    if ((cycle != 0U) &&
        ((cycle % PLANT_DHT_PERIOD_CYCLES) == 0U))
    {
      PlantReadDht11(&s_sensor_snapshot);
    }

    PlantSnapshotPublish(&s_sensor_snapshot);
    cycle++;
    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(PLANT_SENSOR_PERIOD_MS));
  }
}

static void PlantDisplayTask(void *argument)
{
  PlantSnapshot local_snapshot;
  TickType_t last_wake_time;

  (void)argument;
  PlantClearSnapshot(&local_snapshot);
  last_wake_time = xTaskGetTickCount();

  for (;;)
  {
    PlantSnapshotCopy(&local_snapshot);
    PlantDrawSnapshot(&local_snapshot);
    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(PLANT_DISPLAY_PERIOD_MS));
  }
}

static void PlantCommTask(void *argument)
{
  PlantSnapshot local_snapshot;
  TickType_t last_wake_time;

  (void)argument;
  PlantClearSnapshot(&local_snapshot);
  vTaskDelay(pdMS_TO_TICKS(PLANT_COMM_PERIOD_MS));
  last_wake_time = xTaskGetTickCount();

  for (;;)
  {
    PlantSnapshotCopy(&local_snapshot);
    PlantPrintSnapshot(&local_snapshot);
    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(PLANT_COMM_PERIOD_MS));
  }
}

uint8_t AppPlant_Start(void)
{
  PlantClearSnapshot(&s_snapshot);
  PlantClearSnapshot(&s_sensor_snapshot);
  PlantSetSupplementLed(0U);

  if (BSP_SoilMoisture_Init() == 0U)
  {
    return 0U;
  }

  (void)BSP_OLED_Init();
  (void)BSP_DHT11_Init();
  (void)BSP_BH1750_Init();

  s_snapshot_mutex = xSemaphoreCreateMutex();
  if (s_snapshot_mutex == NULL)
  {
    return 0U;
  }

  if ((xTaskCreate(PlantSensorTask,
                   "Sensor",
                   PLANT_SENSOR_TASK_STACK,
                   NULL,
                   PLANT_SENSOR_TASK_PRIORITY,
                   NULL) != pdPASS) ||
      (xTaskCreate(PlantDisplayTask,
                   "Display",
                   PLANT_DISPLAY_TASK_STACK,
                   NULL,
                   PLANT_DISPLAY_TASK_PRIORITY,
                   NULL) != pdPASS) ||
      (xTaskCreate(PlantCommTask,
                   "Comm",
                   PLANT_COMM_TASK_STACK,
                   NULL,
                   PLANT_COMM_TASK_PRIORITY,
                   NULL) != pdPASS))
  {
    return 0U;
  }

  return 1U;
}

void vApplicationMallocFailedHook(void)
{
  taskDISABLE_INTERRUPTS();
  for (;;)
  {
  }
}

void vApplicationStackOverflowHook(TaskHandle_t task_handle,
                                   char *task_name)
{
  (void)task_handle;
  (void)task_name;
  taskDISABLE_INTERRUPTS();
  for (;;)
  {
  }
}
