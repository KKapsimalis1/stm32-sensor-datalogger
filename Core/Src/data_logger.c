#include "data_logger.h"
#include "bme280.h"
#include "mpu6050.h"
#include <stdio.h>
#include <stdlib.h>

static LogRecord buffer[DATALOGGER_CAPACITY];
static uint32_t  head;          /* index where the next record is written */
static uint32_t  count;         /* number of valid records (max DATALOGGER_CAPACITY) */
static uint32_t  period_ms = DATALOGGER_DEFAULT_MS;
static uint32_t  last_sample_ms;
static uint32_t  error_count;
static uint8_t   running;

/* Prints a value given in 0.01 units, e.g. -1234 -> "-12.34". */
static void print_centi(int32_t v)
{
    printf("%s%ld.%02ld", v < 0 ? "-" : "", labs(v) / 100, labs(v) % 100);
}

void DataLogger_Init(void)
{
    DataLogger_Stop();
    DataLogger_Clear();
}

void DataLogger_Start(uint32_t period)
{
    if (period < DATALOGGER_MIN_MS) {
        period = DATALOGGER_MIN_MS;
    }
    period_ms = period;
    last_sample_ms = HAL_GetTick() - period_ms;  /* take the first sample immediately */
    running = 1;
}

void DataLogger_Stop(void)
{
    running = 0;
}

void DataLogger_Clear(void)
{
    head = 0;
    count = 0;
    error_count = 0;
}

void DataLogger_Process(void)
{
    if (!running) {
        return;
    }

    uint32_t now = HAL_GetTick();
    if ((now - last_sample_ms) < period_ms) {
        return;
    }
    last_sample_ms = now;

    BME280_Data  env;
    MPU6050_Data imu;
    if (BME280_Read(&env) != BME280_OK || MPU6050_Read(&imu) != MPU6050_OK) {
        error_count++;
        return;
    }

    LogRecord *r = &buffer[head];
    r->timestamp_ms = now;
    r->temp_centi   = env.temp_centi;
    r->press_pa     = env.press_pa;
    r->hum_centi    = env.hum_centi;
    r->accel_mg[0]  = (int16_t)imu.accel_mg_x;
    r->accel_mg[1]  = (int16_t)imu.accel_mg_y;
    r->accel_mg[2]  = (int16_t)imu.accel_mg_z;
    r->gyro_cdps[0] = (int16_t)imu.gyro_cdps_x;
    r->gyro_cdps[1] = (int16_t)imu.gyro_cdps_y;
    r->gyro_cdps[2] = (int16_t)imu.gyro_cdps_z;

    /* Circular buffer: when full, the oldest record is overwritten. */
    head = (head + 1) % DATALOGGER_CAPACITY;
    if (count < DATALOGGER_CAPACITY) {
        count++;
    }
}

void DataLogger_Dump(void)
{
    printf("\r\nt_ms,temp_c,press_hpa,hum_pct,ax_mg,ay_mg,az_mg,gx_dps,gy_dps,gz_dps\r\n");

    /* When the buffer has wrapped, the oldest record is at 'head'. */
    uint32_t start = (count < DATALOGGER_CAPACITY) ? 0 : head;

    for (uint32_t i = 0; i < count; i++) {
        const LogRecord *r = &buffer[(start + i) % DATALOGGER_CAPACITY];

        printf("%lu,", r->timestamp_ms);
        print_centi(r->temp_centi);
        printf(",%lu.%02lu,%lu.%02lu,%d,%d,%d,",
               r->press_pa / 100, r->press_pa % 100,
               r->hum_centi / 100, r->hum_centi % 100,
               r->accel_mg[0], r->accel_mg[1], r->accel_mg[2]);
        print_centi(r->gyro_cdps[0]);
        printf(",");
        print_centi(r->gyro_cdps[1]);
        printf(",");
        print_centi(r->gyro_cdps[2]);
        printf("\r\n");
    }
    printf("-- %lu records --", count);
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
