#include "i2c_bus.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define BUS_WAIT_MS  200  /* longest a task waits for the bus before giving up */

#ifndef I2C_PORT
#define I2C_PORT              GPIOB
#define I2C_SCL_PIN           GPIO_PIN_8
#define I2C_SDA_PIN           GPIO_PIN_9
#define I2C_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#endif

static SemaphoreHandle_t bus_mutex;
static volatile uint32_t error_count;
static volatile uint32_t recovery_count;

void I2cBus_Init(void)
{
    bus_mutex = xSemaphoreCreateMutex();
}

/* Busy-wait for a few microseconds using the DWT cycle counter. */
static void delay_us(uint32_t us)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000u);
    while ((uint32_t)(DWT->CYCCNT - start) < ticks) {
    }
}

/*
 * Free a bus that a slave is holding low. The caller holds the bus mutex (or the scheduler
 * is not running).
 */
static void bus_recover(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_I2C_DeInit(hi2c);          /* releases the pins and resets the peripheral state */
    I2C_GPIO_CLK_ENABLE();

    /* Both lines as open-drain outputs with the pull-ups, driven by software. */
    gpio.Pin   = I2C_SCL_PIN | I2C_SDA_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(I2C_PORT, &gpio);
    HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN | I2C_SDA_PIN, GPIO_PIN_SET);
    delay_us(10);

    /* A slave that lost count of the clocks releases SDA after at most 9 pulses. */
    for (int i = 0; i < 9 && HAL_GPIO_ReadPin(I2C_PORT, I2C_SDA_PIN) == GPIO_PIN_RESET; i++) {
        HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
        delay_us(5);
        HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
        delay_us(5);
    }

    /* START followed by STOP, so every slave sees a clean end of transaction. */
    HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_RESET);
    delay_us(5);
    HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
    delay_us(5);

    HAL_I2C_Init(hi2c);            /* the MSP init gives the pins back to the peripheral */
    recovery_count++;
}

static int needs_recovery(I2C_HandleTypeDef *hi2c, HAL_StatusTypeDef st)
{
    if (st == HAL_OK) {
        return 0;
    }
    uint32_t err = HAL_I2C_GetError(hi2c);
    return (st == HAL_BUSY) || (st == HAL_TIMEOUT) ||
           ((err & (HAL_I2C_ERROR_BERR | HAL_I2C_ERROR_ARLO | HAL_I2C_ERROR_TIMEOUT)) != 0);
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

/* Common end of a transfer: count errors, recover the bus if needed, release the mutex. */
static void bus_finish(I2C_HandleTypeDef *hi2c, HAL_StatusTypeDef st, int locked)
{
    if (st != HAL_OK) {
        error_count++;
        if (needs_recovery(hi2c, st)) {
            bus_recover(hi2c);
        }
    }
    if (locked > 0) {
        xSemaphoreGive(bus_mutex);
    }
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
    bus_finish(hi2c, st, locked);
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
    bus_finish(hi2c, st, locked);
    return st;
}

uint32_t I2cBus_ErrorCount(void)
{
    return error_count;
}

uint32_t I2cBus_RecoveryCount(void)
{
    return recovery_count;
}
