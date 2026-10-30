#include "data_logger.h"
#include "app_config.h"
#include "sensor_task.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stdio.h>
#include <stdlib.h>

static LogRecord buffer[DATALOGGER_CAPACITY];
static uint32_t  head;          /* index where the next record is written */
static uint32_t  count;         /* number of valid records (max DATALOGGER_CAPACITY) */

static volatile uint32_t period_ms = DATALOGGER_DEFAULT_MS;
static volatile uint32_t error_count;
static volatile uint32_t dropped_count;
static volatile uint8_t  running;
static volatile uint8_t  dumping;

/* Protects head, count and the buffer against the logger task, the CLI and the button. */
static SemaphoreHandle_t buffer_mutex;
static QueueHandle_t     sample_queue;

/* Prints a value given in 0.01 units, e.g. -1234 -> "-12.34". */
static void print_centi(int32_t v)
{
    printf("%s%ld.%02ld", v < 0 ? "-" : "", labs(v) / 100, labs(v) % 100);
}

static void clear_buffer(void)
{
    head = 0;
    count = 0;
    error_count = 0;
    dropped_count = 0;
}

static void store(const SensorSample *s)
{
    LogRecord *r = &buffer[head];

    r->timestamp_ms = s->timestamp_ms;
    r->temp_centi   = s->env.temp_centi;
    r->press_pa     = s->env.press_pa;
    r->hum_centi    = s->env.hum_centi;
    r->accel_mg[0]  = (int16_t)s->imu.accel_mg_x;
    r->accel_mg[1]  = (int16_t)s->imu.accel_mg_y;
    r->accel_mg[2]  = (int16_t)s->imu.accel_mg_z;
    r->gyro_cdps[0] = (int16_t)s->imu.gyro_cdps_x;
    r->gyro_cdps[1] = (int16_t)s->imu.gyro_cdps_y;
    r->gyro_cdps[2] = (int16_t)s->imu.gyro_cdps_z;

    /* Circular buffer: when full, the oldest record is overwritten. */
    head = (head + 1) % DATALOGGER_CAPACITY;
    if (count < DATALOGGER_CAPACITY) {
        count++;
    }
}

static void logger_task(void *argument)
{
    (void)argument;
    SensorSample s;

    for (;;) {
        if (xQueueReceive(sample_queue, &s, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!running) {
            continue;   /* not recording: discard the sample */
        }
        if (!s.env_ok || !s.imu_ok) {
            error_count++;
            continue;
        }

        xSemaphoreTake(buffer_mutex, portMAX_DELAY);
        if (dumping) {
            dropped_count++;
        } else {
            store(&s);
        }
        xSemaphoreGive(buffer_mutex);
    }
}

void DataLogger_Init(void)
{
    /* No FreeRTOS calls here: this runs before the scheduler starts. */
    running = 0;
    dumping = 0;
    clear_buffer();
}

void DataLogger_Create(void)
{
    buffer_mutex = xSemaphoreCreateMutex();
    sample_queue = SensorTask_GetQueue();
    xTaskCreate(logger_task, "Logger", STACK_LOGGER, NULL, PRIO_LOGGER, NULL);
}

void DataLogger_Start(uint32_t period)
{
    if (period < DATALOGGER_MIN_MS) {
        period = DATALOGGER_MIN_MS;
    }
    period_ms = period;
    if (sample_queue != NULL) {
        xQueueReset(sample_queue);   /* discard samples taken before the start */
    }
    running = 1;
    SensorTask_SetPeriod(period);
}

void DataLogger_Stop(void)
{
    running = 0;
    SensorTask_SetPeriod(SENSOR_DEFAULT_PERIOD_MS);   /* back to the slow idle rate */
}

void DataLogger_Clear(void)
{
    if (buffer_mutex != NULL) {
        xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    }
    clear_buffer();
    if (buffer_mutex != NULL) {
        xSemaphoreGive(buffer_mutex);
    }
}

void DataLogger_Dump(void)
{
    /* Freeze the buffer: take a consistent snapshot of the indices, then stop new writes. */
    if (buffer_mutex != NULL) {
        xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    }
    dumping = 1;
    uint32_t n = count;
    uint32_t start = (n < DATALOGGER_CAPACITY) ? 0 : head;   /* when wrapped, the oldest is at 'head' */
    if (buffer_mutex != NULL) {
        xSemaphoreGive(buffer_mutex);
    }

    printf("\r\nt_ms,temp_c,press_hpa,hum_pct,ax_mg,ay_mg,az_mg,gx_dps,gy_dps,gz_dps\r\n");

    for (uint32_t i = 0; i < n; i++) {
        const LogRecord *r = &buffer[(start + i) % DATALOGGER_CAPACITY];

        printf("%lu,", (unsigned long)r->timestamp_ms);
        print_centi(r->temp_centi);
        printf(",%lu.%02lu,%lu.%02lu,%ld,%ld,%ld,",
               (unsigned long)(r->press_pa / 100), (unsigned long)(r->press_pa % 100),
               (unsigned long)(r->hum_centi / 100), (unsigned long)(r->hum_centi % 100),
               (long)r->accel_mg[0], (long)r->accel_mg[1], (long)r->accel_mg[2]);
        print_centi(r->gyro_cdps[0]);
        printf(",");
        print_centi(r->gyro_cdps[1]);
        printf(",");
        print_centi(r->gyro_cdps[2]);
        printf("\r\n");
    }
    printf("-- %lu records --", (unsigned long)n);

    dumping = 0;
}

uint8_t DataLogger_IsRunning(void)
{
    return running;
}

uint32_t DataLogger_Count(void)
{
    return count;
}

uint32_t DataLogger_Period(void)
{
    return period_ms;
}

uint32_t DataLogger_ErrorCount(void)
{
    return error_count;
}

uint32_t DataLogger_DroppedCount(void)
{
    return dropped_count;
}
