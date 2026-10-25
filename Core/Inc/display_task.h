#ifndef INC_DISPLAY_TASK_H_
#define INC_DISPLAY_TASK_H_

#include "main.h"
#include "ssd1306.h"

/* Initialises the OLED. Call after BME280_Init() and MPU6050_Init(). */
SSD1306_Status DisplayTask_Init(I2C_HandleTypeDef *hi2c);

/* Call from the main loop. Refreshes the screen once per second, without blocking in between. */
void DisplayTask_Process(void);

#endif /* INC_DISPLAY_TASK_H_ */
