#ifndef INC_SENSOR_TASK_H_
#define INC_SENSOR_TASK_H_

#include "main.h"
#include "bme280.h"
#include "mpu6050.h"
#include "FreeRTOS.h"
#include "queue.h"

#define SENSOR_DEFAULT_PERIOD_MS  1000
#define SENSOR_MIN_PERIOD_MS      20

typedef struct {
    uint32_t     timestamp_ms;
    BME280_Data  env;
    MPU6050_Data imu;
    uint8_t      env_ok;
    uint8_t      imu_ok;
} SensorSample;

void          SensorTask_Create(void);
QueueHandle_t SensorTask_GetQueue(void);
uint8_t       SensorTask_GetLatest(SensorSample *out);
void          SensorTask_SetPeriod(uint32_t period_ms);
uint32_t      SensorTask_GetPeriod(void);
uint32_t      SensorTask_DroppedCount(void);

#endif /* INC_SENSOR_TASK_H_ */
