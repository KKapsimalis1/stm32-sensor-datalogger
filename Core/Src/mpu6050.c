#include "mpu6050.h"

static I2C_HandleTypeDef *mpu_hi2c;

/*
 * Full-scale ranges selected in MPU6050_Init():
 *   ACCEL_CONFIG = 0x00 -> +/-2 g,     16384 LSB/g
 *   GYRO_CONFIG  = 0x00 -> +/-250 deg/s, 131 LSB/(deg/s)
 *
 * To change a range, change the register value in Init() and the matching
 * constant below:
 *   accel: 0x00 -> 16384, 0x08 -> 8192, 0x10 -> 4096, 0x18 -> 2048   (LSB/g)
 *   gyro:  0x00 -> 1310,  0x08 -> 655,  0x10 -> 328,  0x18 -> 164    (LSB/(deg/s) x 10)
 */
#define ACCEL_LSB_PER_G       16384
#define GYRO_LSB_PER_DPS_X10  1310

/* Gyro zero offsets in raw LSB, filled by MPU6050_CalibrateGyro(). */
static int32_t gyro_off_x, gyro_off_y, gyro_off_z;

static HAL_StatusTypeDef mpu_write_reg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(mpu_hi2c, MPU6050_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static HAL_StatusTypeDef mpu_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(mpu_hi2c, MPU6050_I2C_ADDR, reg,
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

    /* Full-scale ranges (see the constants at the top of this file). */
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
    data->accel_x  = (int16_t)((buf[0]  << 8) | buf[1]);
    data->accel_y  = (int16_t)((buf[2]  << 8) | buf[3]);
    data->accel_z  = (int16_t)((buf[4]  << 8) | buf[5]);
    data->temp_raw = (int16_t)((buf[6]  << 8) | buf[7]);
    data->gyro_x   = (int16_t)((buf[8]  << 8) | buf[9]);
    data->gyro_y   = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_z   = (int16_t)((buf[12] << 8) | buf[13]);
    return MPU6050_OK;
}

MPU6050_Status MPU6050_Read(MPU6050_Data *out)
{
    MPU6050_RawData r;
    MPU6050_Status st = MPU6050_ReadRaw(&r);
    if (st != MPU6050_OK) {
        return st;
    }

    /* accel: mg = raw * 1000 / (LSB per g) */
    out->accel_mg_x = ((int32_t)r.accel_x * 1000) / ACCEL_LSB_PER_G;
    out->accel_mg_y = ((int32_t)r.accel_y * 1000) / ACCEL_LSB_PER_G;
    out->accel_mg_z = ((int32_t)r.accel_z * 1000) / ACCEL_LSB_PER_G;

    /* gyro: 0.01 deg/s = (raw - offset) * 100 / (LSB per deg/s) = (raw - offset) * 1000 / (LSB x10) */
    out->gyro_cdps_x = (((int32_t)r.gyro_x - gyro_off_x) * 1000) / GYRO_LSB_PER_DPS_X10;
    out->gyro_cdps_y = (((int32_t)r.gyro_y - gyro_off_y) * 1000) / GYRO_LSB_PER_DPS_X10;
    out->gyro_cdps_z = (((int32_t)r.gyro_z - gyro_off_z) * 1000) / GYRO_LSB_PER_DPS_X10;

    /* Die temperature: degC = raw / 340 + 36.53 (datasheet). */
    out->temp_centi = ((int32_t)r.temp_raw * 100) / 340 + 3653;

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
        HAL_Delay(10);  /* new sample every 10 ms at 100 Hz */
    }

    gyro_off_x = sx / samples;
    gyro_off_y = sy / samples;
    gyro_off_z = sz / samples;
    return MPU6050_OK;
}
