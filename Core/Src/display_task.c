#include "display_task.h"
#include "bme280.h"
#include "mpu6050.h"
#include <stdlib.h>

#define DISPLAY_PERIOD_MS  1000

static uint8_t  display_ready;
static uint32_t last_update_ms;

/* Writes a value given in 0.01 units with one decimal, e.g. -123 -> "-1.2". */
static void write_deci(int32_t value_centi)
{
    int32_t t = value_centi / 10;  /* tenths, truncated toward zero */
    SSD1306_Printf("%s%ld.%ld", t < 0 ? "-" : "", labs(t) / 10, labs(t) % 10);
}

SSD1306_Status DisplayTask_Init(I2C_HandleTypeDef *hi2c)
{
    SSD1306_Status st = SSD1306_Init(hi2c);
    display_ready = (st == SSD1306_OK);
    last_update_ms = HAL_GetTick() - DISPLAY_PERIOD_MS;  /* draw the first frame immediately */
    return st;
}

void DisplayTask_Process(void)
{
    if (!display_ready) {
        return;
    }

    uint32_t now = HAL_GetTick();
    if ((now - last_update_ms) < DISPLAY_PERIOD_MS) {
        return;
    }
    last_update_ms = now;

    BME280_Data  env;
    MPU6050_Data imu;
    uint8_t env_ok = (BME280_Read(&env) == BME280_OK);
    uint8_t imu_ok = (MPU6050_Read(&imu) == MPU6050_OK);

    SSD1306_Clear();

    /* Environment: 3 text rows */
    if (env_ok) {
        int32_t t = env.temp_centi;
        SSD1306_SetCursor(0, 0);
        SSD1306_Printf("T: %s%ld.%02ld C", t < 0 ? "-" : "", labs(t) / 100, labs(t) % 100);
        SSD1306_SetCursor(0, 8);
        SSD1306_Printf("P: %lu.%02lu hPa", env.press_pa / 100, env.press_pa % 100);
        SSD1306_SetCursor(0, 16);
        SSD1306_Printf("H: %lu.%02lu %%RH", env.hum_centi / 100, env.hum_centi % 100);
    } else {
        SSD1306_SetCursor(0, 0);
        SSD1306_WriteString("BME280: read error");
    }

    /* Separator line */
    for (uint8_t x = 0; x < SSD1306_WIDTH; x++) {
        SSD1306_DrawPixel(x, 27, 1);
    }

    /* Motion: accelerometer and gyroscope */
    if (imu_ok) {
        SSD1306_SetCursor(0, 32);
        SSD1306_WriteString("Accel [mg]");
        SSD1306_SetCursor(0, 40);
        SSD1306_Printf("%ld %ld %ld", imu.accel_mg_x, imu.accel_mg_y, imu.accel_mg_z);

        SSD1306_SetCursor(0, 48);
        SSD1306_WriteString("Gyro [dps]");
        SSD1306_SetCursor(0, 56);
        write_deci(imu.gyro_cdps_x);
        SSD1306_WriteChar(' ');
        write_deci(imu.gyro_cdps_y);
        SSD1306_WriteChar(' ');
        write_deci(imu.gyro_cdps_z);
    } else {
        SSD1306_SetCursor(0, 32);
        SSD1306_WriteString("MPU6050: read error");
    }

    SSD1306_Update();
}
