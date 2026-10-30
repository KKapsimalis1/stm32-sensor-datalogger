#ifndef INC_I2C_BUS_H_
#define INC_I2C_BUS_H_

#include "main.h"

/*
 * Thread-safe access to the shared I2C bus.
 *
 * The drivers (BME280, MPU6050, SSD1306) call these two functions instead of
 * HAL_I2C_Mem_Write/Read. A mutex (with priority inheritance) lets only one task use the
 * bus at a time. Every call is a single short transaction, so a task never waits long.
 * Before the scheduler starts (system initialisation) the mutex is bypassed.
 * Call I2cBus_Init() from a task (not from main()): creating a mutex before the scheduler
 * runs leaves interrupts masked.
 */
void I2cBus_Init(void);

HAL_StatusTypeDef I2cBus_MemWrite(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                  uint16_t mem_addr, uint16_t mem_addr_size,
                                  uint8_t *data, uint16_t size, uint32_t timeout);

HAL_StatusTypeDef I2cBus_MemRead(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                 uint16_t mem_addr, uint16_t mem_addr_size,
                                 uint8_t *data, uint16_t size, uint32_t timeout);

#endif /* INC_I2C_BUS_H_ */
