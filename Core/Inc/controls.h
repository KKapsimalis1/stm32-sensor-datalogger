#ifndef INC_CONTROLS_H_
#define INC_CONTROLS_H_

#include "main.h"

/*
 * Actions bound to the B1 user button:
 *   short press: start a new recording (clears the buffer) / stop the recording
 *   long press : calibrate the gyroscope (only while not recording)
 * LD2 is on while the logger is recording.
 *
 * Controls_Init() runs before the scheduler starts, Controls_Create() creates the task.
 * The button interrupt wakes the task through Controls_NotifyFromISR().
 */
void Controls_Init(void);
void Controls_Create(void);

/* Called from the EXTI interrupt on every accepted button edge. */
void Controls_NotifyFromISR(void);

#endif /* INC_CONTROLS_H_ */
