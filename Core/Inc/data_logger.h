#ifndef INC_DATA_LOGGER_H_
#define INC_DATA_LOGGER_H_

#include "main.h"

#define DATALOGGER_CAPACITY    1024   /* records; 28 bytes each, about 28 KB of RAM */
#define DATALOGGER_DEFAULT_MS  1000
#define DATALOGGER_MIN_MS      20

/* One sample of every sensor, stored in fixed point (same units as the sensor drivers). */
typedef struct {
    uint32_t timestamp_ms;   /* time since boot (HAL_GetTick) */
    int32_t  temp_centi;     /* 0.01 degC */
    uint32_t press_pa;       /* Pa */
    uint32_t hum_centi;      /* 0.01 %RH */
    int16_t  accel_mg[3];    /* milli-g, x y z */
    int16_t  gyro_cdps[3];   /* 0.01 deg/s, x y z */
} LogRecord;

/*
 * The logger is a consumer: the sensor task pushes samples into a queue, and the logger
 * task stores them in a circular buffer while recording is active.
 * DataLogger_Init() runs before the scheduler starts, DataLogger_Create() creates the task.
 */
void     DataLogger_Init(void);
void     DataLogger_Create(void);

/* Start/Stop/Clear/Dump may be called from any task. Start also sets the sensor period. */
void     DataLogger_Start(uint32_t period_ms);
void     DataLogger_Stop(void);
void     DataLogger_Clear(void);

/*
 * Prints all stored records, oldest first, as CSV over printf (UART).
 * New samples are not stored while the dump is running (they are counted as dropped).
 */
void     DataLogger_Dump(void);

uint8_t  DataLogger_IsRunning(void);
uint32_t DataLogger_Count(void);
uint32_t DataLogger_Period(void);
uint32_t DataLogger_ErrorCount(void);     /* samples discarded because a sensor read failed */
uint32_t DataLogger_DroppedCount(void);   /* samples missed during a dump */

#endif /* INC_DATA_LOGGER_H_ */
