#ifndef INC_CONTROLS_H_
#define INC_CONTROLS_H_

#include "main.h"

/*
 * Actions bound to the B1 user button:
 *   short press: start a new recording (clears the buffer) / stop the recording
 *   long press : calibrate the gyroscope (only while not recording)
 * LD2 is on while the logger is recording.
 */
void Controls_Init(void);

/* Call from the main loop. */
void Controls_Process(void);

#endif /* INC_CONTROLS_H_ */
