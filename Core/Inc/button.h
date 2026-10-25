#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#include "main.h"

#ifndef B1_Pin
#define B1_Pin        GPIO_PIN_13
#define B1_GPIO_Port  GPIOC
#endif

#define BUTTON_DEBOUNCE_MS    30     /* edges closer than this to the previous one are ignored */
#define BUTTON_LONG_PRESS_MS  1000   /* hold time that counts as a long press */

typedef enum {
    BUTTON_NONE = 0,
    BUTTON_SHORT,   /* pressed and released in less than BUTTON_LONG_PRESS_MS */
    BUTTON_LONG     /* held for at least BUTTON_LONG_PRESS_MS (reported while still held) */
} ButtonEvent;

/*
 * The B1 user button (PC13, active low) is handled by an EXTI interrupt on both edges.
 * The interrupt only timestamps the edges, so no press is lost while the main loop is busy
 * (display refresh, log dump, ...). Call Button_GetEvent() from the main loop.
 */
ButtonEvent Button_GetEvent(void);

#endif /* INC_BUTTON_H_ */
