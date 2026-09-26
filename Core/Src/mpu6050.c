#include "mpu6050.h"

static I2C_HandleTypeDef *mpu_hi2c;

static HAL_StatusTypeDef mpu_write_reg(uint8_t reg, uint8_t val) {
    return HAL_I2C_Mem_Write(mpu_hi2c, MPU6050_I2C_ADDR, reg,
                              I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static HAL_StatusTypeDef mpu_read_regs(uint8_t reg, uint8_t *buf, uint16_t len) {
    return HAL_I2C_Mem_Read(mpu_hi2c, MPU6050_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

MPU6050_Status MPU6050_Init(I2C_HandleTypeDef *hi2c) {
    mpu_hi2c = hi2c;
    uint8_t who = 0;

    if (mpu_read_regs(MPU6050_REG_WHO_AM_I, &who, 1) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }
    if (who != MPU6050_WHO_AM_I_VAL) {
        return MPU6050_ERR_NOT_FOUND;
    }

    // Το MPU6050 ξεκινάει σε sleep mode by default -> "ξύπνημα"
    if (mpu_write_reg(MPU6050_REG_PWR_MGMT1, 0x00) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }

    // TODO επόμενο βήμα: ρύθμιση full-scale range (accel config 0x1C, gyro config 0x1B)
    // TODO: compensation/scaling σε g και °/s (datasheet §6.1-6.2)

    return MPU6050_OK;
}

MPU6050_Status MPU6050_ReadRaw(MPU6050_RawData *data) {
    uint8_t buf[14];
    if (mpu_read_regs(MPU6050_REG_ACCEL_XOUT_H, buf, 14) != HAL_OK) {
        return MPU6050_ERR_I2C;
    }
    data->accel_x = (int16_t)((buf[0] << 8)  | buf[1]);
    data->accel_y = (int16_t)((buf[2] << 8)  | buf[3]);
    data->accel_z = (int16_t)((buf[4] << 8)  | buf[5]);
    data->temp_raw= (int16_t)((buf[6] << 8)  | buf[7]);
    data->gyro_x  = (int16_t)((buf[8] << 8)  | buf[9]);
    data->gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);
    return MPU6050_OK;
}
