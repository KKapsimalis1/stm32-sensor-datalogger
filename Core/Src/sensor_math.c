#include "sensor_math.h"

/* ---------------- BME280 ---------------- */

void Bme280_ParseCalib(const uint8_t b[26], const uint8_t h[7], Bme280Calib *c)
{
    c->dig_T1 = (uint16_t)((b[1]  << 8) | b[0]);
    c->dig_T2 = (int16_t) ((b[3]  << 8) | b[2]);
    c->dig_T3 = (int16_t) ((b[5]  << 8) | b[4]);

    c->dig_P1 = (uint16_t)((b[7]  << 8) | b[6]);
    c->dig_P2 = (int16_t) ((b[9]  << 8) | b[8]);
    c->dig_P3 = (int16_t) ((b[11] << 8) | b[10]);
    c->dig_P4 = (int16_t) ((b[13] << 8) | b[12]);
    c->dig_P5 = (int16_t) ((b[15] << 8) | b[14]);
    c->dig_P6 = (int16_t) ((b[17] << 8) | b[16]);
    c->dig_P7 = (int16_t) ((b[19] << 8) | b[18]);
    c->dig_P8 = (int16_t) ((b[21] << 8) | b[20]);
    c->dig_P9 = (int16_t) ((b[23] << 8) | b[22]);

    c->dig_H1 = b[25];                                          /* register 0xA1 */
    c->dig_H2 = (int16_t)((h[1] << 8) | h[0]);                  /* 0xE1/0xE2 */
    c->dig_H3 = h[2];                                           /* 0xE3 */
    c->dig_H4 = (int16_t)(((int8_t)h[3] * 16) | (h[4] & 0x0F)); /* 0xE4/0xE5[3:0] */
    c->dig_H5 = (int16_t)(((int8_t)h[5] * 16) | (h[4] >> 4));   /* 0xE6/0xE5[7:4] */
    c->dig_H6 = (int8_t)h[6];                                   /* 0xE7 */
}

void Bme280_ParseRaw(const uint8_t buf[8], int32_t *raw_temp, int32_t *raw_press, int32_t *raw_hum)
{
    *raw_press = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    *raw_temp  = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    *raw_hum   = ((int32_t)buf[6] << 8)  | buf[7];
}

/*
 * Left shifts of values that can be negative are written as multiplications:
 * the result is identical, but it is not undefined behaviour in C.
 */

/* Returns temperature in 0.01 degC and sets t_fine. */
static int32_t compensate_temp(const Bme280Calib *c, int32_t adc_T, int32_t *t_fine)
{
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)c->dig_T1 * 2))) * ((int32_t)c->dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)c->dig_T1)) *
                      ((adc_T >> 4) - ((int32_t)c->dig_T1))) >> 12) * ((int32_t)c->dig_T3)) >> 14;
    *t_fine = var1 + var2;
    return (*t_fine * 5 + 128) >> 8;
}

/* Returns pressure in Pa as unsigned Q24.8 (divide by 256 for Pa). */
static uint32_t compensate_press(const Bme280Calib *c, int32_t adc_P, int32_t t_fine)
{
    int64_t var1, var2, p;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)c->dig_P6;
    var2 = var2 + (var1 * (int64_t)c->dig_P5 * 131072);              /* << 17 */
    var2 = var2 + ((int64_t)c->dig_P4 * 34359738368LL);              /* << 35 */
    var1 = ((var1 * var1 * (int64_t)c->dig_P3) >> 8) +
           (var1 * (int64_t)c->dig_P2 * 4096);                       /* << 12 */
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)c->dig_P1) >> 33;
    if (var1 == 0) {
        return 0;  /* avoid division by zero */
    }
    p = 1048576 - adc_P;
    p = (((p * 2147483648LL) - var2) * 3125) / var1;                 /* p << 31 */
    var1 = (((int64_t)c->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)c->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)c->dig_P7 * 16);        /* << 4 */
    return (uint32_t)p;
}

/* Returns humidity in %RH as unsigned Q22.10 (divide by 1024 for %RH). */
static uint32_t compensate_hum(const Bme280Calib *c, int32_t adc_H, int32_t t_fine)
{
    int32_t v = t_fine - ((int32_t)76800);

    v = (((((adc_H * 16384) - (((int32_t)c->dig_H4) * 1048576) - (((int32_t)c->dig_H5) * v)) +
           ((int32_t)16384)) >> 15) *
         (((((((v * ((int32_t)c->dig_H6)) >> 10) *
              (((v * ((int32_t)c->dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
            ((int32_t)2097152)) * ((int32_t)c->dig_H2) + 8192) >> 14));
    v = (v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)c->dig_H1)) >> 4));
    v = (v < 0 ? 0 : v);
    v = (v > 419430400 ? 419430400 : v);
    return (uint32_t)(v >> 12);
}

void Bme280_Compensate(const Bme280Calib *c, int32_t raw_temp, int32_t raw_press,
                       int32_t raw_hum, Bme280Result *out)
{
    int32_t t_fine;

    out->temp_centi = compensate_temp(c, raw_temp, &t_fine);  /* must run first: sets t_fine */
    out->press_pa   = compensate_press(c, raw_press, t_fine) >> 8;
    out->hum_centi  = (uint32_t)(((uint64_t)compensate_hum(c, raw_hum, t_fine) * 100) >> 10);
}

/* ---------------- MPU6050 ---------------- */

void Mpu6050_ParseRaw(const uint8_t buf[14], Mpu6050Raw *out)
{
    out->accel_x  = (int16_t)((buf[0]  << 8) | buf[1]);
    out->accel_y  = (int16_t)((buf[2]  << 8) | buf[3]);
    out->accel_z  = (int16_t)((buf[4]  << 8) | buf[5]);
    out->temp_raw = (int16_t)((buf[6]  << 8) | buf[7]);
    out->gyro_x   = (int16_t)((buf[8]  << 8) | buf[9]);
    out->gyro_y   = (int16_t)((buf[10] << 8) | buf[11]);
    out->gyro_z   = (int16_t)((buf[12] << 8) | buf[13]);
}

void Mpu6050_Scale(const Mpu6050Raw *raw, const int32_t gyro_offset[3], Mpu6050Scaled *out)
{
    /* accel: mg = raw * 1000 / (LSB per g) */
    out->accel_mg_x = ((int32_t)raw->accel_x * 1000) / MPU6050_ACCEL_LSB_PER_G;
    out->accel_mg_y = ((int32_t)raw->accel_y * 1000) / MPU6050_ACCEL_LSB_PER_G;
    out->accel_mg_z = ((int32_t)raw->accel_z * 1000) / MPU6050_ACCEL_LSB_PER_G;

    /* gyro: 0.01 deg/s = (raw - offset) * 100 / (LSB per deg/s) = (raw - offset) * 1000 / (LSB x10) */
    out->gyro_cdps_x = (((int32_t)raw->gyro_x - gyro_offset[0]) * 1000) / MPU6050_GYRO_LSB_PER_DPS_X10;
    out->gyro_cdps_y = (((int32_t)raw->gyro_y - gyro_offset[1]) * 1000) / MPU6050_GYRO_LSB_PER_DPS_X10;
    out->gyro_cdps_z = (((int32_t)raw->gyro_z - gyro_offset[2]) * 1000) / MPU6050_GYRO_LSB_PER_DPS_X10;

    /* Die temperature: degC = raw / 340 + 36.53 (datasheet). */
    out->temp_centi = ((int32_t)raw->temp_raw * 100) / 340 + 3653;
}
