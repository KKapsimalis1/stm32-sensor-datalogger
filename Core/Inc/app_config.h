#ifndef INC_APP_CONFIG_H_
#define INC_APP_CONFIG_H_

/*
 * Task priorities (a higher number means a higher priority) and stack sizes in words
 * (1 word = 4 bytes). Keeping them in one file makes the scheduling design easy to see
 * and to tune. Use the "rtos" UART command to check how much stack each task really uses.
 */

/* Priorities: the sensor task has the tightest timing, the display the loosest. */
#define PRIO_SENSORS   4
#define PRIO_LOGGER    3
#define PRIO_CONTROLS  3
#define PRIO_CLI       2
#define PRIO_DISPLAY   1

/* Stack sizes in words. Generous at first, to be trimmed after measuring. */
#define STACK_SENSORS  384
#define STACK_LOGGER   256
#define STACK_CONTROLS 512
#define STACK_CLI      768
#define STACK_DISPLAY  512

#endif /* INC_APP_CONFIG_H_ */
