#ifndef INC_COMMAND_MENU_H_
#define INC_COMMAND_MENU_H_

#include "stm32f4xx_hal.h"

void CommandMenu_Init(UART_HandleTypeDef *huart);
void CommandMenu_Process(void); // call from the main loop
void CommandMenu_Create(void);
#endif
