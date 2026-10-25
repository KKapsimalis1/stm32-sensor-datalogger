#ifndef INC_BME280_H_
#define INC_BME280_H_

#include "main.h"

#define BME280_I2C_ADDR       (0x76 << 1)  /* 0x77 if SDO is tied to VDD */
#define BME280_REG_CHIP_ID    0xD0
#define BME280_CHIP_ID_VAL    0x60         /* BMP280 reports 0x58 */
#define BME280_REG_RESET      0xE0
#define BME280_REG_CTRL_HUM   0xF2
#define BME280_REG_CTRL_MEAS  0xF4
#define BME280_REG_CONFIG     0xF5
#define BME280_REG_DATA       0xF7         /* press(3) + temp(3) + hum(2) */
#define BME280_REG_CALIB_00   0x88         /* 0x88..0xA1, 26 bytes */
#define BME280_REG_CALIB_26   0xE1         /* 0xE1..0xE7, 7 bytes  */

typedef enum {
    BME280_OK = 0,
    BME280_ERR_NOT_FOUND,
    BME280_ERR_I2C
} BME280_Status;

/* Compensated measurements in fixed point (no floating point needed). */
typedef struct {
    int32_t  temp_centi;   /* 0.01 degC  (2508  = 25.08 degC) */
    uint32_t press_pa;     /* Pa         (101325 = 1013.25 hPa) */
    uint32_t hum_centi;    /* 0.01 %RH   (4512  = 45.12 %RH)  */
} BME280_Data;

BME280_Status BME280_Init(I2C_HandleTypeDef *hi2c);
BME280_Status BME280_ReadRaw(int32_t *raw_temp, int32_t *raw_press, int32_t *raw_hum);
BME280_Status BME280_Read(BME280_Data *out);

#endif /* INC_BME280_H_ */
