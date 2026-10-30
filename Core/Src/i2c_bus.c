#include "i2c_bus.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define BUS_WAIT_MS  200  /* longest a task waits for the bus before giving up */

static SemaphoreHandle_t bus_mutex;

void I2cBus_Init(void)
{
    bus_mutex = xSemaphoreCreateMutex();
}

/* Returns 1 if the mutex was taken, 0 if it is not needed, -1 on timeout. */
static int bus_lock(void)
{
    if (bus_mutex == NULL || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return 0;
    }
    if (xSemaphoreTake(bus_mutex, pdMS_TO_TICKS(BUS_WAIT_MS)) != pdTRUE) {
        return -1;
    }
    return 1;
}

HAL_StatusTypeDef I2cBus_MemWrite(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                  uint16_t mem_addr, uint16_t mem_addr_size,
                                  uint8_t *data, uint16_t size, uint32_t timeout)
{
    int locked = bus_lock();
    if (locked < 0) {
        return HAL_TIMEOUT;
    }
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(hi2c, dev_addr, mem_addr, mem_addr_size,
                                             data, size, timeout);
    if (locked > 0) {
        xSemaphoreGive(bus_mutex);
    }
    return st;
}

HAL_StatusTypeDef I2cBus_MemRead(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                 uint16_t mem_addr, uint16_t mem_addr_size,
                                 uint8_t *data, uint16_t size, uint32_t timeout)
{
    int locked = bus_lock();
    if (locked < 0) {
        return HAL_TIMEOUT;
    }
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(hi2c, dev_addr, mem_addr, mem_addr_size,
                                            data, size, timeout);
    if (locked > 0) {
        xSemaphoreGive(bus_mutex);
    }
    return st;
}
