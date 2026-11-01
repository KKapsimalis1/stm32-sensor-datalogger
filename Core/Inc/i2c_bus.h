#ifndef INC_I2C_BUS_H_
#define INC_I2C_BUS_H_

#include "main.h"

/*
 * Thread-safe access to the shared I2C bus, with recovery from a stuck bus.
 *
 * The drivers (BME280, MPU6050, SSD1306) call these two functions instead of
 * HAL_I2C_Mem_Write/Read. A mutex lets only one task use the bus at a time. Every call is
 * a single short transaction, so a task never waits long. Before the scheduler starts
 * (system initialisation) the mutex is bypassed.
 * Call I2cBus_Init() from a task (not from main()): creating a mutex before the scheduler
 * runs leaves interrupts masked.
 *
 * Recovery: if a transfer fails with a timeout, a busy bus or a bus/arbitration error,
 * the bus is recovered before the mutex is released. This is the classic "slave holds SDA
 * low" lockup, for example after a reset in the middle of a transaction. The recovery
 * routine releases the I2C peripheral, clocks SCL up to 9 times until the slave lets go of
 * SDA, generates a STOP condition and initialises the peripheral again. A NACK (device not
 * present) is a normal error and does not trigger a recovery.
 *
 * The pins are PB8 (SCL) and PB9 (SDA). Define I2C_PORT, I2C_SCL_PIN, I2C_SDA_PIN and
 * I2C_GPIO_CLK_ENABLE() at the top of i2c_bus.c to use other pins (both on one port).
 */
void I2cBus_Init(void);

HAL_StatusTypeDef I2cBus_MemWrite(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                  uint16_t mem_addr, uint16_t mem_addr_size,
                                  uint8_t *data, uint16_t size, uint32_t timeout);

HAL_StatusTypeDef I2cBus_MemRead(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                 uint16_t mem_addr, uint16_t mem_addr_size,
                                 uint8_t *data, uint16_t size, uint32_t timeout);

/* Statistics, shown by the "health" command. */
uint32_t I2cBus_ErrorCount(void);      /* failed transfers, including NACKs */
uint32_t I2cBus_RecoveryCount(void);   /* how many times the bus was recovered */

#endif /* INC_I2C_BUS_H_ */
