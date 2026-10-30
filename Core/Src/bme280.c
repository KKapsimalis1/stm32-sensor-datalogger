#include "bme280.h"
#include "i2c_bus.h"

static I2C_HandleTypeDef *bme_hi2c;

/* Calibration coefficients of this chip, read once in BME280_Init(). */
static Bme280Calib calib;

static HAL_StatusTypeDef bme_write_reg(uint8_t reg, uint8_t val)
{
    return I2cBus_MemWrite(bme_hi2c, BME280_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static HAL_StatusTypeDef bme_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return I2cBus_MemRead(bme_hi2c, BME280_I2C_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

static BME280_Status bme_read_calibration(void)
{
    uint8_t b[26];  /* 0x88..0xA1 */
    uint8_t h[7];   /* 0xE1..0xE7 */

    if (bme_read_regs(BME280_REG_CALIB_00, b, 26) != HAL_OK) return BME280_ERR_I2C;
    if (bme_read_regs(BME280_REG_CALIB_26, h, 7)  != HAL_OK) return BME280_ERR_I2C;

    Bme280_ParseCalib(b, h, &calib);   /* pure math, unit tested on the PC */
    return BME280_OK;
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
    Bme280_ParseRaw(buf, raw_temp, raw_press, raw_hum);
    return BME280_OK;
}

BME280_Status BME280_Read(BME280_Data *out)
{
    int32_t raw_t, raw_p, raw_h;
    BME280_Status st = BME280_ReadRaw(&raw_t, &raw_p, &raw_h);
    if (st != BME280_OK) {
        return st;
    }

    Bme280_Compensate(&calib, raw_t, raw_p, raw_h, out);
    return BME280_OK;
}
