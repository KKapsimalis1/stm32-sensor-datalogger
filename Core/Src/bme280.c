#include "bme280.h"

static I2C_HandleTypeDef *bme_hi2c;

static HAL_StatusTypeDef bme_write_reg(uint8_t reg, uint8_t val) {
    return HAL_I2C_Mem_Write(bme_hi2c, BME280_I2C_ADDR, reg,
                              I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

static HAL_StatusTypeDef bme_read_regs(uint8_t reg, uint8_t *buf, uint16_t len) {
    return HAL_I2C_Mem_Read(bme_hi2c, BME280_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

BME280_Status BME280_Init(I2C_HandleTypeDef *hi2c) {
    bme_hi2c = hi2c;
    uint8_t chip_id = 0;

    if (bme_read_regs(BME280_REG_CHIP_ID, &chip_id, 1) != HAL_OK) {
        return BME280_ERR_I2C;
    }
    if (chip_id != BME280_CHIP_ID_VAL) {
        return BME280_ERR_NOT_FOUND;  // πιθανώς λάθος address, ή είναι BMP280 (chip_id=0x58)
    }

    // TODO επόμενο βήμα: διάβασμα calibration registers (0x88-0xA1, 0xE1-0xE7)
    // TODO: ctrl_hum, ctrl_meas, config -> normal mode, oversampling x1

    bme_write_reg(BME280_REG_CTRL_HUM, 0x01);   // humidity oversampling x1
    bme_write_reg(BME280_REG_CTRL_MEAS, 0x27);  // temp/press oversampling x1, normal mode

    return BME280_OK;
}

BME280_Status BME280_ReadRaw(int32_t *raw_temp, int32_t *raw_press, int32_t *raw_hum) {
    uint8_t buf[8];
    if (bme_read_regs(BME280_REG_DATA, buf, 8) != HAL_OK) {
        return BME280_ERR_I2C;
    }
    *raw_press = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    *raw_temp  = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    *raw_hum   = ((int32_t)buf[6] << 8)  | buf[7];
    return BME280_OK;
}

