#ifndef INC_BME280_H_
#define INC_BME280_H_

#include "main.h"

#define BME280_I2C_ADDR      (0x76 << 1)  // ή 0x77 αν το SDO είναι στο VDD - τσέκαρε το datasheet/βuy page
#define BME280_REG_CHIP_ID   0xD0
#define BME280_CHIP_ID_VAL   0x60
#define BME280_REG_RESET     0xE0
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CTRL_HUM  0xF2
#define BME280_REG_CONFIG    0xF5
#define BME280_REG_DATA      0xF7   // αρχή των 8 bytes: press(3)+temp(3)+hum(2)

typedef enum {
    BME280_OK = 0,
    BME280_ERR_NOT_FOUND,
    BME280_ERR_I2C
} BME280_Status;

BME280_Status BME280_Init(I2C_HandleTypeDef *hi2c);
BME280_Status BME280_ReadRaw(int32_t *raw_temp, int32_t *raw_press, int32_t *raw_hum);

#endif
