#include "bme280.h"

static I2C_HandleTypeDef *bme_hi2c;

/* Calibration coefficients: unique per chip, stored in the sensor's NVM. */
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3;
static int16_t  dig_H2, dig_H4, dig_H5;
static int8_t   dig_H6;

/* Shared between temperature and the other compensations. */
static int32_t t_fine;

static HAL_StatusTypeDef bme_write_reg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(bme_hi2c, BME280_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static HAL_StatusTypeDef bme_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(bme_hi2c, BME280_I2C_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

static BME280_Status bme_read_calibration(void)
{
    uint8_t b[26];  /* 0x88..0xA1 */
    uint8_t h[7];   /* 0xE1..0xE7 */

    if (bme_read_regs(BME280_REG_CALIB_00, b, 26) != HAL_OK) return BME280_ERR_I2C;
    if (bme_read_regs(BME280_REG_CALIB_26, h, 7)  != HAL_OK) return BME280_ERR_I2C;

    dig_T1 = (uint16_t)((b[1]  << 8) | b[0]);
    dig_T2 = (int16_t) ((b[3]  << 8) | b[2]);
    dig_T3 = (int16_t) ((b[5]  << 8) | b[4]);

    dig_P1 = (uint16_t)((b[7]  << 8) | b[6]);
    dig_P2 = (int16_t) ((b[9]  << 8) | b[8]);
    dig_P3 = (int16_t) ((b[11] << 8) | b[10]);
    dig_P4 = (int16_t) ((b[13] << 8) | b[12]);
    dig_P5 = (int16_t) ((b[15] << 8) | b[14]);
    dig_P6 = (int16_t) ((b[17] << 8) | b[16]);
    dig_P7 = (int16_t) ((b[19] << 8) | b[18]);
    dig_P8 = (int16_t) ((b[21] << 8) | b[20]);
    dig_P9 = (int16_t) ((b[23] << 8) | b[22]);

    dig_H1 = b[25];                                   /* register 0xA1 */
    dig_H2 = (int16_t)((h[1] << 8) | h[0]);           /* 0xE1/0xE2 */
    dig_H3 = h[2];                                    /* 0xE3 */
    dig_H4 = (int16_t)(((int8_t)h[3] * 16) | (h[4] & 0x0F));  /* 0xE4/0xE5[3:0] */
    dig_H5 = (int16_t)(((int8_t)h[5] * 16) | (h[4] >> 4));    /* 0xE6/0xE5[7:4] */
    dig_H6 = (int8_t)h[6];                            /* 0xE7 */

    return BME280_OK;
}

/* Integer compensation formulas from the Bosch BME280 datasheet. */

/* Returns temperature in 0.01 degC. Also computes t_fine. */
static int32_t compensate_temp(int32_t adc_T)
{
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
                      ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

/* Returns pressure in Pa as unsigned Q24.8 (divide by 256 for Pa). */
static uint32_t compensate_press(int32_t adc_P)
{
    int64_t var1, var2, p;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if (var1 == 0) {
        return 0;  /* avoid division by zero */
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (uint32_t)p;
}

/* Returns humidity in %RH as unsigned Q22.10 (divide by 1024 for %RH). */
static uint32_t compensate_hum(int32_t adc_H)
{
    int32_t v = t_fine - ((int32_t)76800);

    v = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v)) +
           ((int32_t)16384)) >> 15) *
         (((((((v * ((int32_t)dig_H6)) >> 10) *
              (((v * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
            ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
    v = (v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
    v = (v < 0 ? 0 : v);
    v = (v > 419430400 ? 419430400 : v);
    return (uint32_t)(v >> 12);
}

BME280_Status BME280_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t chip_id = 0;
    bme_hi2c = hi2c;

    if (bme_read_regs(BME280_REG_CHIP_ID, &chip_id, 1) != HAL_OK) {
        return BME280_ERR_I2C;
    }
    if (chip_id != BME280_CHIP_ID_VAL) {
        return BME280_ERR_NOT_FOUND;  /* wrong address, or a BMP280 (0x58) */
    }

    BME280_Status st = bme_read_calibration();
    if (st != BME280_OK) {
        return st;
    }

    /* ctrl_hum must be written before ctrl_meas to take effect. */
    bme_write_reg(BME280_REG_CTRL_HUM, 0x01);   /* humidity oversampling x1 */
    bme_write_reg(BME280_REG_CTRL_MEAS, 0x27);  /* temp x1, press x1, normal mode */
    HAL_Delay(10);                              /* let the first measurement finish */

    return BME280_OK;
}

BME280_Status BME280_ReadRaw(int32_t *raw_temp, int32_t *raw_press, int32_t *raw_hum)
{
    uint8_t buf[8];

    if (bme_read_regs(BME280_REG_DATA, buf, 8) != HAL_OK) {
        return BME280_ERR_I2C;
    }
    *raw_press = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    *raw_temp  = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    *raw_hum   = ((int32_t)buf[6] << 8)  | buf[7];
    return BME280_OK;
}

BME280_Status BME280_Read(BME280_Data *out)
{
    int32_t raw_t, raw_p, raw_h;
    BME280_Status st = BME280_ReadRaw(&raw_t, &raw_p, &raw_h);
    if (st != BME280_OK) {
        return st;
    }

    out->temp_centi = compensate_temp(raw_t);  /* must run first: sets t_fine */
    out->press_pa   = compensate_press(raw_p) >> 8;
    out->hum_centi  = (uint32_t)(((uint64_t)compensate_hum(raw_h) * 100) >> 10);
    return BME280_OK;
}
