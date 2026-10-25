#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include "main.h"

#define MPU6050_I2C_ADDR          (0x68 << 1)  /* AD0 low -> 0x68, AD0 high -> 0x69 */
#define MPU6050_REG_SMPLRT_DIV    0x19
#define MPU6050_REG_CONFIG        0x1A
#define MPU6050_REG_GYRO_CONFIG   0x1B
#define MPU6050_REG_ACCEL_CONFIG  0x1C
#define MPU6050_REG_ACCEL_XOUT_H  0x3B         /* 14 bytes: accel(6) temp(2) gyro(6) */
#define MPU6050_REG_PWR_MGMT1     0x6B
#define MPU6050_REG_WHO_AM_I      0x75
#define MPU6050_WHO_AM_I_VAL      0x68

typedef enum {
    MPU6050_OK = 0,
    MPU6050_ERR_NOT_FOUND,
    MPU6050_ERR_I2C
} MPU6050_Status;

/* Raw 16-bit register values, exactly as read from the sensor. */
typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t temp_raw;
    int16_t gyro_x, gyro_y, gyro_z;
} MPU6050_RawData;

/* Scaled measurements in fixed point (no floating point needed). */
typedef struct {
    int32_t accel_mg_x, accel_mg_y, accel_mg_z;     /* milli-g        (1000 = 1 g)      */
    int32_t gyro_cdps_x, gyro_cdps_y, gyro_cdps_z;  /* 0.01 deg/s     (100 = 1 deg/s)   */
    int32_t temp_centi;                             /* 0.01 degC, die temperature       */
} MPU6050_Data;

MPU6050_Status MPU6050_Init(I2C_HandleTypeDef *hi2c);
MPU6050_Status MPU6050_ReadRaw(MPU6050_RawData *data);
MPU6050_Status MPU6050_Read(MPU6050_Data *out);

/* Measure the gyro zero offset while the board is perfectly still. */
MPU6050_Status MPU6050_CalibrateGyro(uint16_t samples);

#endif /* INC_MPU6050_H_ */
