#ifndef INC_DISPLAY_TASK_H_
#define INC_DISPLAY_TASK_H_

#include "main.h"
#include "ssd1306.h"

SSD1306_Status DisplayTask_Init(I2C_HandleTypeDef *hi2c);
void           DisplayTask_Create(void);
void DisplayTask_ShowMessage(const char *line1, const char *line2, uint32_t duration_ms);

#endif /* INC_DISPLAY_TASK_H_ */
