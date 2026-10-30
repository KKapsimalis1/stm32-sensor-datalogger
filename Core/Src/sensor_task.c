#include "sensor_task.h"
#include "app_config.h"
#include "task.h"
#include <string.h>

#define SENSOR_QUEUE_LEN  16

static TaskHandle_t   task_handle;
static QueueHandle_t  sample_queue;
static SensorSample   latest;
static volatile uint8_t  have_latest;
static volatile uint32_t period_ms = SENSOR_DEFAULT_PERIOD_MS;
static volatile uint32_t dropped_count;

static void sensor_task(void *argument)
{
    (void)argument;
    TickType_t next_wake = xTaskGetTickCount();

    for (;;) {
        SensorSample s;
        memset(&s, 0, sizeof(s));
        s.timestamp_ms = HAL_GetTick();
        s.env_ok = (BME280_Read(&s.env) == BME280_OK);
        s.imu_ok = (MPU6050_Read(&s.imu) == MPU6050_OK);

        taskENTER_CRITICAL();
        latest = s;
        have_latest = 1;
        taskEXIT_CRITICAL();

        if (xQueueSend(sample_queue, &s, 0) != pdTRUE) {
            dropped_count++;
        }

        /*
         * Wait until the next period. The deadline advances by exactly one period, so the
         * time spent reading does not accumulate as drift. A notification (sent when the
         * period changes) ends the wait early and restarts the schedule.
         */
        TickType_t period_ticks = pdMS_TO_TICKS(period_ms);
        next_wake += period_ticks;
        TickType_t now  = xTaskGetTickCount();
        TickType_t wait = next_wake - now;
        if (wait > period_ticks) {      /* the deadline has already passed: resynchronise */
            next_wake = now;
            wait = 0;
        }
        if (ulTaskNotifyTake(pdTRUE, wait) != 0) {
            next_wake = xTaskGetTickCount();
        }
    }
}

void SensorTask_Create(void)
{
    sample_queue = xQueueCreate(SENSOR_QUEUE_LEN, sizeof(SensorSample));
    xTaskCreate(sensor_task, "Sensors", STACK_SENSORS, NULL, PRIO_SENSORS, &task_handle);
}

QueueHandle_t SensorTask_GetQueue(void)
{
    return sample_queue;
}

uint8_t SensorTask_GetLatest(SensorSample *out)
{
    uint8_t ok;

    taskENTER_CRITICAL();
    ok = have_latest;
    if (ok) {
        *out = latest;
    }
    taskEXIT_CRITICAL();
    return ok;
}

void SensorTask_SetPeriod(uint32_t new_period_ms)
{
    if (new_period_ms < SENSOR_MIN_PERIOD_MS) {
        new_period_ms = SENSOR_MIN_PERIOD_MS;
    }
    period_ms = new_period_ms;
    if (task_handle != NULL) {
        xTaskNotifyGive(task_handle);
    }
}

uint32_t SensorTask_GetPeriod(void)
{
    return period_ms;
}

uint32_t SensorTask_DroppedCount(void)
{
    return dropped_count;
}
