#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include "main.h"

#define MPU6050_I2C_ADDR      (0x68 << 1)  // AD0 pin στο GND -> 0x68. Αν AD0=VCC -> 0x69
#define MPU6050_REG_WHO_AM_I  0x75
#define MPU6050_WHO_AM_I_VAL  0x68
#define MPU6050_REG_PWR_MGMT1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B  // αρχή 14 bytes: accel(6)+temp(2)+gyro(6)

typedef enum {
    MPU6050_OK = 0,
    MPU6050_ERR_NOT_FOUND,
    MPU6050_ERR_I2C
} MPU6050_Status;

typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t temp_raw;
    int16_t gyro_x, gyro_y, gyro_z;
} MPU6050_RawData;

MPU6050_Status MPU6050_Init(I2C_HandleTypeDef *hi2c);
MPU6050_Status MPU6050_ReadRaw(MPU6050_RawData *data);

#endif
