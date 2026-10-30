#include "mpu6050.h"
#include "i2c_bus.h"
#include "FreeRTOS.h"
#include "task.h"

static I2C_HandleTypeDef *mpu_hi2c;

/* Full-scale ranges and the scaling constants live in sensor_math.h. */

/* Gyro zero offsets in raw LSB, filled by MPU6050_CalibrateGyro(). */
static int32_t gyro_off[3];

static HAL_StatusTypeDef mpu_write_reg(uint8_t reg, uint8_t val)
{
    return I2cBus_MemWrite(mpu_hi2c, MPU6050_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static HAL_StatusTypeDef mpu_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return I2cBus_MemRead(mpu_hi2c, MPU6050_I2C_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

MPU6050_Status MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t who = 0;
    mpu_hi2c = hi2c;

    if (mpu_read_regs(MPU6050_REG_WHO_AM_I, &who, 1) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }
    if (who != MPU6050_WHO_AM_I_VAL) {
        return MPU6050_ERR_NOT_FOUND;
    }

    /* Wake up from sleep and use the X-gyro PLL as clock (more stable than the internal 8 MHz). */
    if (mpu_write_reg(MPU6050_REG_PWR_MGMT1, 0x01) != HAL_OK) return MPU6050_ERR_I2C;
    HAL_Delay(100);  /* let the clock settle */

    /* Digital low-pass filter ~44 Hz, sample rate = 1 kHz / (1 + 9) = 100 Hz. */
    if (mpu_write_reg(MPU6050_REG_CONFIG, 0x03)       != HAL_OK) return MPU6050_ERR_I2C;
    if (mpu_write_reg(MPU6050_REG_SMPLRT_DIV, 9)      != HAL_OK) return MPU6050_ERR_I2C;

    /* Full-scale ranges (see the constants in sensor_math.h). */
    if (mpu_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x00) != HAL_OK) return MPU6050_ERR_I2C;
    if (mpu_write_reg(MPU6050_REG_GYRO_CONFIG, 0x00)  != HAL_OK) return MPU6050_ERR_I2C;

    return MPU6050_OK;
}

MPU6050_Status MPU6050_ReadRaw(MPU6050_RawData *data)
{
    uint8_t buf[14];

    if (mpu_read_regs(MPU6050_REG_ACCEL_XOUT_H, buf, 14) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }
    Mpu6050_ParseRaw(buf, data);
    return MPU6050_OK;
}

MPU6050_Status MPU6050_Read(MPU6050_Data *out)
{
    MPU6050_RawData r;
    MPU6050_Status st = MPU6050_ReadRaw(&r);
    if (st != MPU6050_OK) {
        return st;
    }

    Mpu6050_Scale(&r, gyro_off, out);   /* pure math, unit tested on the PC */
    return MPU6050_OK;
}

MPU6050_Status MPU6050_CalibrateGyro(uint16_t samples)
{
    MPU6050_RawData r;
    int32_t sx = 0, sy = 0, sz = 0;

    if (samples == 0) {
        return MPU6050_OK;
    }

    for (uint16_t i = 0; i < samples; i++) {
        MPU6050_Status st = MPU6050_ReadRaw(&r);
        if (st != MPU6050_OK) {
            return st;
        }
        sx += r.gyro_x;
        sy += r.gyro_y;
        sz += r.gyro_z;
        vTaskDelay(pdMS_TO_TICKS(10));  /* new sample every 10 ms at 100 Hz; lets other tasks run */
    }

    gyro_off[0] = sx / samples;
    gyro_off[1] = sy / samples;
    gyro_off[2] = sz / samples;
    return MPU6050_OK;
}
