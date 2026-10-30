#ifndef INC_SENSOR_MATH_H_
#define INC_SENSOR_MATH_H_

/*
 * Pure, hardware-independent sensor math: no HAL, no FreeRTOS, only <stdint.h>.
 * It is compiled both into the firmware and into the unit tests that run on the PC.
 */

#include <stdint.h>

/* ---------------- BME280 ---------------- */

/* Calibration coefficients: unique per chip, stored in the sensor's NVM. */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4, dig_H5;
    int8_t   dig_H6;
} Bme280Calib;

/* Compensated measurements in fixed point (no floating point needed). */
typedef struct {
    int32_t  temp_centi;   /* 0.01 degC  (2508  = 25.08 degC) */
    uint32_t press_pa;     /* Pa         (101325 = 1013.25 hPa) */
    uint32_t hum_centi;    /* 0.01 %RH   (4512  = 45.12 %RH)  */
} Bme280Result;

/* b: registers 0x88..0xA1 (26 bytes), h: registers 0xE1..0xE7 (7 bytes). */
void Bme280_ParseCalib(const uint8_t b[26], const uint8_t h[7], Bme280Calib *c);

/* buf: registers 0xF7..0xFE (press[3], temp[3], hum[2]). */
void Bme280_ParseRaw(const uint8_t buf[8], int32_t *raw_temp, int32_t *raw_press, int32_t *raw_hum);

/* Integer compensation formulas from the Bosch BME280 datasheet. */
void Bme280_Compensate(const Bme280Calib *c, int32_t raw_temp, int32_t raw_press,
                       int32_t raw_hum, Bme280Result *out);

/* ---------------- MPU6050 ---------------- */

/*
 * Full-scale ranges selected in MPU6050_Init():
 *   ACCEL_CONFIG = 0x00 -> +/-2 g,       16384 LSB/g
 *   GYRO_CONFIG  = 0x00 -> +/-250 deg/s, 131 LSB/(deg/s)
 *
 * To change a range, change the register value in MPU6050_Init() and the matching
 * constant below:
 *   accel: 0x00 -> 16384, 0x08 -> 8192, 0x10 -> 4096, 0x18 -> 2048   (LSB/g)
 *   gyro:  0x00 -> 1310,  0x08 -> 655,  0x10 -> 328,  0x18 -> 164    (LSB/(deg/s) x 10)
 */
#define MPU6050_ACCEL_LSB_PER_G       16384
#define MPU6050_GYRO_LSB_PER_DPS_X10  1310

/* Raw 16-bit register values, exactly as read from the sensor. */
typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t temp_raw;
    int16_t gyro_x, gyro_y, gyro_z;
} Mpu6050Raw;

/* Scaled measurements in fixed point. */
typedef struct {
    int32_t accel_mg_x, accel_mg_y, accel_mg_z;     /* milli-g        (1000 = 1 g)      */
    int32_t gyro_cdps_x, gyro_cdps_y, gyro_cdps_z;  /* 0.01 deg/s     (100 = 1 deg/s)   */
    int32_t temp_centi;                             /* 0.01 degC, die temperature       */
} Mpu6050Scaled;

/* buf: registers 0x3B..0x48 (accel[6], temp[2], gyro[6]), big endian. */
void Mpu6050_ParseRaw(const uint8_t buf[14], Mpu6050Raw *out);

/* gyro_offset: zero offsets in raw LSB for x, y, z. */
void Mpu6050_Scale(const Mpu6050Raw *raw, const int32_t gyro_offset[3], Mpu6050Scaled *out);

#endif /* INC_SENSOR_MATH_H_ */
