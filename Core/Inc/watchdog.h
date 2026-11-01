#ifndef INC_WATCHDOG_H_
#define INC_WATCHDOG_H_

#include <stdint.h>

/*
 * Task watchdog on top of the independent watchdog of the MCU (IWDG).
 *
 * How it works:
 *   - Every monitored task calls Watchdog_Register() once when it starts and
 *     Watchdog_Checkin() on every pass through its main loop.
 *   - A low priority supervisor task wakes up every 500 ms. If every task checked in
 *     within its limit, it refreshes the IWDG. Otherwise it stops refreshing, remembers
 *     which tasks were silent, and the IWDG resets the MCU a few seconds later.
 *   - If a higher priority task spins forever, the supervisor never runs at all and the
 *     IWDG resets the MCU as well (the reset report then says "starved").
 *   - After the reset, Watchdog_Init() prints which task stalled.
 *
 * The IWDG itself is enabled in CubeMX (prescaler 32, reload 4095, about 4 s nominal;
 * the LSI oscillator varies from 17 to 47 kHz, so the real timeout is 2.8 to 7.7 s).
 */

typedef enum {
    WDG_TASK_SENSORS = 0,
    WDG_TASK_LOGGER,
    WDG_TASK_CONTROLS,
    WDG_TASK_CLI,
    WDG_TASK_DISPLAY,
    WDG_TASK_COUNT
} WdgTaskId;

/* Call once after the UART is initialised and just before the scheduler starts. Reads and
 * prints the reset cause, freezes the IWDG while the debugger halts the core, and
 * refreshes the IWDG once. */
void Watchdog_Init(void);

/* Create the supervisor task. Call it from the bootstrap task, after the other tasks. */
void Watchdog_Create(void);

/* Task side. Safe to call from any task. */
void Watchdog_Register(WdgTaskId id, uint32_t limit_ms);
void Watchdog_Checkin(WdgTaskId id);
void Watchdog_SetLimit(WdgTaskId id, uint32_t limit_ms);

/* Text such as "watchdog", "power-on" or "reset pin", for the "health" command. */
const char *Watchdog_ResetCause(void);

#endif /* INC_WATCHDOG_H_ */
